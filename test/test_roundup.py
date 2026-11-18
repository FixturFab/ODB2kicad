"""Tests for the ODB++ round-up harness. Made-up boards only; no converter needed
except the last test (skipped when odb2kicad/build/odb2kicad is not built).

    python3 -m unittest discover -s test -p 'test_roundup.py' -v
"""

from __future__ import annotations

import json
import math
import os
import socket
import subprocess
import sys
import tempfile
import threading
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import odb_render_text  # noqa: E402
import raster  # noqa: E402
from raster import _popcount  # noqa: E402
import roundup  # noqa: E402
import synthetic_odb  # noqa: E402
from kicad_render import KicadBoard  # noqa: E402
from odb_render import OdbJob, map_layers, standard_symbol  # noqa: E402
from valor_oracle import parse_feature_geometry  # noqa: E402

BIN = HERE.parent / "odb2kicad" / "build" / "odb2kicad"


class Raster(unittest.TestCase):
    def test_square_exact(self):
        b = raster.Bitmap(20, 20)
        b.fill([[(2, 2), (12, 2), (12, 7), (2, 7)]])
        self.assertEqual(b.count(), 50)

    def test_even_odd_hole_and_clear(self):
        b = raster.Bitmap(20, 20)
        b.fill([[(0, 0), (20, 0), (20, 20), (0, 20)], [(5, 5), (15, 5), (15, 15), (5, 15)]])
        self.assertEqual(b.count(), 400 - 100)
        b.fill([[(0, 0), (20, 0), (20, 10), (0, 10)]], 0)
        self.assertEqual(b.count(), 150)

    def test_compare_tolerance(self):
        a, c = raster.Bitmap(30, 30), raster.Bitmap(30, 30)
        a.fill([[(5, 5), (20, 5), (20, 20), (5, 20)]])
        c.fill([[(6, 5), (21, 5), (21, 20), (6, 20)]])  # shifted by 1 px
        m = raster.compare(a, c, tol_px=1)
        self.assertGreater(m["xor_ratio"], 0)
        self.assertEqual(m["missing_px"], 0)
        self.assertEqual(m["extra_px"], 0)
        self.assertGreater(raster.compare(a, c, tol_px=0)["missing_px"], 0)

    def test_png_and_shrink(self):
        b = raster.Bitmap(10, 6)
        b.fill([[(1, 1), (5, 1), (5, 4), (1, 4)]])
        m, w, h = raster.shrink(b.as_int(), 10, 6, 2)
        self.assertEqual((w, h, bin(m).count("1")), (5, 3, 6))
        with tempfile.TemporaryDirectory() as d:
            raster.ink_image(b, (255, 0, 0)).save_png(Path(d) / "x.png")
            self.assertTrue((Path(d) / "x.png").read_bytes().startswith(b"\x89PNG"))


