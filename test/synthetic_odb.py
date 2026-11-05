#!/usr/bin/env python3
"""Made-up ODB++ boards for the round-up: no customer data, every feature known.

    python3 test/synthetic_odb.py <out_dir> [--variant all|basic|edge]

``basic``  a 4-layer board in INCH units: pads (r, rect with rounded corners, oval, donut, rotated
           and mirrored), traces and arcs on every copper layer (In1/In2 included), a surface with a
           hole, a negative pad clearing a plane, a user symbol, solder mask openings larger than
           the copper, silk lines, a drill layer and a rectangular profile.
``mixed``  ``edge`` again, but the layers it leaves empty say UNITS=INCH: ODB++ lets every file
           pick its own units, so a reader must scale each file by its own UNITS line.
``negative`` (only with --variant negative) negative features on a plane layer in file order, and a
           NEGATIVE matrix layer.
``outline`` (only with --variant outline) an L-shaped MM profile whose first OS does not repeat
           OB, with one rounded inner corner, and an L-shaped plane.
``symbols`` (only with --variant symbols) every standard symbol 0.3.0 drew wrong or not at all, on
           TOP (turned / mirrored), negative in a GND plane, and on an inner layer.
``edge``   the corner cases a converter is most likely to get wrong, one per layer: a thermal
           relief, a chamfered rect, a full-circle arc, a clockwise surface arc, an `I`/`M` unit
           override on a symbol, a 9-orient (mirrored) flash at 30 deg, an octagon and an ellipse.

The matching hand-written KiCad board for ``basic`` (positive features only, used by
test_roundup.py to prove the two renderers agree) is ``kicad_equivalent()``.
"""

from __future__ import annotations

import argparse
from pathlib import Path

MATRIX_ROWS = [
    ("COMP_+_TOP", "COMPONENT", "BOARD"), ("F.SILK", "SILK_SCREEN", "BOARD"), ("F.MASK", "SOLDER_MASK", "BOARD"),
    ("TOP", "SIGNAL", "BOARD"), ("D1", "DIELECTRIC", "BOARD"), ("GND", "POWER_GROUND", "BOARD"), ("D2", "DIELECTRIC", "BOARD"),
    ("SIG3", "SIGNAL", "BOARD"), ("D3", "DIELECTRIC", "BOARD"), ("BOTTOM", "SIGNAL", "BOARD"), ("B.MASK", "SOLDER_MASK", "BOARD"),
    ("DRILL", "DRILL", "BOARD"), ("NOTES", "DOCUMENT", "MISC"),
]


def _matrix(negative=()) -> str:
    out = ["STEP {\n    COL=1\n    NAME=PCB\n}\n"]
    for i, (name, t, ctx) in enumerate(MATRIX_ROWS, 1):
        pol = "NEGATIVE" if name in negative else "POSITIVE"
        out.append(f"LAYER {{\n    ROW={i}\n    CONTEXT={ctx}\n    TYPE={t}\n    NAME={name}\n    POLARITY={pol}\n}}\n")
    return "\n".join(out)


def _features(units: str, syms: list, body: list) -> str:
    return "\n".join([f"UNITS={units}", f"F {len(body)}"] + [f"${i} {s}" for i, s in enumerate(syms)] + [""] + body) + "\n"


# basic board, INCH: a 2.0 x 1.2 in board (50.8 x 30.48 mm), origin bottom-left
PROFILE_IN = "UNITS=INCH\nF 1\nS P 0\nOB 0 0 I\nOS 0 1.2\nOS 2.0 1.2\nOS 2.0 0\nOS 0 0\nOE\nSE\n"

