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
``stackup`` (only with --variant stackup) ``basic`` again with layer attrlists: dielectric
           thicknesses and copper weights (the board is 1.542 mm thick), and an output transform
           on SIG3 (.out_x_scale / .out_y_scale 0.5), empty and near-empty attrlists elsewhere.
``text``   (only with --variant text) ``basic`` again with T text records and a vector font
           (fonts/standard): plain, turned and mirrored text on the silkscreen and on the NOTES
           document layer (drawn on User.1), and a negative text cut out of the GND plane.
``specv2`` (only with --variant specv2) ``basic`` again with a millimetre font holding filled
           POLYGON glyphs and square-ended lines, T and V text, and arcs without a direction token
``steps``  (only with --variant steps) a job with two steps (PCB = ``basic`` in INCH, FLEX =
           ``edge`` in MM), an empty PANEL step that repeats PCB and a step folder the matrix does
           not name; the matrix lists the steps out of COL order.
``origin`` (only with --variant origin) ``basic`` again with a stephdr origin (X_ORIGIN=0.5,
           Y_ORIGIN=-0.25 in): every coordinate of the step is relative to it, so the whole board
           sits 12.7 mm right and 6.35 mm down of where its numbers say.
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
# the stackup variant's layer attrlists: a 4-layer stack (INCH) and an output transform on SIG3
STACKUP_ATTRS = {
    "TOP": "UNITS=INCH\n.copper_weight=1\n", "GND": "UNITS=INCH\n.copper_weight=0.5\n",
    "SIG3": "UNITS=INCH\n.copper_weight=0.5\n.out_x_scale = 0.500000\n.out_y_scale = 0.500000\n.out_mirror = no\n.out_angle = 0.0\n",
    "BOTTOM": "UNITS=INCH\n.copper_weight=1\n",
    "D1": "UNITS=INCH\n.layer_dielectric=0.008\n", "D2": "UNITS=INCH\n.layer_dielectric=0.04\n",
    "D3": "UNITS=INCH\n.layer_dielectric=0.008\n",
    "F.MASK": "UNITS=INCH\n.layer_dielectric=0.0006\n", "B.MASK": "", "F.SILK": "\n", "NOTES": ".comment = \n",
}
# the text variant: a font in the standard font's layout (0.302 box, characters 0.2 x 0.3 from
# (-0.1, -0.1), 12 mil lines) with the letters of "TEXT LIE", and T records on silk and NOTES
def _glyph(ch, lines):
    return f"CHAR {ch}\n" + "".join(f"LINE {a:.6f} {b:.6f} {c:.6f} {d:.6f} P R 0.012000\n" for a, b, c, d in lines) + "ECHAR\n\n"


TEXT_FONT = "XSIZE  0.302000\nYSIZE  0.302000\nOFFSET 0.000000\n\n" + "".join([
    _glyph("T", [(-0.1, 0.2, 0.1, 0.2), (0, 0.2, 0, -0.1)]),
    _glyph("E", [(-0.1, -0.1, -0.1, 0.2), (-0.1, 0.2, 0.1, 0.2), (-0.1, 0.05, 0.05, 0.05), (-0.1, -0.1, 0.1, -0.1)]),
    _glyph("X", [(-0.1, -0.1, 0.1, 0.2), (-0.1, 0.2, 0.1, -0.1)]),
    _glyph("L", [(-0.1, 0.2, -0.1, -0.1), (-0.1, -0.1, 0.1, -0.1)]),
    _glyph("I", [(0, -0.1, 0, 0.2)]),
    _glyph(".", [(0.025, -0.075, 0.025, -0.075)]),
])
TEXT = dict(BASIC)
TEXT["F.SILK"] = ("INCH", ["r6"], ["L 0.2 0.15 0.6 0.15 0 P 0", "T 0.9 0.6 standard P 0 0.1 0.12 1 'TEXT LIE.' 1",
                                   "T 1.9 0.2 standard P 8 90 0.06 0.06 0.5 'TILE' 1"])
