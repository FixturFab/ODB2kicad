"""ODB++ reference side of the round-up: matrix -> KiCad layer names, features -> pixels.

Independent of odb2kicad's C++: the records come from valor_oracle.parse_feature_geometry, the
symbols are expanded here from the ODB++ spec (standard symbols + the job's symbols/<name>
folders), units come from each file (INCH: coordinates in inches, symbol sizes in mils; MM:
millimetres and microns; a ``$n name I|M`` override wins), polarity is applied in file order
(a negative feature clears what was drawn before it; a NEGATIVE matrix layer starts dark over the
profile and its positive features clear it), and rotation is the spec's: clockwise,
with orient 4-7 / 9 mirroring X before rotating.

Layer mapping (the harness's own, from the matrix only): BOARD-context SIGNAL / POWER_GROUND /
MIXED rows in row order -> F.Cu, In1.Cu .. In<n-2>.Cu, B.Cu; SOLDER_MASK / SOLDER_PASTE /
SILK_SCREEN above the first copper row -> F.*, below the last -> B.*; DRILL rows -> one "Drill"
class; the step profile -> "Edge.Cuts" (stroked, see EDGE_W); DOCUMENT rows whose name equals a
KiCad layer's name (KiCad's own exports: f.fab, f.courtyard, user.drawings ...) -> that layer.
"""

from __future__ import annotations

import math
import re
from pathlib import Path

import raster
from valor_oracle import DirectOdbOracle, ci_child, parse_feature_geometry

EDGE_W = 0.3  # mm: both sides' outlines are stroked with this width, whatever their own width
COPPER_TYPES = ("SIGNAL", "POWER_GROUND", "MIXED")
SIDE_TYPES = {"SOLDER_MASK": "Mask", "SOLDER_PASTE": "Paste", "SILK_SCREEN": "SilkS"}

# KiCad canonical name for the user-visible names kicad-cli writes as ODB++ layer names
KICAD_DOC_NAMES = {
    "f.fab": "F.Fab", "b.fab": "B.Fab", "f.courtyard": "F.CrtYd", "b.courtyard": "B.CrtYd",
    "f.adhesive": "F.Adhes", "b.adhesive": "B.Adhes", "user.drawings": "Dwgs.User", "user.comments": "Cmts.User",
    "user.eco1": "Eco1.User", "user.eco2": "Eco2.User", "margin": "Margin",
    **{f"user.{i}": f"User.{i}" for i in range(1, 10)},
}


def layer_class(kicad_name: str) -> str:
    """The round-up's metric class for a KiCad layer name (what the PC lane's outbox aggregates by)."""
    if kicad_name in ("F.Cu", "B.Cu"):
        return "cu_outer"
    if kicad_name.startswith("In") and kicad_name.endswith(".Cu"):
        return "cu_inner"
    if kicad_name.endswith(".Mask"):
        return "mask"
    if kicad_name.endswith(".Paste"):
        return "paste"
    if kicad_name.endswith(".SilkS"):
        return "silk"
    if kicad_name == "Edge.Cuts":
        return "edge"
    if kicad_name == "Drill":
        return "drill"
    return "doc"