class Symbols(unittest.TestCase):
    def area(self, ops, ppm=200.0):
        v = raster.View(-3, 3, 6, 6, ppm, True)
        b = raster.Bitmap(v.w, v.h)
        for val, cs in ops:
            b.fill(v.px(cs), val)
        return b.count() / ppm ** 2

    def test_areas(self):
        self.assertAlmostEqual(self.area(standard_symbol("r1000", 0.001, 200)), math.pi * 0.25, delta=0.01)
        self.assertAlmostEqual(self.area(standard_symbol("rect2000x1000", 0.001, 200)), 2.0, delta=0.01)
        self.assertAlmostEqual(self.area(standard_symbol("donut_r2000x1000", 0.001, 200)), math.pi * 0.75, delta=0.02)
        rr = self.area(standard_symbol("rect2000x1000xr250", 0.001, 200))
        self.assertAlmostEqual(rr, 2.0 - (4 - math.pi) * 0.0625, delta=0.01)
        self.assertAlmostEqual(self.area(standard_symbol("r40", 0.0254, 200)), math.pi * (0.508 ** 2), delta=0.01)  # mils
        self.assertLess(self.area(standard_symbol("thr2000x1400x45x4x300", 0.001, 200)), math.pi * (1 - 0.49))
        self.assertIsNone(standard_symbol("special_thing", 0.001, 200))
        self.assertEqual(standard_symbol("null5", 0.001, 200), [])
        self.assertIsNotNone(standard_symbol("donut_rc10520.0x5670.0x120.0xr60.0", 0.001, 200))

    def test_spec_shapes(self):
        """ODB++ 8.1 Appendix A shapes, areas worked by hand (the converter's unit tests use the same)."""
        pi = math.pi

        def a(name):
            return self.area(standard_symbol(name, 0.001, 400), 400)
        d = math.asin(0.3 / 0.85)  # thr: pieces stop where their round end clears the gap
        self.assertAlmostEqual(a("thr2000x1400x45x4x300"), 4 * ((pi / 2 - 2 * d) * 0.85 * 0.3 + pi * 0.0225), delta=0.006)
        self.assertAlmostEqual(a("oval_h2000x1000"), 1.5 + pi * 0.25 / 2, delta=0.005)
        self.assertAlmostEqual(a("s_tho2000x1400x45x4x300"), 4 * 0.8 * 0.3, delta=0.005)
        self.assertAlmostEqual(a("rc_tho3000x2000x45x4x300x300"), 2 * 1.8 * 0.3 + 2 * 0.8 * 0.3, delta=0.005)
        moire = pi * (0.0625 - 0.0225) + pi * (0.25 - 0.16) + 0.1 - 0.0025 - 0.04
        self.assertAlmostEqual(a("moire100x200x2x50x1000x0"), moire, delta=0.006)
        for name, want in (("hplate2000x1000x500", 1.75), ("rhplate2000x1000x500", 1.75), ("fhplate2000x1000x200x500", 1.9),
                           ("radhplate2000x1000x400", 2 - pi * 0.02), ("dshape2000x2000x1000", 2 + pi / 2),
                           ("cross3000x2000x400x600x50x50xs", 2.16), ("dogbone3000x2000x400x600x50xs", 3.12),
                           ("dpack1000x1000x100x100x3x3", 0.64)):
            self.assertAlmostEqual(a(name), want, delta=0.006, msg=name)
        # rc_ths at 45 deg: the gap runs through the rectangle's corner
        ops = standard_symbol("rc_ths3000x2000x45x4x300x300", 0.001, 400)
        v = raster.View(-3, 3, 6, 6, 400, True)
        b = raster.Bitmap(v.w, v.h)
        for val, cs in ops:
            b.fill(v.px(cs), val)
        corner = raster.Bitmap(v.w, v.h)
        corner.fill(v.px([[(1.25, 0.75), (1.45, 0.75), (1.45, 0.95), (1.25, 0.95)]]), 1)
        self.assertLess(_popcount(b.as_int() & corner.as_int()), 0.2 * corner.count())
        # every name the made-up symbols board uses is drawn
        for name in synthetic_odb.SYMBOL_NAMES:
            self.assertTrue(standard_symbol(name, 0.001, 50), name)


class KicadPadstacks(unittest.TestCase):
    """KiCad 9 boards: layer ids 0 / 2 / 4.. are not the stack order; padstacks size per layer"""

    BOARD = """(kicad_pcb (version 20241229) (generator test)
  (layers (0 "F.Cu" signal) (4 "In1.Cu" signal) (6 "In2.Cu" signal) (2 "B.Cu" signal) (25 "Edge.Cuts" user))
  (via (at 0 0) (size 0.8) (drill 0.4) (layers "F.Cu" "B.Cu")
    (padstack (mode custom) (layer "In1.Cu" (size 0.6)) (layer "In2.Cu" (size 0.4)) (layer "B.Cu" (size 0.6))) (net 0))
  (via (at 5 0) (size 1.0) (drill 0.4) (layers "F.Cu" "B.Cu")
    (padstack (mode front_inner_back) (layer "Inner" (size 0.5)) (layer "B.Cu" (size 1.0))) (net 0))
  (footprint "x" (layer "F.Cu") (at 10 0)
    (pad "1" thru_hole rect (at 0 0) (size 2 2) (layers "*.Cu") (drill 1)
      (padstack (mode front_inner_back) (layer "Inner" (shape circle) (size 1.5 1.5)) (layer "B.Cu" (shape rect) (size 2 2))))))
"""

    def area(self, board, layer, x0, x1, ppm=50.0):
        view = raster.View(x0, -2, x1 - x0, 4, ppm, False)
        return board.render(layer, view).count() / ppm ** 2

    def test_copper_order_and_per_layer_sizes(self):
        b = KicadBoard(self.BOARD)
        self.assertEqual(b.copper, ["F.Cu", "In1.Cu", "In2.Cu", "B.Cu"])
        for layer, d in (("F.Cu", 0.8), ("In1.Cu", 0.6), ("In2.Cu", 0.4), ("B.Cu", 0.6)):
            self.assertAlmostEqual(self.area(b, layer, -2, 2), math.pi * d * d / 4, delta=0.03, msg=layer)
        for layer, d in (("F.Cu", 1.0), ("In1.Cu", 0.5), ("In2.Cu", 0.5), ("B.Cu", 1.0)):
            self.assertAlmostEqual(self.area(b, layer, 3, 7), math.pi * d * d / 4, delta=0.03, msg=layer)
        self.assertAlmostEqual(self.area(b, "F.Cu", 8, 12), 4.0, delta=0.05)
        self.assertAlmostEqual(self.area(b, "B.Cu", 8, 12), 4.0, delta=0.05)
        self.assertAlmostEqual(self.area(b, "In1.Cu", 8, 12), math.pi * 1.5 * 1.5 / 4, delta=0.05)

    def test_unused_via_layers_have_no_pad(self):
        """remove_unused_layers: only the forced layers and the kept ends are flashed"""
        b = KicadBoard("""(kicad_pcb (version 20241229) (generator test)
  (layers (0 "F.Cu" signal) (4 "In1.Cu" signal) (6 "In2.Cu" signal) (2 "B.Cu" signal) (25 "Edge.Cuts" user))
  (via (at 0 0) (size 0.8) (drill 0.4) (layers "F.Cu" "B.Cu")
    (padstack (mode custom) (layer "In1.Cu" (size 0.6)) (layer "In2.Cu" (size 0.6)) (layer "B.Cu" (size 0.6)))
    (remove_unused_layers yes) (keep_end_layers yes) (zone_layer_connections "In1.Cu") (net 0)))
""")
        for layer, d in (("F.Cu", 0.8), ("In1.Cu", 0.6), ("In2.Cu", 0.0), ("B.Cu", 0.6)):
            self.assertAlmostEqual(self.area(b, layer, -2, 2), math.pi * d * d / 4, delta=0.03, msg=layer)


