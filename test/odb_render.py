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


def _thermal_gaps(L, angle, num, gap):
    """Rectangles (gap wide, L long from the centre) at the spoke angles, as separate contours.

    L only just clears the symbol's rim: these are clearances, and a longer bar would clear the
    copper of whatever lies next to the thermal on its layer.
    """
    out = []
    for k in range(int(num)):
        a = math.radians(angle) + 2 * math.pi * k / max(int(num), 1)
        ca, sa = math.cos(a), math.sin(a)
        hw = gap / 2
        pts = [(0, -hw), (L, -hw), (L, hw), (0, hw)]
        out.append([(x * ca - y * sa, x * sa + y * ca) for x, y in pts])
    return out


def _bar_ops(points):
    """A clearance op from a polygon."""
    return (0, [points])


def _round_end_pieces(segments, lw, rays, reach, ppm):
    """Round-ended copper along a centre line made of primitives, broken near the spokes.

    ``segments``: ("L", x0, y0, x1, y1) or ("A", cx, cy, r, a0, sweep). A point of the centre line is
    cut when it lies on a spoke ray's forward side within ``reach`` of it. The kept runs are drawn as
    strokes ``lw`` wide with round ends (a disc swept along the run), so a run that crosses from one
    primitive into the next is still one piece of copper.
    """
    def covered(x, y):
        for dx, dy in rays:
            if x * dx + y * dy >= 0 and abs(dx * y - dy * x) < reach:
                return True
        return False

    ops = []
    for seg in segments:
        if seg[0] == "L":
            _, x0, y0, x1, y1 = seg
            pt = lambda u, x0=x0, y0=y0, x1=x1, y1=y1: (x0 + (x1 - x0) * u, y0 + (y1 - y0) * u)  # noqa: E731
        else:
            _, cx, cy, r, a0, sw = seg
            pt = lambda u, cx=cx, cy=cy, r=r, a0=a0, sw=sw: (cx + r * math.cos(a0 + sw * u), cy + r * math.sin(a0 + sw * u))  # noqa: E731
        n = 2000
        keep = [not covered(*pt(i / n)) for i in range(n + 1)]
        i = 0
        while i <= n:
            if not keep[i]:
                i += 1
                continue
            j = i
            while j + 1 <= n and keep[j + 1]:
                j += 1
            u0, u1 = i / n, j / n
            # pull each end out to the cut's edge (bisection on the primitive's parameter)
            for end in (0, 1):
                if (end == 0 and i > 0) or (end == 1 and j < n):
                    lo, hi = (u0, (i - 1) / n) if end == 0 else (u1, (j + 1) / n)
                    for _ in range(40):
                        mid = (lo + hi) / 2
                        if covered(*pt(mid)):
                            hi = mid
                        else:
                            lo = mid
                    if end == 0:
                        u0 = lo
                    else:
                        u1 = lo
            if seg[0] == "L":
                (xa, ya), (xb, yb) = pt(u0), pt(u1)
                ops.append((1, [raster.stroke(xa, ya, xb, yb, lw, ppm)]))
            else:
                _, cx, cy, r, a0, sw = seg
                (xa, ya), (xb, yb) = pt(u0), pt(u1)
                if u1 - u0 >= 1 - 1e-12:
                    ops.append((1, raster.arc_stroke(xa, ya, xa, ya, cx, cy, sw < 0, lw, ppm)))
                else:
                    ops.append((1, raster.arc_stroke(xa, ya, xb, yb, cx, cy, sw < 0, lw, ppm)))
            i = j + 1
    return ops


def _spoke_dirs(angle, num):
    return [(math.cos(math.radians(angle + 360.0 * k / num)), math.sin(math.radians(angle + 360.0 * k / num)))
            for k in range(int(num))]


def _is_corner_angle(deg):
    return abs((deg % 90.0) - 45.0) < 1e-6