TEXT["NOTES"] = ("INCH", ["r5"], ["L 0 0 2 1.2 0 P 0", "T 0.2 1.0 standard P 0 0.15 0.15 2 'EXIT' 1;0",
                                  "T 1.8 0.5 standard P 9 30 0.1 0.1 1 'LET' 1"])
TEXT["GND"] = ("INCH", ["r40"], BASIC["GND"][2] + ["T 0.3 0.75 standard N 0 0.12 0.12 2 'TEXT' 1"])
# the specv2 variant (reader spec v2): a font in millimetres (UNITS=MM, a 4 x 6 box) with a
# square-ended stroke glyph, a filled POLYGON glyph and one that is both; T and V records drawn
# with it; arcs written without their direction token (read counter-clockwise), one with it
FONT_V2 = ("UNITS=MM\nXSIZE 4.000000\nYSIZE 6.000000\nOFFSET 0.000000\n\n"
           "CHAR I\nLINE 1.000000 0.000000 1.000000 5.000000 P S 0.300000\nECHAR\n\n"
           "CHAR O\nPOLYGON 4\n0.000000 0.000000\n3.000000 0.000000\n3.000000 5.000000\n0.000000 5.000000\nECHAR\n\n"
           "CHAR P\nLINE 0.500000 0.000000 0.500000 5.000000 P R 0.300000\n"
           "POLYGON\n0.500000 2.500000 3.000000 2.500000\n3.000000 5.000000 0.500000 5.000000\nECHAR\n")
SPECV2 = dict(BASIC)
SPECV2["F.SILK"] = ("INCH", ["r6"], ["A 0.5 0.2 0.3 0.2 0.4 0.2 0 P 0", "A 0.9 0.2 0.7 0.2 0.8 0.2 0 P 0 Y", "A 1.2 0.3 1.3 0.2 1.2 0.2 0",
                                     "T 0.2 0.5 v2font P 0 0.1 0.15 1 'POI' 1", "V 1.6 0.2 v2font P 0 0.1 0.15 2 'IO' 1"])
SPECV2["NOTES"] = ("MM", ["r100"], ["L 0 0 50 30 0 P 0", "T 10 20 v2font P 8 30 3 4.5 1 'OPIO' 1", "T 40 8 v2font P 9 45 2 3 1 'PO' 1"])
SPECV2["GND"] = ("INCH", ["r40"], BASIC["GND"][2] + ["T 0.3 0.75 v2font N 0 0.12 0.18 2 'OP' 1"])
ORIGIN_IN = (0.5, -0.25)  # the origin variant's stephdr X_ORIGIN / Y_ORIGIN, inches
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
              negative=(), origin=(0, 0), attrlists=None, font=None, font_name: str = "standard") -> Path:
    step = root / "steps" / "pcb"
    (root / "matrix").mkdir(parents=True, exist_ok=True)
    (root / "misc").mkdir(parents=True, exist_ok=True)
    (root / "matrix" / "matrix").write_text(_matrix(negative))
    (root / "misc" / "info").write_text(f"JOB_NAME=synthetic\nUNITS={units}\nODB_SOURCE=roundup-synthetic\n")
    step.mkdir(parents=True, exist_ok=True)
    (step / "profile").write_text(profile)
    org = f"X_ORIGIN={origin[0]}\nY_ORIGIN={origin[1]}\n" if tuple(origin) != (0, 0) else ""
    (step / "stephdr").write_text(f"UNITS={units}\nX_DATUM=0\nY_DATUM=0\n{org}")
    for name, _t, _c in MATRIX_ROWS:
        d = step / "layers" / name.lower()
        d.mkdir(parents=True, exist_ok=True)
        units, syms, body = layers.get(name, (units_empty or units, [], []))
        (d / "features").write_text(_features(units, syms, body))
        if attrlists and name in attrlists:
            (d / "attrlist").write_text(attrlists[name])
    if font:
        (root / "fonts").mkdir(parents=True, exist_ok=True)
        (root / "fonts" / font_name).write_text(font)
    if user_symbol:
        d = root / "symbols" / "logo"
        d.mkdir(parents=True, exist_ok=True)
        (d / "features").write_text(_features(*user_symbol))
    return root


