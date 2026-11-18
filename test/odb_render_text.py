"""ODB++ vector-font parser + T/V text renderer for the round-up's reference side.

Ported from the reader spec's ``tools/odb_render_text.py`` (odb2kicad-spec v2, section 5.5) and
wired into ``odb_render.OdbJob``. Coordinates follow the ODB++ / repo convention: mm, y up;
rotation clockwise about the text's location.

Font grammar (reference reader, ODB++ 8.x):
    UNITS=<INCH|MM>          # the font's own units, default INCH
    XSIZE <w>  YSIZE <h>  OFFSET <o>
    CHAR <c>
      LINE x1 y1 x2 y2 <P|N> <R|S> <width>     # stroke glyph (R = round ends, S = square)
      POLYGON [n]                             # filled-polygon glyph (points until LINE/POLYGON/ECHAR)
      ECHAR
    # glyph coordinates are used as written: the glyph's own zero goes on the text's location,
    # +y up, and each character advances one box (the record's xsize; V: one ysize along +y)

Where this port departs from the shipped tool (each is a reading of the ODB++ format the tool
got wrong or left open; ``tools/render_odb_sheet.py --shipped-text-grammar`` draws the tool's way):
  * the field after the polarity of a T record is ``orient_def`` (0-7 legacy, ``8 <angle>``,
    ``9 <angle>`` mirrored), and the last field is the record's version (0 / 1), not a mirror
    flag: the tool reads ``'FS' 1`` as mirrored text;
  * a mirrored text is mirrored as a whole about its location (it runs leftwards), not glyph by
    glyph in place;
  * fonts and features are read through the harness's readers, so ``.z`` files work, and a
    polygon's points may share a line.
"""
from __future__ import annotations

import math
import re
from pathlib import Path

import raster
from valor_oracle import parse_feature_geometry, read_odb_text


def _floats(tokens):
    try:
        return [float(t) for t in tokens]
    except ValueError:
        return None