def _rect_thermal(w, h, ang, num, gap, air, r, kind, corners, ppm, open_corners):
    """Rectangle (or square) thermal ops. A spoke at 45 deg (+90 n) is a gap through the corner, or with
    open corners the corner square taken out; any other spoke a gap from the centre outwards."""
    if r > 0:
        ring = _round_rect(w, h, r, corners, ppm, kind == "c") + _round_rect(w - 2 * air, h - 2 * air, max(0.0, r - air), corners, ppm, kind == "c")
    else:
        ring = _rect(w, h) + _rect(w - 2 * air, h - 2 * air)
    ops = [(1, ring)]
    n = int(num)
    for k in range(n):
        deg = ang + 360.0 * k / n
        dx, dy = math.cos(math.radians(deg)), math.sin(math.radians(deg))
        if not _is_corner_angle(deg):
            L, hw = math.hypot(w, h) / 2 + gap, gap / 2
            ops.append(_bar_ops([(-dy * hw, dx * hw), (dx * L - dy * hw, dy * L + dx * hw), (dx * L + dy * hw, dy * L - dx * hw), (dy * hw, -dx * hw)]))
            continue
        sx, sy = (1 if dx > 0 else -1), (1 if dy > 0 else -1)
        if open_corners:
            x0, y0 = w / 2 - air - gap, h / 2 - air - gap
            x1, y1 = w / 2 + gap, h / 2 + gap
            ops.append(_bar_ops([(sx * x0, sy * y0), (sx * x1, sy * y0), (sx * x1, sy * y1), (sx * x0, sy * y1)]))
        else:
            xi, yi = sx * (w / 2 - air), sy * (h / 2 - air)  # the inner corner; the outer one is on the same 45 deg line
            back = max(0.0, min(gap, min(w, h) / 2 - air))
            out = air * 1.5 + gap  # past the outer corner, which is air * 1.414 further along
            ops.append((0, [raster.stroke(xi - dx * (back - gap / 2), yi - dy * (back - gap / 2), xi + dx * out, yi + dy * out, gap, ppm, square=True)]))
    return ops


def _fillet(pts, radii, ppm):
    """Round polygon corners (counter-clockwise points) by radii[i]: arcs tangent to both edges."""
    out = []
    n = len(pts)
    for i in range(n):
        bx, by = pts[i]
        ax, ay = pts[i - 1]
        cx, cy = pts[(i + 1) % n]
        r = radii[i]
        e1, e2 = math.hypot(ax - bx, ay - by), math.hypot(cx - bx, cy - by)
        if r <= 0 or e1 == 0 or e2 == 0:
            out.append((bx, by))
            continue
        a_in, a_out = math.atan2(ay - by, ax - bx), math.atan2(cy - by, cx - bx)
        half = abs((a_out - a_in + math.pi) % (2 * math.pi) - math.pi) / 2  # half the angle between the edges
        if half < 1e-9 or half > math.pi / 2 - 1e-9:
            out.append((bx, by))
            continue
        t = min(r / math.tan(half), e1 / 2, e2 / 2)
        r = t * math.tan(half)
        bis = math.atan2(math.sin(a_in) + math.sin(a_out), math.cos(a_in) + math.cos(a_out))
        d = r / math.sin(half)
        ox, oy = bx + d * math.cos(bis), by + d * math.sin(bis)
        p1 = (bx + t * math.cos(a_in), by + t * math.sin(a_in))
        p2 = (bx + t * math.cos(a_out), by + t * math.sin(a_out))
        s0 = math.atan2(p1[1] - oy, p1[0] - ox)
        sw = (math.atan2(p2[1] - oy, p2[0] - ox) - s0 + math.pi) % (2 * math.pi) - math.pi
        out.extend(raster.arc_points(ox, oy, r, s0, sw, ppm))
    return out


def _corner_radii(pts, ra, ro):
    """ra where a counter-clockwise polygon's inside angle is 90 deg or less, else ro."""
    out = []
    n = len(pts)
    for i in range(n):
        (ax, ay), (bx, by), (cx, cy) = pts[i - 1], pts[i], pts[(i + 1) % n]
        turn = math.atan2((bx - ax) * (cy - by) - (by - ay) * (cx - bx), (bx - ax) * (cx - bx) + (by - ay) * (cy - by))
        inside = math.pi - turn  # a left turn of `turn` leaves pi - turn inside
        out.append(ra if inside <= math.pi / 2 + 1e-9 else ro)
    return out