BASIC = {
    "TOP": ("INCH", ["r20", "rect60x40xr10", "oval80x40", "donut_r100x50", "r8", "logo"], [
        "P 0.3 0.3 0 P 0 0",
        "P 0.5 0.3 1 P 0 8 0.0",
        "P 0.7 0.3 1 P 0 8 90.0",
        "P 0.9 0.3 2 P 0 8 45.0",
        "P 1.2 0.3 3 P 0 0",
        "L 0.3 0.3 0.5 0.3 4 P 0",
        "L 0.5 0.3 0.5 0.8 4 P 0",
        "A 0.5 0.8 0.8 0.8 0.65 0.8 4 P 0 Y",
        "P 1.6 0.8 5 P 0 0",
    ]),
    "GND": ("INCH", ["r40"], [
        "S P 0", "OB 0.1 0.1 I", "OS 0.1 1.1", "OS 1.9 1.1", "OS 1.9 0.1", "OS 0.1 0.1", "OE",
        "OB 0.8 0.4 H", "OS 1.2 0.4", "OS 1.2 0.6", "OS 0.8 0.6", "OS 0.8 0.4", "OE", "SE",
        "P 1.5 0.5 0 N 0 0",
    ]),
    "SIG3": ("INCH", ["r10"], [
        "L 0.2 1.0 1.8 1.0 0 P 0",
        "A 1.0 0.2 1.0 0.2 1.0 0.35 0 P 0 N",
    ]),
    "BOTTOM": ("INCH", ["rect50x30", "r20"], [
        "P 0.6 0.9 0 P 0 9 30.0",
        "P 1.4 0.9 0 P 0 8 30.0",
        "P 0.3 0.3 1 P 0 0",
    ]),
    "F.MASK": ("INCH", ["r26", "rect66x46xr13"], [
        "P 0.3 0.3 0 P 0 0",
        "P 0.5 0.3 1 P 0 8 0.0",
        "P 0.7 0.3 1 P 0 8 90.0",
    ]),
    "B.MASK": ("INCH", ["rect56x36"], ["P 0.6 0.9 0 P 0 9 30.0"]),
    "F.SILK": ("INCH", ["r6"], ["L 0.2 0.15 0.6 0.15 0 P 0", "L 0.6 0.15 0.6 0.45 0 P 0"]),
    "DRILL": ("INCH", ["r12"], ["P 0.3 0.3 0 P 0 0"]),
    "NOTES": ("INCH", ["r5"], ["L 0 0 2 1.2 0 P 0"]),
}
USER_SYMBOL = ("INCH", ["r30"], ["P 0 0 0 P 0 0", "P 0.05 0 0 P 0 0", "L -0.05 -0.05 0.05 -0.05 0 P 0"])

EDGE = {
    "TOP": ("MM", ["thr2000x1400x45x4x300", "rect1500x1000xc300", "r200", "oct1600x1600x400", "el2000x1000",
                   "rect50x30 I"], [
        "P 5 5 0 P 0 0",
        "P 10 5 1 P 0 8 15.0",
        "A 15 5 15 5 15 6.5 2 P 0 Y",
        "P 20 5 3 P 0 0",
        "P 25 5 4 P 0 8 60.0",
        "P 30 5 5 P 0 0",
    ]),
    "GND": ("MM", ["r300"], ["S P 0", "OB 5 10 I", "OS 15 10", "OC 15 20 15 15 Y", "OS 5 20", "OS 5 10", "OE", "SE"]),
    "SIG3": ("MM", ["s300"], ["L 5 15 30 15 0 P 0"]),
    "BOTTOM": ("MM", ["rect2000x1000"], ["P 10 20 0 P 0 9 30.0", "P 20 20 0 P 0 6"]),
}
PROFILE_MM = "UNITS=MM\nF 1\nS P 0\nOB 0 0 I\nOS 0 25\nOS 35 25\nOS 35 0\nOS 0 0\nOE\nSE\n"

# outline board, MM: an L-shaped profile whose first OS does not repeat OB (KiCad's
# exporter repeats it, Cadence's does not), with a rounded corner (OC), and an L-shaped plane
OUTLINE = {
    "GND": ("MM", ["r300"], ["S P 0", "OB 2 2 I", "OS 28 2", "OS 28 8", "OS 8 8", "OS 8 23", "OS 2 23", "OS 2 2", "OE", "SE"]),
    "TOP": ("MM", ["r1000"], ["P 5 5 0 P 0 0"]),
}
PROFILE_L = ("UNITS=MM\nF 1\nS P 0\nOB 0 0 I\nOS 30 0\nOS 30 10\nOS 15 10\nOC 10 15 15 15 N\nOS 10 25\n"
             "OS 0 25\nOS 0 0\nOE\nSE\n")


# negative board, MM: negative features on a positive plane layer (GND), in file
# order -- the first one comes before the plane and must not clear it, one crosses the plane's
# edge, one overlaps a hole, one is a surface, one a line, one an arc, and a positive pad drawn
# after a negative one restores copper inside its clearance -- and a NEGATIVE matrix layer (SIG3):
# the board is copper, its positive features are clearances (overlapping antipads, a slot, one
# crossing the board edge).
NEGATIVE = {
    "GND": ("MM", ["r3000", "rect4000x3000", "r800", "r1500"], [
        "P 35 25 0 N 0 0",
        "S P 0", "OB 2 2 I", "OS 2 28", "OS 38 28", "OS 38 2", "OS 2 2", "OE",
        "OB 14 14 H", "OS 18 14", "OS 18 18", "OS 14 18", "OS 14 14", "OE", "SE",
        "P 10 10 0 N 0 0",
        "P 2 15 1 N 0 8 30.0",
        "P 18 18 0 N 0 0",
        "L 20 6 32 9 2 N 0",
        "A 25 20 31 20 28 20 2 N 0 Y",
        "S N 0", "OB 30 22 I", "OS 36 22", "OS 33 27", "OS 30 22", "OE", "SE",
        "P 10 10 3 P 0 0",
    ]),
    "SIG3": ("MM", ["r2000", "r2400", "oval3000x1500"], [
        "P 8 8 0 P 0 0",
        "P 9.5 8 1 P 0 0",
        "P 20 15 2 P 0 8 45.0",
        "L 25 5 25 25 0 P 0",
        "P 0 20 1 P 0 0",
    ]),
    "TOP": ("MM", ["r1000"], ["P 5 5 0 P 0 0"]),
}
PROFILE_NEG = "UNITS=MM\nF 1\nS P 0\nOB 0 0 I\nOS 0 30\nOS 40 30\nOS 40 0\nOS 0 0\nOE\nSE\n"