def parse_font(path):
    """-> {'units', 'scale' (mm per font unit), 'xsize', 'ysize', 'offset', 'bad_lines': [n],
    'chars': {c: {'lines': [(x1, y1, x2, y2, positive, round, width)], 'polys': [[(x, y)..]]}}};
    None if unreadable or without a box size."""
    text = read_odb_text(path)
    if text is None:
        return None
    units, xsize, ysize, offset = "INCH", None, None, 0.0
    chars, cur, poly, bad = {}, None, None, []
    for n, raw in enumerate(text.splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        tok = line.split()
        k = tok[0].upper()
        if k.startswith("UNITS"):
            units = line[5:].lstrip(" \t=").strip().upper()
            if units not in ("INCH", "MM"):
                bad.append(n)
            continue
        if k in ("XSIZE", "YSIZE", "OFFSET"):
            v = _floats(tok[1:2])
            if not v:
                bad.append(n)
            elif k == "XSIZE":
                xsize = v[0]
            elif k == "YSIZE":
                ysize = v[0]
            else:
                offset = v[0]
        elif k == "CHAR":
            at = raw.find(tok[0])
            cur, poly = (raw[at + 5] if len(raw) > at + 5 else None), None  # "CHAR  " is a blank's glyph
            if cur is None:
                bad.append(n)
            else:
                chars.setdefault(cur, {"lines": [], "polys": []})
        elif k == "ECHAR":
            cur, poly = None, None
        elif cur is None:
            bad.append(n)
        elif k == "LINE":
            poly = None
            v = _floats(tok[1:5])
            if not v or len(v) < 4:
                bad.append(n)
                continue
            w = _floats(tok[7:8]) or [0.0]
            chars[cur]["lines"].append((v[0], v[1], v[2], v[3], len(tok) < 6 or tok[5].upper() != "N",
                                        len(tok) < 7 or tok[6].upper() != "S", w[0]))
        elif k == "POLYGON":
            poly = []
            chars[cur]["polys"].append(poly)
            v = _floats(tok[1:])
            if v is None:
                bad.append(n)
                continue
            v = v[len(v) % 2:]  # an odd count: the first is the number of points
            poly.extend(zip(v[0::2], v[1::2]))
        elif poly is not None:
            v = _floats(tok)
            if not v or len(v) % 2:
                bad.append(n)
                continue
            poly.extend(zip(v[0::2], v[1::2]))
        else:
            bad.append(n)
    if not xsize or not ysize or xsize <= 0 or ysize <= 0:
        return None
    return {"units": units, "scale": 1.0 if units == "MM" else 25.4, "xsize": xsize, "ysize": ysize,
            "offset": offset, "chars": chars, "bad_lines": bad}


def text_record(rec: tuple) -> dict:
    """A ('T', ...) record of valor_oracle.parse_feature_geometry as the renderer's dict."""
    _, x, y, pol, font, mirror, angle, xsize, ysize, weight, text, vertical = rec
    return {"kind": "V" if vertical else "T", "x": x, "y": y, "font": font.lower(), "pol": bool(pol),
            "angle": angle, "xsize": xsize, "ysize": ysize, "weight": weight if weight and weight > 0 else 1.0,
            "text": text, "mirror": 1 if mirror else 0}


def parse_text_records(features_path) -> list:
    """The T / V records of a features file (plain or .z), in the file's units:
    [{'kind': 'T'|'V', 'x', 'y', 'font', 'pol', 'angle', 'xsize', 'ysize', 'weight', 'text', 'mirror'}]."""
    if read_odb_text(features_path) is None:
        return []
    return [text_record(r) for r in parse_feature_geometry(features_path)["records"] if r[0] == "T"]


_SHIPPED = re.compile(r"^([TV])\s+(-?[\d.]+)\s+(-?[\d.]+)\s+(\S+)\s+([PN])((?:\s+-?[\d.]+){1,4})?\s+'([^']*)'\s*(\d+)?")


def parse_text_records_shipped(features_path) -> list:
    """The shipped tool's own reading of a T / V record (`<angle> <xsize> <ysize> <weight> '<text>'
    <mirror>`), kept to show what it draws: it takes the version field for a mirror flag."""
    text = read_odb_text(features_path)
    out = []
    for raw in (text or "").splitlines():
        m = _SHIPPED.match(raw.strip().split(";", 1)[0].strip())
        if not m:
            continue
        f = [float(v) for v in (m.group(6) or "").split()]
        out.append({"kind": m.group(1), "x": float(m.group(2)), "y": float(m.group(3)), "font": m.group(4).lower(),
                    "pol": m.group(5) != "N", "angle": f[0] if f else 0.0, "xsize": f[1] if len(f) >= 2 else None,
                    "ysize": f[2] if len(f) >= 3 else None, "weight": (f[3] if len(f) >= 4 else 1.0) or 1.0,
                    "text": m.group(7), "mirror": int(m.group(8) or 0), "glyph_mirror": True})
    return out


def text_strokes(rec: dict, font: dict, layer_scale_mm: float):
    """-> (strokes [(x1, y1, x2, y2, width_mm, positive, round)], polygons [(positive, [(x, y)..])]),
    absolute mm, y up. `layer_scale_mm`: mm per unit of the record's layer file."""
    if not font or not font.get("xsize") or not font.get("ysize"):
        return [], []
    s, fs = layer_scale_mm, font["scale"]
    fx, fy = font["xsize"] * fs, font["ysize"] * fs
    tw = rec["xsize"] * s if rec["xsize"] else fx
    th = rec["ysize"] * s if rec["ysize"] else fy
    sx, sy = tw / fx, th / fy
    a = math.radians(rec["angle"])
    ca, sa = math.cos(a), math.sin(a)
    X, Y = rec["x"] * s, rec["y"] * s
    vertical = rec["kind"] == "V"
    in_place = rec.get("glyph_mirror", False)  # the shipped tool: each glyph flipped where it stands

    def place(px, py, adv):
        px, py = px * sx * fs, py * sy * fs
        if rec["mirror"] and in_place:
            px = -px
        lx, ly = (px, adv + py) if vertical else (adv + px, py)
        if rec["mirror"] and not in_place:
            lx = -lx
        return X + lx * ca + ly * sa, Y - lx * sa + ly * ca  # clockwise, y up

    strokes, polys, adv = [], [], 0.0
    for ch in rec["text"]:
        glyph = font["chars"].get(ch)
        if glyph:
            for x1, y1, x2, y2, positive, rnd, w in glyph["lines"]:
                (ax, ay), (bx, by) = place(x1, y1, adv), place(x2, y2, adv)
                strokes.append((ax, ay, bx, by, w * fs * rec["weight"], rec["pol"] == positive, rnd))
            for pts in glyph["polys"]:
                if len(pts) >= 3:
                    polys.append((rec["pol"], [place(x, y, adv) for x, y in pts]))
        adv += th if vertical else tw
    return strokes, polys


def text_contours(rec: dict, font: dict, layer_scale_mm: float, font_scale_mm: float = None, ppm: float = 40.0) -> list:
    """[(value, [contours in mm, y up, absolute])] for one text record; value 1 draws, 0 clears.
    (`font_scale_mm` is the shipped tool's argument: the font knows its own scale.)"""
    strokes, polys = text_strokes(rec, font, layer_scale_mm)
    ops = [(1 if positive else 0, [raster.stroke(x1, y1, x2, y2, w, ppm, square=not rnd)])
           for x1, y1, x2, y2, w, positive, rnd in strokes]
    ops += [(1 if positive else 0, [pts]) for positive, pts in polys]
    return ops


def write_png(path, w, h, buf):
    """Minimal grayscale PNG from a raster.Bitmap buffer (1 = ink): white ground, black ink."""
    import struct
    import zlib

    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    out = bytearray()
    for y in range(h):
        out.append(0)  # filter: none
        out.extend(255 if v == 0 else 0 for v in buf[y * w:(y + 1) * w])
    png = (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 0, 0, 0, 0))
           + chunk(b"IDAT", zlib.compress(bytes(out), 9)) + chunk(b"IEND", b""))
    Path(path).write_bytes(png)