def _inner_corner(kx, ky, sx, sy, r, ppm):
    """Material a concave corner of radius r adds where an edge along sx meets one along sy at (kx, ky)."""
    ox, oy = kx + sx * r, ky + sy * r
    a0 = math.atan2(-sy, 0.0)
    sw = (math.atan2(0.0, -sx) - a0 + math.pi) % (2 * math.pi) - math.pi
    return [(kx, ky)] + raster.arc_points(ox, oy, r, a0, sw, ppm)


def _span(x0, y0, x1, y1, w, round_ends, ppm):
    """A line w wide whose full length, ends included, runs from (x0, y0) to (x1, y1)."""
    L = math.hypot(x1 - x0, y1 - y0)
    if L < 1e-12:
        return raster.circle(x0, y0, w / 2, ppm)
    ux, uy = (x1 - x0) / L, (y1 - y0) / L
    if round_ends:
        if L <= w:
            return raster.circle((x0 + x1) / 2, (y0 + y1) / 2, w / 2, ppm)
        return raster.stroke(x0 + ux * w / 2, y0 + uy * w / 2, x1 - ux * w / 2, y1 - uy * w / 2, w, ppm)
    nx, ny = -uy * w / 2, ux * w / 2
    return [(x0 - nx, y0 - ny), (x1 - nx, y1 - ny), (x1 + nx, y1 + ny), (x0 + nx, y0 + ny)]