# symbols board, MM: one flash of every standard symbol 0.3.0 drew wrong or not at all (ODB++ 8.1
# Appendix A: thermals with rounded ends, corners or open corners, oval / oblong thermals, the half
# oval, moire, the stencil symbols and a capitalised name), turned, mirrored and plain, on TOP; the
# same thermals flashed negative into a GND plane (thermal reliefs) and positive on an inner layer.
SYMBOL_NAMES = [
    "thr3000x2000x45x4x400", "thr3000x2000x0x2x400", "ths3000x2000x30x3x400", "s_ths3000x2000x0x4x400xr500",
    "s_ths3000x2000x45x4x400xc400x13", "s_tho3000x2000x45x4x400", "s_thr3000x2000x45x4x400", "sr_ths3000x2000x45x4x400",
    "rc_ths4000x2500x45x4x400x400", "rc_ths4000x2500x0x4x400x400xr600", "rc_tho4000x2500x45x4x400x400",
    "o_ths4000x2500x0x4x400x400", "oblong_ths4000x2500x45x4x400x400xr", "oblong_ths4000x2500x90x2x400x400xs",
    "oval_h3000x1600", "moire150x400x3x150x3000x15", "hplate3000x2000x800xra200xro100", "rhplate3000x2000x800",
    "fhplate3000x2000x500x700xra150", "radhplate3000x2000x800xra200", "dshape3000x2000x800xra100",
    "cross3000x3000x600x600x30x60xrxra200", "dogbone3000x3000x600x800x50xsxra200", "dpack3000x2000x300x300x3x2xra100",
    "THR3000X2000X45X4X400",
]
_ORIENTS = ["0", "8 30.0", "9 30.0", "1", "6", "8 135.0"]