def map_layers(matrix: list) -> list:
    """matrix rows (DirectOdbOracle._parse_matrix) -> [{'odb': name, 'kicad': name, 'type': t}], board order."""
    allrows = sorted(matrix, key=lambda m: int(m.get("row", 0) or 0))
    rows = [m for m in allrows if m.get("context", "").upper() == "BOARD"]
    cu = [m for m in rows if m.get("type", "").upper() in COPPER_TYPES]
    out = []
    first = int(cu[0]["row"]) if cu else None
    last = int(cu[-1]["row"]) if cu else None
    for i, m in enumerate(cu):
        name = "F.Cu" if i == 0 else "B.Cu" if i == len(cu) - 1 else f"In{i}.Cu"
        out.append({"odb": m["name"], "kicad": name, "type": m.get("type", "").upper(),
                    "negative": m.get("polarity", "").upper() == "NEGATIVE"})
    for m in rows:
        t = m.get("type", "").upper()
        if t in SIDE_TYPES and first is not None:
            side = (m.get("side") or "").upper()
            r = int(m.get("row", 0) or 0)
            top = side == "TOP" or (side not in ("TOP", "BOTTOM") and r < first)
            bot = side == "BOTTOM" or (side not in ("TOP", "BOTTOM") and r > last)
            if top or bot:
                out.append({"odb": m["name"], "kicad": ("F." if top else "B.") + SIDE_TYPES[t], "type": t})
        elif t == "DRILL":
            out.append({"odb": m["name"], "kicad": "Drill", "type": t})
    for m in allrows:  # MISC context too: that is where kicad-cli puts its fab / courtyard / user layers
        if m.get("type", "").upper() == "DOCUMENT" and m.get("name", "").lower() in KICAD_DOC_NAMES:
            out.append({"odb": m["name"], "kicad": KICAD_DOC_NAMES[m["name"].lower()], "type": "DOCUMENT"})
    return out


# ----------------------------------------------------------------------------------------------
# symbols
# ----------------------------------------------------------------------------------------------

_NUM = r"(\d+(?:\.\d+)?)"
_CORNER = r"(?:x(r|c)(\d+(?:\.\d+)?)(?:x(\d+))?)?"  # optional rounded / chamfered corners (+ which corners)


def _corner(m, g: int, scale: float):
    """(radius mm, 'r' | 'c' | None, corner set) from the _CORNER groups starting at group g."""
    kind = m.group(g)
    if not kind:
        return 0.0, None, {1, 2, 3, 4}
    corners = {int(c) for c in m.group(g + 2)} if m.group(g + 2) else {1, 2, 3, 4}
    return float(m.group(g + 1)) * scale, kind, corners


def _rect(w, h):
    return [[(-w / 2, -h / 2), (w / 2, -h / 2), (w / 2, h / 2), (-w / 2, h / 2)]]


def _round_rect(w, h, r, corners, ppm, chamfer=False):
    """Rounded (or chamfered) rectangle; corners: set of 1..4 (1 top-right, 2 top-left, 3 bottom-left, 4 bottom-right)."""
    r = max(0.0, min(r, w / 2, h / 2))
    if r <= 0:
        return _rect(w, h)
    pts = []
    spec = [(1, 1, 1, 0.0), (2, -1, 1, math.pi / 2), (3, -1, -1, math.pi), (4, 1, -1, 3 * math.pi / 2)]
    for k, sx, sy, a0 in spec:
        cx, cy = sx * (w / 2 - r), sy * (h / 2 - r)
        if k not in corners:
            pts.append((sx * w / 2, sy * h / 2))
        elif chamfer:
            pts.append((cx + r * math.cos(a0), cy + r * math.sin(a0)))
            pts.append((cx + r * math.cos(a0 + math.pi / 2), cy + r * math.sin(a0 + math.pi / 2)))
        else:
            pts.extend(raster.arc_points(cx, cy, r, a0, math.pi / 2, ppm))
    return [pts]


def _oval(w, h, ppm):
    if w >= h:
        return [raster.stroke(-(w - h) / 2, 0, (w - h) / 2, 0, h, ppm)]
    return [raster.stroke(0, -(h - w) / 2, 0, (h - w) / 2, w, ppm)]


def _ellipse(w, h, ppm):
    n = raster.seg_count(max(w, h) / 2, ppm)
    return [[(w / 2 * math.cos(2 * math.pi * i / n), h / 2 * math.sin(2 * math.pi * i / n)) for i in range(n)]]


def _thermal_gaps(od, angle, num, gap):
    """Rectangles (gap wide, od long) at the spoke angles, as separate contours."""
    out = []
    for k in range(int(num)):
        a = math.radians(angle) + 2 * math.pi * k / max(int(num), 1)
        ca, sa = math.cos(a), math.sin(a)
        L = od  # from the centre to past the rim
        hw = gap / 2
        pts = [(0, -hw), (L, -hw), (L, hw), (0, hw)]
        out.append([(x * ca - y * sa, x * sa + y * ca) for x, y in pts])
    return out


