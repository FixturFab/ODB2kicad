"""Tests for compare_oracle.py's bounding_box and pin1_positions checks: they pass on the
converter's boards and fail on boards broken on purpose. Uses the samples under samples/;
the converter is only needed for the synthetic multilayer case (skipped when it is not built).

    python3 -m unittest discover -s test -p 'test_compare_oracle.py' -v
"""

from __future__ import annotations

import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
sys.path.insert(0, str(HERE))

from compare_oracle import KiCadPcbParser, OracleComparator, _footprint_to_board  # noqa: E402
from valor_oracle import BBox, DirectOdbOracle  # noqa: E402

BIN = ROOT / "odb2kicad" / "build" / "odb2kicad"
SIMPLE = ROOT / "samples" / "odb-output"
SIMPLE_PCB = ROOT / "samples" / "test.kicad_pcb"


def check(odb: Path, pcb_text: str, category: str):
    with tempfile.TemporaryDirectory() as d:
        p = Path(d) / "b.kicad_pcb"
        p.write_text(pcb_text, encoding="latin-1")
        kd = KiCadPcbParser(str(p)).parse()
    od = DirectOdbOracle(str(odb)).parse()
    res = [r for r in OracleComparator(tolerance=0.1).compare(od, kd) if r.category == category]
    assert len(res) == 1, res
    return res[0]


class Geometry(unittest.TestCase):
    def test_footprint_turn(self):
        # KiCad: +90 deg turns counter-clockwise on screen (Y down): local +X goes to board -Y
        x, y = _footprint_to_board(10, 20, 90, 1, 0)
        self.assertAlmostEqual(x, 10)
        self.assertAlmostEqual(y, 19)

    def test_arc_bulge(self):
        b = BBox()
        b.add_arc((1, 0), (-1, 0), (0, 0), cw=False)  # upper half circle
        self.assertAlmostEqual(b.ymax, 1)
        self.assertAlmostEqual(b.ymin, 0)
        b = BBox()
        b.add_arc((1, 0), (-1, 0), (0, 0), cw=True)  # lower half circle
        self.assertAlmostEqual(b.ymin, -1)
        self.assertAlmostEqual(b.ymax, 0)


class Checks(unittest.TestCase):
    def setUp(self):
        self.pcb = SIMPLE_PCB.read_text(encoding="latin-1")

    def test_stored_board_passes(self):
        self.assertTrue(check(SIMPLE, self.pcb, "bounding_box").passed)
        self.assertTrue(check(SIMPLE, self.pcb, "pin1_positions").passed)

    def test_outline_moved_fails(self):
        # the board edge 1 mm wider than the profile
        broken, n = re.subn(r'(\(gr_rect \(start [-\d.]+ [-\d.]+\) \(end )([-\d.]+)',
                            lambda m: m.group(1) + str(float(m.group(2)) + 1.0), self.pcb, count=1)
        self.assertEqual(n, 1)
        r = check(SIMPLE, broken, "bounding_box")
        self.assertFalse(r.passed)
        self.assertIn("xmax +1.0000", r.message)

    def test_no_edge_cuts_fails(self):
        broken = re.sub(r'\(layer "Edge\.Cuts"\)', '(layer "Dwgs.User")', self.pcb)
        self.assertFalse(check(SIMPLE, broken, "bounding_box").passed)

    def test_footprint_turned_fails(self):
        # turn the first footprint by 90 deg: its pin 1 moves, the check names the turn
        broken, n = re.subn(r'(\(footprint "[^"]+"[\s\S]*?\(at [-\d.]+ [-\d.]+)\)', r'\1 90)', self.pcb, count=1)
        self.assertEqual(n, 1)
        r = check(SIMPLE, broken, "pin1_positions")
        self.assertFalse(r.passed)
        self.assertIn("turned 90 deg", r.message)

    @unittest.skipUnless(BIN.exists(), "converter not built")
    def test_multilayer_rotated_and_bottom_parts(self):
        import synthetic_odb  # noqa: PLC0415
        with tempfile.TemporaryDirectory() as d:
            odb = synthetic_odb.write_multilayer_job(Path(d) / "synthetic-multilayer")
            out = Path(d) / "multilayer.kicad_pcb"
            subprocess.run([str(BIN), str(odb), str(out)], check=True, capture_output=True, timeout=120)
            text = out.read_text(encoding="latin-1")
            self.assertTrue(check(odb, text, "bounding_box").passed)
            r = check(odb, text, "pin1_positions")
        self.assertTrue(r.passed, r.message)
        self.assertIn("all 6 of 6", r.message)

if __name__ == "__main__":
    unittest.main()