def write_two_step_job(root: Path) -> Path:
    """A job with two real steps and an empty one. The matrix lists FLEX (COL=2) before PCB
    (COL=1): the first step is PCB, by COL. FLEX is the ``edge`` board in MM, PCB the ``basic``
    board in INCH; PANEL has a stephdr that repeats PCB and no layer data; SCRAP is a step folder
    the matrix does not name."""
    write_job(root, BASIC, PROFILE_IN, USER_SYMBOL)
    rows = _matrix().split("\n", 1)[1].split("}\n", 1)[1]  # the LAYER rows of the one-step matrix
    steps = "".join(f"STEP {{\n    COL={c}\n    NAME={n}\n}}\n\n" for c, n in ((2, "FLEX"), (3, "PANEL"), (1, "PCB")))
    (root / "matrix" / "matrix").write_text(steps + rows)
    (root / "misc" / "info").write_text("JOB_NAME=two step/job\nUNITS=INCH\nODB_SOURCE=roundup-synthetic\n")
    for name, layers, profile, units in (("flex", EDGE, PROFILE_MM, "MM"), ("scrap", OUTLINE, PROFILE_L, "MM")):
        step = root / "steps" / name
        step.mkdir(parents=True, exist_ok=True)
        (step / "profile").write_text(profile)
        (step / "stephdr").write_text(f"UNITS={units}\nX_DATUM=0\nY_DATUM=0\n")
        for lname, _t, _c in MATRIX_ROWS:
            d = step / "layers" / lname.lower()
            d.mkdir(parents=True, exist_ok=True)
            u, syms, body = layers.get(lname, (units, [], []))
            (d / "features").write_text(_features(u, syms, body))
    panel = root / "steps" / "panel"
    panel.mkdir(parents=True, exist_ok=True)
    (panel / "stephdr").write_text("UNITS=INCH\nX_DATUM=0\nY_DATUM=0\n\nSTEP-REPEAT {\n    NAME=PCB\n    X=0.5\n    Y=0.5\n"
                                   "    DX=2.5\n    DY=0\n    NX=2\n    NY=1\n    ANGLE=0\n    MIRROR=NO\n}\n")
    (panel / "profile").write_text("UNITS=INCH\nF 1\nS P 0\nOB 0 0 I\nOS 0 2.2\nOS 6 2.2\nOS 6 0\nOS 0 0\nOE\nSE\n")
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
    ap.add_argument("--variant", choices=("all", "basic", "edge", "mixed", "outline", "negative", "symbols", "origin", "stackup", "steps", "text", "specv2"), default="all")
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
    if a.variant == "origin":  # not in "all" either
        made.append(write_job(a.out / "synthetic-origin", BASIC, PROFILE_IN, USER_SYMBOL, origin=ORIGIN_IN))
    if a.variant == "stackup":  # not in "all" either
        made.append(write_job(a.out / "synthetic-stackup", BASIC, PROFILE_IN, USER_SYMBOL, attrlists=STACKUP_ATTRS))
    if a.variant == "steps":  # not in "all" either
        made.append(write_two_step_job(a.out / "synthetic-steps"))
    if a.variant == "text":  # not in "all" either
        made.append(write_job(a.out / "synthetic-text", TEXT, PROFILE_IN, USER_SYMBOL, font=TEXT_FONT))
    if a.variant == "specv2":  # not in "all" either
        made.append(write_job(a.out / "synthetic-specv2", SPECV2, PROFILE_IN, USER_SYMBOL, font=FONT_V2, font_name="v2font"))
    for m in made:
        print(m)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