def _symbols_top() -> list:
    out = []
    for i in range(len(SYMBOL_NAMES)):
        x, y = 6 + 7 * (i % 5), 6 + 7 * (i // 5)
        out.append(f"P {x} {y} {i} P 0 {_ORIENTS[i % len(_ORIENTS)]}")
    return out


SYMBOLS = {
    "TOP": ("MM", SYMBOL_NAMES, _symbols_top()),
    "GND": ("MM", ["thr3000x2000x45x4x400", "rc_ths4000x2500x45x4x400x400", "s_thr3000x2000x45x4x400", "moire150x400x3x150x3000x15"], [
        "S P 0", "OB 1 1 I", "OS 1 47", "OS 41 47", "OS 41 1", "OS 1 1", "OE", "SE",
        "P 6 6 0 N 0 0", "P 13 6 1 N 0 8 20.0", "P 20 6 2 N 0 0", "P 27 6 3 N 0 0",
    ]),
    "SIG3": ("MM", ["o_ths4000x2500x0x4x400x400", "oblong_ths4000x2500x45x4x400x400xr", "s_tho3000x2000x45x4x400"], [
        "P 6 20 0 P 0 8 10.0", "P 13 20 1 P 0 0", "P 20 20 2 P 0 9 45.0",
    ]),
}
PROFILE_SYM = "UNITS=MM\nF 1\nS P 0\nOB 0 0 I\nOS 0 48\nOS 42 48\nOS 42 0\nOS 0 0\nOE\nSE\n"


def write_job(root: Path, layers: dict, profile: str, user_symbol=None, units: str = "INCH", units_empty: str = "",
              negative=()) -> Path:
    step = root / "steps" / "pcb"
    (root / "matrix").mkdir(parents=True, exist_ok=True)
    (root / "misc").mkdir(parents=True, exist_ok=True)
    (root / "matrix" / "matrix").write_text(_matrix(negative))
    (root / "misc" / "info").write_text(f"JOB_NAME=synthetic\nUNITS={units}\nODB_SOURCE=roundup-synthetic\n")
    step.mkdir(parents=True, exist_ok=True)
    (step / "profile").write_text(profile)
    (step / "stephdr").write_text(f"UNITS={units}\nX_DATUM=0\nY_DATUM=0\n")
    for name, _t, _c in MATRIX_ROWS:
        d = step / "layers" / name.lower()
        d.mkdir(parents=True, exist_ok=True)
        units, syms, body = layers.get(name, (units_empty or units, [], []))
        (d / "features").write_text(_features(units, syms, body))
    if user_symbol:
        d = root / "symbols" / "logo"
        d.mkdir(parents=True, exist_ok=True)
        (d / "features").write_text(_features(*user_symbol))
    return root


def kicad_equivalent() -> str:
    """BASIC's positive TOP / F.SILK / profile features, as KiCad would hold them (mm, y down)."""
    I = 25.4  # noqa: E741

    def p(x, y):
        return f"{x * I:.6f} {-y * I:.6f}"
    fp = lambda x, y, shape, w, h, rot=0, extra="": (  # noqa: E731
        f'(footprint "x" (layer "F.Cu") (at {p(x, y)}) (pad "1" smd {shape} (at 0 0 {rot}) (size {w * I / 1000:.6f} {h * I / 1000:.6f}) '
        f'(layers "F.Cu") {extra}))')
    items = [
        fp(0.3, 0.3, "circle", 20, 20),
        fp(0.5, 0.3, "roundrect", 60, 40, 0, "(roundrect_rratio 0.25)"),
        fp(0.7, 0.3, "roundrect", 60, 40, -90, "(roundrect_rratio 0.25)"),   # ODB 90 cw = KiCad -90
        fp(0.9, 0.3, "oval", 80, 40, -45),
        f'(gr_circle (center {p(1.2, 0.3)}) (end {p(1.2 + 0.0375, 0.3)}) (stroke (width {25 * I / 1000:.6f})) (fill none) (layer "F.Cu"))',
        f'(segment (start {p(0.3, 0.3)}) (end {p(0.5, 0.3)}) (width {8 * I / 1000:.6f}) (layer "F.Cu"))',
        f'(segment (start {p(0.5, 0.3)}) (end {p(0.5, 0.8)}) (width {8 * I / 1000:.6f}) (layer "F.Cu"))',
        f'(arc (start {p(0.5, 0.8)}) (mid {p(0.65, 0.95)}) (end {p(0.8, 0.8)}) (width {8 * I / 1000:.6f}) (layer "F.Cu"))',
        f'(gr_line (start {p(0.2, 0.15)}) (end {p(0.6, 0.15)}) (stroke (width {6 * I / 1000:.6f})) (layer "F.SilkS"))',
        f'(gr_line (start {p(0.6, 0.15)}) (end {p(0.6, 0.45)}) (stroke (width {6 * I / 1000:.6f})) (layer "F.SilkS"))',
        f'(gr_rect (start {p(0, 1.2)}) (end {p(2.0, 0)}) (stroke (width 0.1)) (fill none) (layer "Edge.Cuts"))',
    ]
    layers = '(layers (0 "F.Cu" signal) (1 "In1.Cu" signal) (2 "In2.Cu" signal) (31 "B.Cu" signal) (37 "F.SilkS" user) (44 "Edge.Cuts" user))'
    return "(kicad_pcb (version 20240108) (generator test)\n" + layers + "\n" + "\n".join(items) + ")\n"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("out", type=Path)
    ap.add_argument("--variant", choices=("all", "basic", "edge", "mixed", "outline", "negative", "symbols"), default="all")
    a = ap.parse_args()
    made = []
    if a.variant in ("all", "basic"):
        made.append(write_job(a.out / "synthetic-basic", BASIC, PROFILE_IN, USER_SYMBOL))
    if a.variant in ("all", "edge"):
        made.append(write_job(a.out / "synthetic-edge", EDGE, PROFILE_MM, units="MM"))
    if a.variant in ("all", "mixed"):
        made.append(write_job(a.out / "synthetic-mixed", EDGE, PROFILE_MM, units="MM", units_empty="INCH"))
    if a.variant == "outline":  # not in "all": the PC-lane recipe pins the "all" set
        made.append(write_job(a.out / "synthetic-outline", OUTLINE, PROFILE_L, units="MM"))
    if a.variant == "negative":  # not in "all" either
        made.append(write_job(a.out / "synthetic-negative", NEGATIVE, PROFILE_NEG, units="MM", negative=("SIG3",)))
    if a.variant == "symbols":  # not in "all" either
        made.append(write_job(a.out / "synthetic-symbols", SYMBOLS, PROFILE_SYM, units="MM"))
    for m in made:
        print(m)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