class Odb(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.root = Path(cls.tmp.name)
        cls.basic = synthetic_odb.write_job(cls.root / "basic", synthetic_odb.BASIC, synthetic_odb.PROFILE_IN, synthetic_odb.USER_SYMBOL)
        cls.edge = synthetic_odb.write_job(cls.root / "edge", synthetic_odb.EDGE, synthetic_odb.PROFILE_MM, units="MM")

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_parse_records(self):
        g = parse_feature_geometry(self.basic / "steps/pcb/layers/gnd/features")
        self.assertEqual(g["units"], "INCH")
        s = [r for r in g["records"] if r[0] == "S"][0]
        self.assertEqual([h for h, _ in s[2]], [False, True])
        p = [r for r in g["records"] if r[0] == "P"][0]
        self.assertFalse(p[4])  # negative
        g = parse_feature_geometry(self.edge / "steps/pcb/layers/top/features")
        self.assertEqual(g["symbols"][5], ("rect50x30", "I"))

    def test_layer_map(self):
        m = {L["odb"]: L["kicad"] for L in map_layers(OdbJob(self.basic).matrix)}
        self.assertEqual(m["TOP"], "F.Cu")
        self.assertEqual(m["GND"], "In1.Cu")
        self.assertEqual(m["SIG3"], "In2.Cu")
        self.assertEqual(m["BOTTOM"], "B.Cu")
        self.assertEqual(m["F.MASK"], "F.Mask")
        self.assertEqual(m["B.MASK"], "B.Mask")
        self.assertEqual(m["DRILL"], "Drill")
        self.assertNotIn("NOTES", m)

    def test_negative_clears_and_user_symbol(self):
        job = OdbJob(self.basic)
        v = raster.View(-1, 32, 53, 34, 10.0, True)
        gnd, _ = job.render("In1.Cu", v)
        # the plane minus its hole minus the negative pad
        plane = (1.8 * 1.0 - 0.4 * 0.2) * 25.4 ** 2
        neg = math.pi * (0.02 * 25.4) ** 2
        self.assertAlmostEqual(gnd.count() / 100.0, plane - neg, delta=plane * 0.01)
        top, _ = job.render("F.Cu", v)
        self.assertGreater(top.count(), 0)
        self.assertEqual(job.stats["unsupported_symbols"], {})

    def test_negative_layer_is_dark_over_the_profile(self):
        """A NEGATIVE matrix layer is the profile less its positive features."""
        neg = synthetic_odb.write_job(self.root / "negative", synthetic_odb.NEGATIVE, synthetic_odb.PROFILE_NEG,
                                      units="MM", negative=("SIG3",))
        job = OdbJob(neg)
        self.assertTrue({L["odb"]: L.get("negative") for L in job.layers}["SIG3"])
        v = raster.View(-1, 31, 42, 32, 10.0, True)
        sig3, _ = job.render("In2.Cu", v)
        board = 40 * 30
        self.assertLess(sig3.count() / 100.0, board - 20)
        self.assertGreater(sig3.count() / 100.0, board - 60)

    def test_renderers_agree_on_equivalent_board(self):
        """The ODB++ reference and a hand-made KiCad twin of its positive TOP / silk / outline match."""
        with tempfile.TemporaryDirectory() as d:
            pcb = Path(d) / "twin.kicad_pcb"
            pcb.write_text(synthetic_odb.kicad_equivalent())
            job = self.root / "twin"
            layers = dict(synthetic_odb.BASIC)
            layers["TOP"] = ("INCH", layers["TOP"][1], layers["TOP"][2][:-1])  # drop the user-symbol pad
            synthetic_odb.write_job(job, layers, synthetic_odb.PROFILE_IN)
            rep = roundup.compare_job(job, {"twin": {"pcb": pcb}}, Path(d) / "out", images=False, siemens=False,
                                      structural_diff=False)
        by = {L["kicad"]: L["by"]["twin"] for L in rep["layers"]}
        for name in ("F.Cu", "F.SilkS", "Edge.Cuts"):
            self.assertLess(by[name]["xor_ratio"], 0.01, name)
            self.assertEqual(by[name]["missing_px"], 0, name)
            self.assertEqual(by[name]["extra_px"], 0, name)
        self.assertTrue(rep["boards"]["twin"]["align"]["edge"])
        self.assertEqual(rep["boards"]["twin"]["align"]["dw_mm"], 0.0)

    def test_metrics_only_is_numbers(self):
        with tempfile.TemporaryDirectory() as d:
            pcb = Path(d) / "twin.kicad_pcb"
            pcb.write_text(synthetic_odb.kicad_equivalent())
            rep = roundup.compare_job(self.basic, {"a": {"pcb": pcb}, "b": {"pcb": None}}, Path(d) / "o",
                                      images=True, siemens=False, structural_diff=False)
            self.assertTrue((Path(d) / "o" / "sheet.png").is_file())
            roundup.write_index(Path(d), [rep])
            self.assertIn("Download verdicts.json", (Path(d) / "index.html").read_text())
        m = roundup.metrics_only(rep)

        def leaves(x):
            if isinstance(x, dict):
                for k, v in x.items():
                    self.assertRegex(k, r"^[a-z_0-9]+$")
                    yield from leaves(v)
            else:
                yield x
        for v in leaves(m):
            self.assertTrue(v is None or isinstance(v, (int, float, bool)), v)
        self.assertNotIn("b", m["boards"]["a"])

    def test_siemens_skips_cleanly(self):
        env = os.environ.pop("ODBVIEWER_HOME", None)
        try:
            self.assertIn("skipped", roundup.siemens_column(OdbJob(self.basic), ["TOP"]))
        finally:
            if env is not None:
                os.environ["ODBVIEWER_HOME"] = env

    def test_siemens_fake_server(self):
        """ValorClient's COM round trip against a stand-in server.pl that writes csh info files."""
        srv = socket.socket()
        srv.bind(("127.0.0.1", 0))
        srv.listen(4)
        port = srv.getsockname()[1]

        def serve():
            while True:
                try:
                    c, _ = srv.accept()
                except OSError:
                    return
                serve_one(c)

        def serve_one(c):
            buf = b""
            while True:
                data = c.recv(4096)
                if not data:
                    break
                buf += data
                while b"\n" in buf:
                    line, buf = buf.split(b"\n", 1)
                    cmd = line.decode()
                    if "out_file=" in cmd:
                        path = cmd.split("out_file=")[1].split(",")[0].strip()
                        body = "set gLIMITSxmin = '0'\nset gLIMITSxmax = '50.8'\n" if "LIMITS" in cmd else \
                            "set gSYMS_HISTsymbol = ('r20' 'r8')\nset gSYMS_HISTpad = ('1' '0')\n"
                        Path(path).write_text(body)
                    c.sendall(b"0\nOK\n")
            c.close()
        t = threading.Thread(target=serve, daemon=True)
        t.start()
        with tempfile.TemporaryDirectory() as d:
            old = {k: os.environ.get(k) for k in ("ODBVIEWER_HOME", "ODBVIEWER_PORT", "ODBVIEWER_TMP", "DISPLAY")}
            os.environ.update({"ODBVIEWER_HOME": d, "ODBVIEWER_PORT": str(port), "ODBVIEWER_TMP": d, "DISPLAY": ":99"})
            try:
                col = roundup.siemens_column(OdbJob(self.basic), ["TOP"])
            finally:
                for k, v in old.items():
                    if v is None:
                        os.environ.pop(k, None)
                    else:
                        os.environ[k] = v
        srv.close()
        self.assertEqual(col["layers"]["TOP"]["limits"]["gLIMITSxmax"], "50.8")
        self.assertEqual(col["layers"]["TOP"]["syms_hist"]["gSYMS_HISTsymbol"], ["r20", "r8"])

    @unittest.skipUnless(BIN.is_file(), "odb2kicad not built")
    def test_cli_on_sample(self):
        with tempfile.TemporaryDirectory() as d:
            rc = roundup.main([str(HERE.parent / "samples" / "odb-output"), "--out", d, "--no-siemens"])
            self.assertEqual(rc, 0)
            rep = json.loads((Path(d) / "odb-output" / "report.json").read_text())
            fcu = [L for L in rep["layers"] if L["kicad"] == "F.Cu"][0]["by"]["odb2kicad"]
            self.assertLess(fcu["xor_ratio"], 0.01)
            self.assertTrue((Path(d) / "index.html").is_file())

    @unittest.skipUnless(BIN.is_file(), "odb2kicad not built")
    def test_step_origin_moves_the_board(self):
        """stephdr X/Y_ORIGIN: the converter and the reference both draw the step moved by it."""
        with tempfile.TemporaryDirectory() as d:
            job = synthetic_odb.write_job(Path(d) / "in" / "synthetic-origin", synthetic_odb.BASIC, synthetic_odb.PROFILE_IN,
                                          synthetic_odb.USER_SYMBOL, origin=synthetic_odb.ORIGIN_IN)
            ox, oy = (v * 25.4 for v in synthetic_odb.ORIGIN_IN)
            pb = OdbJob(job).profile_bbox()
            self.assertAlmostEqual(pb[0], ox, places=6)
            self.assertAlmostEqual(pb[1], oy, places=6)
            rc = roundup.main([str(job), "--out", str(Path(d) / "out"), "--no-siemens", "--no-images"])
            self.assertEqual(rc, 0)
            rep = json.loads((Path(d) / "out" / "synthetic-origin" / "report.json").read_text())
            al = rep["boards"]["odb2kicad"]["align"]
            self.assertLess(abs(al["dx_mm"]), 0.01)  # the Edge.Cuts bbox is where the moved profile is
            self.assertLess(abs(al["dy_mm"]), 0.01)
            for L in rep["layers"]:
                if L["kicad"] in ("F.Cu", "B.Cu", "In2.Cu", "F.SilkS", "Edge.Cuts"):
                    self.assertLess(L["by"]["odb2kicad"]["xor_ratio"], 0.02, L["kicad"])

    @unittest.skipUnless(BIN.is_file(), "odb2kicad not built")
    def test_attrlist_thickness_and_output_transform(self):
        """Layer attrlists: the stackup gives the board thickness, .out_* moves that layer only."""
        with tempfile.TemporaryDirectory() as d:
            job = synthetic_odb.write_job(Path(d) / "in" / "synthetic-stackup", synthetic_odb.BASIC, synthetic_odb.PROFILE_IN,
                                          synthetic_odb.USER_SYMBOL, attrlists=synthetic_odb.STACKUP_ATTRS)
            rc = roundup.main([str(job), "--out", str(Path(d) / "out"), "--no-siemens", "--no-images"])
            self.assertEqual(rc, 0)
            pcb = (Path(d) / "out" / "synthetic-stackup" / "odb2kicad.kicad_pcb")
            if not pcb.is_file():
                pcb = next((Path(d) / "out").rglob("*.kicad_pcb"))
            # (0.008 + 0.04 + 0.008) in + 3 oz of copper + 0.0006 in of mask = 1.4224 + 0.1044 + 0.01524
            self.assertIn("(thickness 1.542)", pcb.read_text())
            rep = json.loads((Path(d) / "out" / "synthetic-stackup" / "report.json").read_text())
            by = {L["kicad"]: L["by"]["odb2kicad"] for L in rep["layers"]}
            self.assertGreater(by["In2.Cu"]["got_px"], 0)   # SIG3, drawn at half scale
            for name in ("F.Cu", "In2.Cu", "B.Cu"):
                self.assertLess(by[name]["xor_ratio"], 0.03, name)

    @unittest.skipUnless(BIN.is_file(), "odb2kicad not built")
    def test_multi_step_job(self):
        """Two steps: the first (by COL) goes to the output file, the others beside it as
        <job>__<step>.kicad_pcb; the panel step (no layer data, a STEP-REPEAT of PCB) is one board
        holding two copies of PCB; --step picks one; the reference compares the same first step."""
        with tempfile.TemporaryDirectory() as d:
            job = synthetic_odb.write_two_step_job(Path(d) / "in" / "synthetic-steps")
            single = synthetic_odb.write_job(Path(d) / "in" / "single", synthetic_odb.BASIC, synthetic_odb.PROFILE_IN,
                                             synthetic_odb.USER_SYMBOL)
            self.assertEqual(OdbJob(job).step, "pcb")
            out = Path(d) / "cli"
            out.mkdir()
            run = subprocess.run([str(BIN), str(job), str(out / "board.kicad_pcb")], capture_output=True, text=True)
            self.assertEqual(run.returncode, 0, run.stderr)
            self.assertEqual(sorted(p.name for p in out.iterdir()),
                             ["board.kicad_pcb", "two_step_job__FLEX.kicad_pcb", "two_step_job__PANEL.kicad_pcb",
                              "two_step_job__scrap.kicad_pcb"])
            self.assertNotIn("was not converted", run.stderr)
            first = (out / "board.kicad_pcb").read_text()
            panel = (out / "two_step_job__PANEL.kicad_pcb").read_text()
            # PCB at (0.5, 0.5) in and (3.0, 0.5) in: every footprint twice, 63.5 mm apart; the
            # panel's profile is the board edge, the copies' outlines are drawings
            self.assertEqual(panel.count("(footprint "), 2 * first.count("(footprint "))
            self.assertEqual(panel.count('(layer "Edge.Cuts")'), 1)
            self.assertEqual(panel.count('(layer "Dwgs.User")'), 2)
            self.assertIn('(gr_rect (start 12.7 -43.18) (end 63.5 -12.7)', panel)
            self.assertIn('(gr_rect (start 76.2 -43.18) (end 127 -12.7)', panel)
            # the first step is the basic board, byte for byte what it is as a one-step job
            alone = subprocess.run([str(BIN), str(single)], capture_output=True, text=True)
            self.assertEqual(first, alone.stdout)
            flex = subprocess.run([str(BIN), "--step", "flex", str(job)], capture_output=True, text=True)
            self.assertEqual(flex.returncode, 0, flex.stderr)
            self.assertEqual(flex.stdout, (out / "two_step_job__FLEX.kicad_pcb").read_text())
            self.assertNotEqual(flex.stdout, first)
            to_stdout = subprocess.run([str(BIN), str(job)], capture_output=True, text=True)
            self.assertEqual(to_stdout.stdout, first)
            self.assertIn("the job has 4 steps", to_stdout.stderr)
            missing = subprocess.run([str(BIN), "--step", "nope", str(job)], capture_output=True, text=True)
            self.assertEqual(missing.returncode, 1)
            self.assertIn("no-step", missing.stderr)
            rc = roundup.main([str(job), "--out", str(Path(d) / "out"), "--no-siemens", "--no-images"])
            self.assertEqual(rc, 0)
            rep = json.loads((Path(d) / "out" / "synthetic-steps" / "report.json").read_text())
            self.assertEqual(rep["step"], "pcb")
            fcu = [L for L in rep["layers"] if L["kicad"] == "F.Cu"][0]["by"]["odb2kicad"]
            self.assertLess(fcu["xor_ratio"], 0.02)

    def test_text_is_drawn_from_the_font(self):
        """T records: the reference strokes each character from fonts/<name>."""
        with tempfile.TemporaryDirectory() as d:
            job = synthetic_odb.write_job(Path(d) / "synthetic-text", synthetic_odb.TEXT, synthetic_odb.PROFILE_IN,
                                          synthetic_odb.USER_SYMBOL, font=synthetic_odb.TEXT_FONT)
            geo = parse_feature_geometry(job / "steps" / "pcb" / "layers" / "notes" / "features")
            t = [r for r in geo["records"] if r[0] == "T"]
            self.assertEqual(len(t), 2)
            self.assertEqual(t[0][1:], (0.2, 1.0, True, "standard", False, 0.0, 0.15, 0.15, 2.0, "EXIT", False))
            self.assertEqual((t[1][5], t[1][6], t[1][10]), (True, 30.0, "LET"))
            oj = OdbJob(job)
            self.assertIn(("NOTES", "User.1"), [(L["odb"], L["kicad"]) for L in oj.layers])
            strokes, polys = oj._text_strokes(t[0], 25.4)
            self.assertEqual((len(strokes), polys), (4 + 2 + 1 + 2, []))  # E X I T
            # E's first stroke: its left edge (x = -0.1 in the font), from the foot (-0.1) to the
            # top (0.2), scaled by 0.15 / 0.302, the glyph's own zero on the text's location
            k = 0.15 / 0.302
            x1, y1, x2, y2, w, positive, rnd = strokes[0]
            self.assertAlmostEqual(x1, (0.2 - 0.1 * k) * 25.4, places=6)
            self.assertAlmostEqual(y1, (1.0 - 0.1 * k) * 25.4, places=6)
            self.assertAlmostEqual(y2, (1.0 + 0.2 * k) * 25.4, places=6)
            self.assertAlmostEqual(w, 2 * 0.012 * 25.4, places=6)
            self.assertTrue(positive and rnd)
            # the second glyph is one xsize on: X's first stroke starts at its lower left
            self.assertAlmostEqual(strokes[4][0], (0.2 + 0.15 - 0.1 * k) * 25.4, places=6)
            v = raster.View(0, 30.48, 50.8, 30.48, 20.0, True)
            with_text = oj.render("F.SilkS", v)[0].count()
            plain = OdbJob(self.basic).render("F.SilkS", v)[0].count()
            self.assertGreater(with_text, plain + 200)
            self.assertEqual(oj.stats["text"], 2)

    @unittest.skipUnless(BIN.is_file(), "odb2kicad not built")
    def test_text_converter_agrees_with_the_reference(self):
        with tempfile.TemporaryDirectory() as d:
            job = synthetic_odb.write_job(Path(d) / "in" / "synthetic-text", synthetic_odb.TEXT, synthetic_odb.PROFILE_IN,
                                          synthetic_odb.USER_SYMBOL, font=synthetic_odb.TEXT_FONT)
            rc = roundup.main([str(job), "--out", str(Path(d) / "out"), "--no-siemens", "--no-images"])
            self.assertEqual(rc, 0)
            rep = json.loads((Path(d) / "out" / "synthetic-text" / "report.json").read_text())
            by = {L["kicad"]: L for L in rep["layers"]}
            for name in ("F.SilkS", "User.1", "In1.Cu"):   # silk text, document-layer text, negative text in a plane
                self.assertGreater(by[name]["ref_px"], 0, name)
                self.assertLess(by[name]["by"]["odb2kicad"]["xor_ratio"], 0.03, name)
            pcb = next((Path(d) / "out").rglob("*.kicad_pcb")).read_text()
            self.assertIn('(50 "User.1" user "NOTES")', pcb)

    def test_spec_v2_font_and_arcs(self):
        """Reader spec v2: a font's own UNITS, POLYGON glyphs and square line ends; V text runs
        up; an arc without its direction token runs counter-clockwise."""
        with tempfile.TemporaryDirectory() as d:
            job = synthetic_odb.write_job(Path(d) / "synthetic-specv2", synthetic_odb.SPECV2, synthetic_odb.PROFILE_IN,
                                          synthetic_odb.USER_SYMBOL, font=synthetic_odb.FONT_V2, font_name="v2font")
            font = odb_render_text.parse_font(job / "fonts" / "v2font")
            self.assertEqual((font["units"], font["scale"], font["xsize"], font["ysize"], font["bad_lines"]), ("MM", 1.0, 4.0, 6.0, []))
            self.assertEqual(font["chars"]["I"]["lines"][0][5], False)          # S: square ends
            self.assertEqual([len(p) for p in font["chars"]["O"]["polys"]], [4])
            self.assertEqual([len(p) for p in font["chars"]["P"]["polys"]], [4])  # two points a line
            self.assertEqual(len(font["chars"]["P"]["lines"]), 1)
            geo = parse_feature_geometry(job / "steps" / "pcb" / "layers" / "f.silk" / "features")
            arcs = [r for r in geo["records"] if r[0] == "A"]
            self.assertEqual([(r[8], r[9]) for r in arcs], [(True, False), (True, True), (True, False)])
            oj = OdbJob(job)
            v = [r for r in geo["records"] if r[0] == "T" and r[11]][0]
            strokes, polys = oj._text_strokes(v, 25.4)
            # 'IO' upwards from (1.6, 0.2) in: I's stroke at x + 1/4 box, 2 x 0.3 mm wide; O one ysize up
            self.assertAlmostEqual(strokes[0][0], (1.6 + 0.1 / 4) * 25.4, places=6)
            self.assertAlmostEqual(strokes[0][4], 0.6, places=9)
            self.assertFalse(strokes[0][6])
            self.assertAlmostEqual(min(y for _x, y in polys[0][1]), (0.2 + 0.15) * 25.4, places=6)
            self.assertAlmostEqual(max(y for _x, y in polys[0][1]), (0.2 + 0.15 + 0.15 * 5 / 6) * 25.4, places=6)
            # the arcs: without a token the half circle from (0.5, 0.2) to (0.3, 0.2) goes over the top
            view = raster.View(0, 30.48, 50.8, 30.48, 20.0, True)
            bm = oj.render("F.SilkS", view)[0]
            def ink(x_in, y_in):
                px, py = int(x_in * 25.4 * 20), int((30.48 - y_in * 25.4) * 20)
                return bm.buf[py * bm.w + px]
            self.assertEqual((ink(0.4, 0.3), ink(0.4, 0.1)), (1, 0))   # no token: counter-clockwise
            self.assertEqual((ink(0.8, 0.3), ink(0.8, 0.1)), (0, 1))   # Y: clockwise

    @unittest.skipUnless(BIN.is_file(), "odb2kicad not built")
    def test_spec_v2_converter_agrees_with_the_reference(self):
        with tempfile.TemporaryDirectory() as d:
            job = synthetic_odb.write_job(Path(d) / "in" / "synthetic-specv2", synthetic_odb.SPECV2, synthetic_odb.PROFILE_IN,
                                          synthetic_odb.USER_SYMBOL, font=synthetic_odb.FONT_V2, font_name="v2font")
            rc = roundup.main([str(job), "--out", str(Path(d) / "out"), "--no-siemens", "--no-images"])
            self.assertEqual(rc, 0)
            rep = json.loads((Path(d) / "out" / "synthetic-specv2" / "report.json").read_text())
            by = {L["kicad"]: L for L in rep["layers"]}
            for name in ("F.SilkS", "User.1", "In1.Cu"):   # arcs + T + V on silk, turned / mirrored text, negative text in a plane
                self.assertGreater(by[name]["ref_px"], 0, name)
                self.assertLess(by[name]["by"]["odb2kicad"]["xor_ratio"], 0.03, name)

    @unittest.skipUnless(BIN.is_file(), "odb2kicad not built")
    def test_sheet_tool_compares_a_board_with_the_reference(self):
        """tools/render_odb_sheet.py (reader spec v2, 5.5): PNGs per layer, and with --kicad the
        converter's board against them on a view grown to the layer's content."""
        tool = HERE.parent / "tools" / "render_odb_sheet.py"
        with tempfile.TemporaryDirectory() as d:
            job = synthetic_odb.write_job(Path(d) / "synthetic-specv2", synthetic_odb.SPECV2, synthetic_odb.PROFILE_IN,
                                          synthetic_odb.USER_SYMBOL, font=synthetic_odb.FONT_V2, font_name="v2font")
            pcb = Path(d) / "board.kicad_pcb"
            subprocess.run([str(BIN), str(job), str(pcb)], check=True, capture_output=True)
            def run(*extra):
                out = Path(d) / ("out" + "".join(extra).replace("-", "_"))
                subprocess.run([sys.executable, "-I", str(tool), str(job), "--layer", "notes", "--layer", "f.silk", "--ppm", "20",
                                "--out", str(out), "--kicad", str(pcb), *extra], check=True, capture_output=True)
                rep = json.loads((out / "render_report.json").read_text())
                return {L["kicad"]: L for L in rep["layers"]}
            by = run()
            self.assertEqual(sorted(by), ["F.SilkS", "User.1"])       # NOTES once, under its KiCad layer
            self.assertEqual((by["User.1"]["text_strokes"], by["F.SilkS"]["text_strokes"]), (8, 6))  # OPIO + PO, POI + IO
            for name, lay in by.items():
                self.assertTrue(Path(lay["png"]).is_file() and Path(lay["diff_png"]).is_file())
                self.assertLess(lay["converter"]["xor_ratio"], 0.03, name)
            # the tool as shipped reads the version field as a mirror flag: every glyph flipped
            shipped = run("--shipped-text-grammar")
            self.assertGreater(shipped["User.1"]["converter"]["xor_ratio"], 0.1)


if __name__ == "__main__":
    unittest.main()