_OPT = r"(?:x(?:{})?(\d+(?:\.\d+)?))?"


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
    if m:  # half oval: square left end, round right end
        w, h = float(m.group(1)) * scale, float(m.group(2)) * scale
        if w < h / 2:
            return [(1, _rect(w, h))]
        return [(1, [[(-w / 2, -h / 2), (w / 2 - h / 2, -h / 2), (w / 2 - h / 2, h / 2), (-w / 2, h / 2)]]),
                (1, [raster.circle(w / 2 - h / 2, 0, h / 2, ppm)])]
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
    T5 = _NUM + "x" + _NUM + "x" + _NUM + "x" + _NUM + "x" + _NUM
    m = re.fullmatch(r"thr" + T5, n)
    if m:  # round thermal, rounded ends: arcs of the ring, each end a half disc clear of the gap
        od, idd = float(m.group(1)) * scale, float(m.group(2)) * scale
        ang, num, gap = float(m.group(3)), int(round(float(m.group(4)))), float(m.group(5)) * scale
        rc, lw = (od + idd) / 4, (od - idd) / 2
        if num <= 0:
            return [(1, [raster.circle(0, 0, od / 2, ppm), raster.circle(0, 0, idd / 2, ppm)])]
        d = math.asin(min(1.0, (gap + lw) / 2 / rc)) if rc > 0 else math.pi
        ops = []
        for k in range(num):
            b0 = math.radians(ang + 360.0 * k / num) + d
            b1 = math.radians(ang + 360.0 * (k + 1) / num) - d
            if b1 - b0 <= 1e-9:
                continue
            ops.append((1, raster.arc_stroke(rc * math.cos(b0), rc * math.sin(b0), rc * math.cos(b1), rc * math.sin(b1), 0, 0, False, lw, ppm)))
        return ops
    m = re.fullmatch(r"s_thr" + T5, n)
    if m:  # line thermal: the square's sides as round-ended lines (os - is) / 2 wide
        os_, is_ = float(m.group(1)) * scale, float(m.group(2)) * scale
        ang, num, gap = float(m.group(3)), int(round(float(m.group(4)))), float(m.group(5)) * scale
        c, lw = (os_ + is_) / 4, (os_ - is_) / 2
        sides = [("L", c, -c, c, c), ("L", c, c, -c, c), ("L", -c, c, -c, -c), ("L", -c, -c, c, -c)]
        if num <= 0:
            return [(1, _rect(os_, os_) + _rect(is_, is_))]
        return _round_end_pieces(sides, lw, _spoke_dirs(ang, num), gap / 2 + lw / 2, ppm)
    m = re.fullmatch(r"(ths|s_ths|sr_ths)" + T5 + _CORNER, n)
    if m:
        kind = m.group(1)
        od, idd = float(m.group(2)) * scale, float(m.group(3)) * scale
        ang, num, gap = float(m.group(4)), float(m.group(5)), float(m.group(6)) * scale
        if kind == "ths":
            ring = [raster.circle(0, 0, od / 2, ppm), raster.circle(0, 0, idd / 2, ppm)]
        else:
            r, ck, corners = _corner(m, 7, scale)
            outer = _round_rect(od, od, r, corners, ppm, ck == "c") if ck else _rect(od, od)
            if kind == "sr_ths":
                inner = [raster.circle(0, 0, idd / 2, ppm)]
            else:
                inner = _round_rect(idd, idd, max(0.0, r - (od - idd) / 2), corners, ppm, ck == "c") if ck else _rect(idd, idd)
            ring = outer + inner
        return [(1, ring), (0, _thermal_gaps(od * 0.75 + gap, ang, num, gap))]  # a square's corner is od / 1.414 out
    m = re.fullmatch(r"s_tho" + T5, n)
    if m:  # square thermal, open corners
        od, idd = float(m.group(1)) * scale, float(m.group(2)) * scale
        return _rect_thermal(od, od, float(m.group(3)), int(round(float(m.group(4)))), float(m.group(5)) * scale,
                             (od - idd) / 2, 0.0, None, {1, 2, 3, 4}, ppm, True)
    m = re.fullmatch(r"rc_th(s|o)" + T5 + "x" + _NUM + _CORNER, n)
    if m:
        w, h = float(m.group(2)) * scale, float(m.group(3)) * scale
        ang, num, gap, air = float(m.group(4)), int(round(float(m.group(5)))), float(m.group(6)) * scale, float(m.group(7)) * scale
        r, ck, corners = _corner(m, 8, scale)
        return _rect_thermal(w, h, ang, num, gap, air, r, ck, corners, ppm, m.group(1) == "o")
    m = re.fullmatch(r"(o_ths|oblong_ths)" + T5 + "x" + _NUM + r"(?:x(r|s))?", n)
    if m:  # oval thermal; oblong with r: round copper ends
        w, h = float(m.group(2)) * scale, float(m.group(3)) * scale
        ang, num, gap, lw = float(m.group(4)), int(round(float(m.group(5)))), float(m.group(6)) * scale, float(m.group(7)) * scale
        if m.group(1) == "oblong_ths" and m.group(8) == "r" and num > 0:
            a, b = (w - lw) / 2, (h - lw) / 2
            if a >= b:
                e, rho = a - b, b
                path = [("A", e, 0, rho, -math.pi / 2, math.pi), ("L", e, rho, -e, rho),
                        ("A", -e, 0, rho, math.pi / 2, math.pi), ("L", -e, -rho, e, -rho)]
            else:
                e, rho = b - a, a
                path = [("L", rho, -e, rho, e), ("A", 0, e, rho, 0, math.pi),
                        ("L", -rho, e, -rho, -e), ("A", 0, -e, rho, math.pi, math.pi)]
            return _round_end_pieces(path, lw, _spoke_dirs(ang, num), gap / 2 + lw / 2, ppm)
        return [(1, _oval(w, h, ppm) + _oval(w - 2 * lw, h - 2 * lw, ppm)), (0, _thermal_gaps(math.hypot(w, h) / 2 + gap, ang, num, gap))]
    m = re.fullmatch(r"moire" + _NUM + "x" + _NUM + "x" + _NUM + "x" + _NUM + "x" + _NUM + "x" + _NUM, n)
    if m:  # rings rw wide, the i-th reaching i * (rg + rw / 2); a crosshair lw wide, ll long, at la
        rw, rg, lw, ll = (float(m.group(i)) * scale for i in (1, 2, 4, 5))
        nr, la = int(round(float(m.group(3)))), math.radians(float(m.group(6)))
        ops = []
        for i in range(1, nr + 1):
            ro = i * (rg + rw / 2)
            ri = ro - rw
            ops.append((1, [raster.circle(0, 0, ro, ppm)] + ([raster.circle(0, 0, ri, ppm)] if ri > 1e-9 else [])))
        for a in (la, la + math.pi / 2):
            c, sn = math.cos(a) * ll / 2, math.sin(a) * ll / 2
            ops.append((1, [_span(-c, -sn, c, sn, lw, False, ppm)]))
        return ops
    RA, RO = _OPT.format("ra"), _OPT.format("ro")
    m = re.fullmatch(r"(hplate|rhplate)" + _NUM + "x" + _NUM + "x" + _NUM + RA + RO, n)
    if m:
        w, h, c = (float(m.group(i)) * scale for i in (2, 3, 4))
        ra, ro = (float(m.group(i)) * scale if m.group(i) else 0.0 for i in (5, 6))
        if m.group(1) == "hplate":
            pts = [(-w / 2, -h / 2), (w / 2 - c, -h / 2), (w / 2, 0), (w / 2 - c, h / 2), (-w / 2, h / 2)]
        else:
            pts = [(-w / 2, -h / 2), (w / 2, -h / 2), (w / 2 - c, 0), (w / 2, h / 2), (-w / 2, h / 2)]
        pts = [p for i, p in enumerate(pts) if math.dist(p, pts[i - 1]) > 1e-12]
        return [(1, [_fillet(pts, _corner_radii(pts, ra, ro), ppm)])]
    m = re.fullmatch(r"fhplate" + _NUM + "x" + _NUM + "x" + _NUM + "x" + _NUM + RA + RO, n)
    if m:
        w, h, vc, hc = (float(m.group(i)) * scale for i in (1, 2, 3, 4))
        ra, ro = (float(m.group(i)) * scale if m.group(i) else 0.0 for i in (5, 6))
        pts = [(-w / 2, -h / 2), (w / 2 - hc, -h / 2), (w / 2, -h / 2 + vc), (w / 2, h / 2 - vc), (w / 2 - hc, h / 2), (-w / 2, h / 2)]
        pts = [p for i, p in enumerate(pts) if math.dist(p, pts[i - 1]) > 1e-12]
        return [(1, [_fillet(pts, _corner_radii(pts, ra, ro), ppm)])]
    m = re.fullmatch(r"radhplate" + _NUM + "x" + _NUM + "x" + _NUM + RA, n)
    if m:  # a half-circle bite ms across in the middle of the right side; ra on the rectangle's corners
        w, h, ms = (float(m.group(i)) * scale for i in (1, 2, 3))
        ra = float(m.group(4)) * scale if m.group(4) else 0.0
        r = min(ms / 2, h / 2)
        bite = raster.arc_points(w / 2, 0, r, -math.pi / 2, -math.pi, ppm)  # clockwise: into the plate
        pts = [(-w / 2, -h / 2), (w / 2, -h / 2)] + bite + [(w / 2, h / 2), (-w / 2, h / 2)]
        radii = [ra, ra] + [0.0] * len(bite) + [ra, ra]
        return [(1, [_fillet(pts, radii, ppm)])]
    m = re.fullmatch(r"dshape" + _NUM + "x" + _NUM + "x" + _NUM + RA, n)
    if m:  # D shape: the right side an arc through (w/2 - r, -h/2), (w/2, 0), (w/2 - r, h/2)
        w, h, rel = (float(m.group(i)) * scale for i in (1, 2, 3))
        ra = float(m.group(4)) * scale if m.group(4) else 0.0
        if rel <= 0:
            return [(1, [_fillet(_rect(w, h)[0], [ra] * 4, ppm)])]
        R = (rel * rel + h * h / 4) / (2 * rel)
        phi = math.atan2(h / 2, R - rel)
        arc = raster.arc_points(w / 2 - R, 0, R, -phi, 2 * phi, ppm)
        pts = [(-w / 2, -h / 2)] + arc + [(-w / 2, h / 2)]
        return [(1, [_fillet(pts, [ra] + [0.0] * len(arc) + [ra], ppm)])]
    m = re.fullmatch(r"cross" + _NUM + "x" + _NUM + "x" + _NUM + "x" + _NUM + "x" + _NUM + "x" + _NUM + r"x(r|s)(?:x?(?:ra)?" + _NUM + ")?", n)
    if m:
        w, h, hs, vs = (float(m.group(i)) * scale for i in (1, 2, 3, 4))
        xv, yh = -w / 2 + float(m.group(5)) / 100 * w, -h / 2 + float(m.group(6)) / 100 * h
        rnd = m.group(7) == "r"
        ra = float(m.group(8)) * scale if m.group(8) else 0.0
        ops = [(1, [_span(-w / 2, yh, w / 2, yh, hs, rnd, ppm)]), (1, [_span(xv, -h / 2, xv, h / 2, vs, rnd, ppm)])]
        if ra > 0:
            for sx in (-1, 1):
                for sy in (-1, 1):
                    kx, ky = xv + sx * vs / 2, yh + sy * hs / 2
                    if abs(kx + sx * ra) <= w / 2 - (hs / 2 if rnd else 0) and abs(ky + sy * ra) <= h / 2 - (vs / 2 if rnd else 0):
                        ops.append((1, [_inner_corner(kx, ky, sx, sy, ra, ppm)]))
        return ops
    m = re.fullmatch(r"dogbone" + _NUM + "x" + _NUM + "x" + _NUM + "x" + _NUM + "x" + _NUM + r"x(r|s)(?:x?(?:ra)?" + _NUM + ")?", n)
    if m:
        w, h, hs, vs = (float(m.group(i)) * scale for i in (1, 2, 3, 4))
        xv = -w / 2 + float(m.group(5)) / 100 * w
        rnd = m.group(6) == "r"
        ra = float(m.group(7)) * scale if m.group(7) else 0.0
        ops = [(1, [_span(-w / 2, sy * (h / 2 - hs / 2), w / 2, sy * (h / 2 - hs / 2), hs, rnd, ppm)]) for sy in (-1, 1)]
        ops.append((1, [[(xv - vs / 2, -h / 2 + hs / 2), (xv + vs / 2, -h / 2 + hs / 2), (xv + vs / 2, h / 2 - hs / 2), (xv - vs / 2, h / 2 - hs / 2)]]))
        if ra > 0:
            for sx in (-1, 1):
                for sy in (-1, 1):
                    kx, ky = xv + sx * vs / 2, sy * (h / 2 - hs)
                    if abs(kx + sx * ra) <= w / 2 - (hs / 2 if rnd else 0):
                        ops.append((1, [_inner_corner(kx, ky, sx, -sy, ra, ppm)]))
        return ops
    m = re.fullmatch(r"dpack" + _NUM + "x" + _NUM + "x" + _NUM + "x" + _NUM + "x" + _NUM + "x" + _NUM + _OPT.format("ra"), n)
    if m:
        w, h, hg, vg = (float(m.group(i)) * scale for i in (1, 2, 3, 4))
        hn, vn = max(1, int(round(float(m.group(5))))), max(1, int(round(float(m.group(6)))))
        ra = float(m.group(7)) * scale if m.group(7) else 0.0
        pw, ph = (w - hg * (hn - 1)) / hn, (h - vg * (vn - 1)) / vn
        if pw <= 0 or ph <= 0:
            return []
        ops = []
        for i in range(hn):
            for j in range(vn):
                cx, cy = -w / 2 + pw / 2 + i * (pw + hg), -h / 2 + ph / 2 + j * (ph + vg)
                pad = _round_rect(pw, ph, ra, {1, 2, 3, 4}, ppm) if ra > 0 else _rect(pw, ph)
                ops.append((1, [[(cx + x, cy + y) for x, y in c] for c in pad]))
        return ops
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