def standard_symbol(name: str, scale: float, ppm: float):
    """-> list of (value, contours) in mm, symbol-centred, y up; None if the name is not a standard symbol.

    ``scale`` turns the symbol's numbers into mm (0.0254 for mils, 0.001 for microns).
    """
    n = name.lower()
    m = re.fullmatch(r"r" + _NUM, n)
    if m:
        return [(1, [raster.circle(0, 0, float(m.group(1)) * scale / 2, ppm)])]
    m = re.fullmatch(r"s" + _NUM, n)
    if m:
        s = float(m.group(1)) * scale
        return [(1, _rect(s, s))]
    m = re.fullmatch(r"rect" + _NUM + "x" + _NUM + r"(?:x(r|c)" + _NUM + r"(?:x(\d+))?)?", n)
    if m:
        w, h = float(m.group(1)) * scale, float(m.group(2)) * scale
        if not m.group(3):
            return [(1, _rect(w, h))]
        corners = {int(c) for c in m.group(5)} if m.group(5) else {1, 2, 3, 4}
        return [(1, _round_rect(w, h, float(m.group(4)) * scale, corners, ppm, chamfer=m.group(3) == "c"))]
    m = re.fullmatch(r"oval" + _NUM + "x" + _NUM, n)
    if m:
        return [(1, _oval(float(m.group(1)) * scale, float(m.group(2)) * scale, ppm))]
    m = re.fullmatch(r"oval_h" + _NUM + "x" + _NUM, n)
    if m:
        return [(1, _oval(float(m.group(1)) * scale, float(m.group(2)) * scale, ppm))]
    m = re.fullmatch(r"el" + _NUM + "x" + _NUM, n)
    if m:
        return [(1, _ellipse(float(m.group(1)) * scale, float(m.group(2)) * scale, ppm))]
    m = re.fullmatch(r"di" + _NUM + "x" + _NUM, n)
    if m:
        w, h = float(m.group(1)) * scale, float(m.group(2)) * scale
        return [(1, [[(w / 2, 0), (0, h / 2), (-w / 2, 0), (0, -h / 2)]])]
    m = re.fullmatch(r"oct" + _NUM + "x" + _NUM + "x" + _NUM, n)
    if m:
        w, h, c = (float(v) * scale for v in m.groups())
        return [(1, _round_rect(w, h, c, {1, 2, 3, 4}, ppm, chamfer=True))]
    m = re.fullmatch(r"donut_r" + _NUM + "x" + _NUM, n)
    if m:
        od, idd = float(m.group(1)) * scale, float(m.group(2)) * scale
        return [(1, [raster.circle(0, 0, od / 2, ppm), raster.circle(0, 0, idd / 2, ppm)])]
    m = re.fullmatch(r"donut_s" + _NUM + "x" + _NUM + _CORNER, n)
    if m:
        od, idd = float(m.group(1)) * scale, float(m.group(2)) * scale
        r, kind, corners = _corner(m, 3, scale)
        if not kind:
            return [(1, _rect(od, od) + _rect(idd, idd))]
        rin = max(0.0, r - (od - idd) / 2)
        return [(1, _round_rect(od, od, r, corners, ppm, kind == "c") + _round_rect(idd, idd, rin, corners, ppm, kind == "c"))]
    m = re.fullmatch(r"donut_rc" + _NUM + "x" + _NUM + "x" + _NUM + _CORNER, n)
    if m:
        w, h, lw = (float(m.group(i)) * scale for i in (1, 2, 3))
        r, kind, corners = _corner(m, 4, scale)
        if not kind:
            return [(1, _rect(w, h) + _rect(w - 2 * lw, h - 2 * lw))]
        rin = max(0.0, r - lw)
        return [(1, _round_rect(w, h, r, corners, ppm, kind == "c") + _round_rect(w - 2 * lw, h - 2 * lw, rin, corners, ppm, kind == "c"))]
    m = re.fullmatch(r"donut_o" + _NUM + "x" + _NUM + "x" + _NUM, n)
    if m:
        w, h, lw = (float(v) * scale for v in m.groups())
        return [(1, _oval(w, h, ppm) + _oval(w - 2 * lw, h - 2 * lw, ppm))]
    m = re.fullmatch(r"hex_l" + _NUM + "x" + _NUM + "x" + _NUM, n)
    if m:
        w, h, r = (float(v) * scale for v in m.groups())
        return [(1, [[(w / 2, 0), (w / 2 - r, h / 2), (-w / 2 + r, h / 2), (-w / 2, 0), (-w / 2 + r, -h / 2), (w / 2 - r, -h / 2)]])]
    m = re.fullmatch(r"hex_s" + _NUM + "x" + _NUM + "x" + _NUM, n)
    if m:
        w, h, r = (float(v) * scale for v in m.groups())
        return [(1, [[(0, h / 2), (w / 2, h / 2 - r), (w / 2, -h / 2 + r), (0, -h / 2), (-w / 2, -h / 2 + r), (-w / 2, h / 2 - r)]])]
    m = re.fullmatch(r"tri" + _NUM + "x" + _NUM, n)
    if m:
        b, h = float(m.group(1)) * scale, float(m.group(2)) * scale
        return [(1, [[(-b / 2, -h / 2), (b / 2, -h / 2), (0, h / 2)]])]
    m = re.fullmatch(r"bf(r|s)" + _NUM, n)
    if m:
        d = float(m.group(2)) * scale
        q1 = raster.arc_points(0, 0, d / 2, 0, math.pi / 2, ppm) if m.group(1) == "r" else [(d / 2, 0), (d / 2, d / 2), (0, d / 2)]
        q3 = [(-x, -y) for x, y in q1]
        return [(1, [[(0, 0)] + q1, [(0, 0)] + q3])]
    m = re.fullmatch(r"(thr|ths|s_ths|s_tho|s_thr|sr_ths)" + _NUM + "x" + _NUM + "x" + _NUM + "x" + _NUM + "x" + _NUM, n)
    if m:
        kind = m.group(1)
        od, idd = float(m.group(2)) * scale, float(m.group(3)) * scale
        ang, num, gap = float(m.group(4)), float(m.group(5)), float(m.group(6)) * scale
        ring = [raster.circle(0, 0, od / 2, ppm), raster.circle(0, 0, idd / 2, ppm)] if kind in ("thr", "ths") \
            else _rect(od, od) + _rect(idd, idd)
        return [(1, ring), (0, _thermal_gaps(od, ang, num, gap))]
    m = re.fullmatch(r"rc_th[so]" + _NUM + "x" + _NUM + "x" + _NUM + "x" + _NUM + "x" + _NUM + "x" + _NUM, n)
    if m:
        w, h, ang, num, gap, air = (float(v) for v in m.groups())
        w, h, gap, air = w * scale, h * scale, gap * scale, air * scale
        return [(1, _rect(w, h) + _rect(w - 2 * air, h - 2 * air)), (0, _thermal_gaps(max(w, h), ang, num, gap))]
    m = re.fullmatch(r"hole" + _NUM + r"(?:x.*)?", n)
    if m:
        return [(1, [raster.circle(0, 0, float(m.group(1)) * scale / 2, ppm)])]
    if re.fullmatch(r"null\d*", n):
        return []
    return None


