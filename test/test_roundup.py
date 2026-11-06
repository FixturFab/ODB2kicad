"""Tests for the ODB++ round-up harness. Made-up boards only; no converter needed
except the last test (skipped when odb2kicad/build/odb2kicad is not built).

    python3 -m unittest discover -s test -p 'test_roundup.py' -v
"""

from __future__ import annotations

import json
import math
import os
import socket
import sys
import tempfile
import threading
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import raster  # noqa: E402
from raster import _popcount  # noqa: E402
import roundup  # noqa: E402
import synthetic_odb  # noqa: E402
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


if __name__ == "__main__":
    unittest.main()