def _line_width(name: str, scale: float):
    """(width mm, square caps) for a line/arc symbol."""
    n = name.lower()
    m = re.fullmatch(r"(r|s)" + _NUM, n)
    if m:
        return float(m.group(2)) * scale, m.group(1) == "s"
    m = re.findall(r"\d+(?:\.\d+)?", n)
    return (float(min(m, key=float)) * scale if m else 0.0), False


# ----------------------------------------------------------------------------------------------
# a job
# ----------------------------------------------------------------------------------------------


class OdbJob:
    """One ODB++ job (a folder), its first step by default, rendered layer by layer."""

    def __init__(self, root: Path, step: str | None = None):
        self.root = Path(root)
        self.oracle = DirectOdbOracle(str(self.root))
        self.matrix = self.oracle._parse_matrix()
        steps = sorted(p.name for p in (self.root / "steps").iterdir() if p.is_dir()) if (self.root / "steps").is_dir() else []
        if not steps:
            raise ValueError("no steps/ in the ODB++ job")
        self.step = step if step in steps else self._pick_step(steps)
        self.step_dir = self.root / "steps" / self.step
        self.layers = map_layers(self.matrix)
        self.stats = {"unsupported_symbols": {}, "text": 0, "barcodes": 0, "skipped_lines": 0, "features": 0}
        self._sym_cache: dict = {}

    def _pick_step(self, steps: list) -> str:
        """The step with the most layer folders (panel steps usually hold few)."""
        def n(st):
            d = self.root / "steps" / st / "layers"
            return len(list(d.iterdir())) if d.is_dir() else 0
        return max(steps, key=n)

    # -- geometry ------------------------------------------------------------------------------
    def profile_contours(self, ppm: float = 50.0) -> list:
        """Profile surface contours in mm (list of point lists)."""
        geo = parse_feature_geometry(self.step_dir / "profile")
        s = 25.4 if geo["units"] == "INCH" else 1.0
        out = []
        for rec in geo["records"]:
            if rec[0] == "S":
                for _hole, c in rec[2]:
                    out.append(self._contour(c, s, ppm))
        return out

    def profile_bbox(self):
        cs = self.profile_contours()
        return raster.bbox(cs) if cs else None

    @staticmethod
    def _contour(c: list, s: float, ppm: float) -> list:
        pts = []
        cur = None
        for seg in c:
            if seg[0] == "S":
                cur = (seg[1] * s, seg[2] * s)
                pts.append(cur)
            else:
                _, xe, ye, xc, yc, cw = seg
                xe, ye, xc, yc = xe * s, ye * s, xc * s, yc * s
                if cur is None:
                    cur = (xe, ye)
                r = math.hypot(cur[0] - xc, cur[1] - yc)
                a0 = math.atan2(cur[1] - yc, cur[0] - xc)
                a1 = math.atan2(ye - yc, xe - xc)
                if math.hypot(xe - cur[0], ye - cur[1]) < 1e-9:
                    sw = -2 * math.pi if cw else 2 * math.pi
                else:
                    sw = a1 - a0
                    if cw and sw > 0:
                        sw -= 2 * math.pi
                    elif not cw and sw < 0:
                        sw += 2 * math.pi
                pts.extend(raster.arc_points(xc, yc, r, a0, sw, ppm)[1:])
                cur = (xe, ye)
        return pts

    def _user_symbol(self, name: str):
        if name in self._sym_cache:
            return self._sym_cache[name]
        d = ci_child(self.root / "symbols", name) if (self.root / "symbols").is_dir() else None
        geo = parse_feature_geometry(d / "features") if d and (d / "features").is_file() else None
        self._sym_cache[name] = geo
        return geo

    def _symbol_ops(self, name: str, override, file_units: str, ppm: float, depth: int = 0):
        units = {"I": "INCH", "M": "MM"}.get(override or "", file_units)
        scale = 0.0254 if units == "INCH" else 0.001
        ops = standard_symbol(name, scale, ppm)
        if ops is not None:
            return ops
        geo = self._user_symbol(name) if depth < 4 else None
        if geo is None:
            self.stats["unsupported_symbols"][name] = self.stats["unsupported_symbols"].get(name, 0) + 1
            return []
        return self.record_ops(geo, ppm, depth + 1)

    def record_ops(self, geo: dict, ppm: float, depth: int = 0) -> list:
        """A parsed features file -> [(value, contours mm)] in drawing order."""
        s = 25.4 if geo["units"] == "INCH" else 1.0
        syms = geo["symbols"]
        units = geo["units"]
        ops: list = []
        if depth == 0:
            self.stats["skipped_lines"] += geo.get("skipped", 0)
        for rec in geo["records"]:
            t = rec[0]
            if depth == 0:
                self.stats["features"] += 1
            if t == "P":
                _, x, y, si, pol, orient, ang = rec
                name, ov = syms.get(si, ("", None))
                sym = self._symbol_ops(name, ov, units, ppm, depth)
                mirror = orient in (4, 5, 6, 7, 9)
                rot = ang if orient in (8, 9) else 90.0 * (orient % 4)
                a = math.radians(rot)
                ca, sa = math.cos(a), math.sin(a)
                X, Y = x * s, y * s

                def tf(px, py, X=X, Y=Y, ca=ca, sa=sa, mirror=mirror):
                    if mirror:
                        px = -px
                    return (X + px * ca + py * sa, Y - px * sa + py * ca)  # clockwise rotation, y up
                for val, cs in sym:
                    v = val if pol else 0
                    if not pol and val == 0:
                        continue
                    ops.append((v, raster.xform(cs, tf)))
            elif t == "L":
                _, xs, ys, xe, ye, si, pol = rec
                name, ov = syms.get(si, ("", None))
                sc = 0.0254 if {"I": "INCH", "M": "MM"}.get(ov or "", units) == "INCH" else 0.001
                w, sq = _line_width(name, sc)
                ops.append((1 if pol else 0, [raster.stroke(xs * s, ys * s, xe * s, ye * s, w, ppm, sq)]))
            elif t == "A":
                _, xs, ys, xe, ye, xc, yc, si, pol, cw = rec
                name, ov = syms.get(si, ("", None))
                sc = 0.0254 if {"I": "INCH", "M": "MM"}.get(ov or "", units) == "INCH" else 0.001
                w, sq = _line_width(name, sc)
                ops.append((1 if pol else 0, raster.arc_stroke(xs * s, ys * s, xe * s, ye * s, xc * s, yc * s, cw, w, ppm, sq)))
            elif t == "S":
                _, pol, contours = rec
                cs = [self._contour(c, s, ppm) for _hole, c in contours]
                ops.append((1 if pol else 0, [c for c in cs if len(c) >= 3]))
            elif t == "T":
                self.stats["text"] += 1
            elif t == "B":
                self.stats["barcodes"] += 1
        return ops

    def layer_dir(self, odb_name: str):
        return ci_child(self.step_dir / "layers", odb_name)

    def render(self, kicad_name: str, view: raster.View) -> tuple:
        """-> (Bitmap, [odb layer names drawn]) for one KiCad layer name (Drill is every drill row)."""
        bm = raster.Bitmap(view.w, view.h)
        drawn = []
        if kicad_name == "Edge.Cuts":
            for c in self.profile_contours():
                n = len(c)
                for i in range(n):
                    (x0, y0), (x1, y1) = c[i], c[(i + 1) % n]
                    bm.fill(view.px([raster.stroke(x0, y0, x1, y1, EDGE_W, view.ppm)]), 1)
            return bm, ["profile"]
        for lay in self.layers:
            if lay["kicad"] != kicad_name:
                continue
            d = self.layer_dir(lay["odb"])
            if d is None:
                continue
            geo = parse_feature_geometry(d / "features")
            neg = lay.get("negative", False)
            if neg:  # a NEGATIVE layer: the board is dark, its features clear it
                bm.fill(view.px([c for c in self.profile_contours(view.ppm) if len(c) >= 3]), 1)
            for val, cs in self.record_ops(geo, view.ppm):
                if cs:
                    bm.fill(view.px(cs), 1 - val if neg else val)
            drawn.append(lay["odb"])
        return bm, drawn
