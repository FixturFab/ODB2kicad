"""KiCad side of the round-up: a .kicad_pcb -> pixels per layer, with raster.py.

Draws what a fabrication plot of the board would hold: pads (rect, roundrect, circle, oval,
trapezoid, chamfered_rect as roundrect, custom primitives), segments, arcs, vias, zone fills
(filled_polygon; a zone with no fill is drawn from its outline and counted), graphic lines /
arcs / circles / rects / polygons (filled when the file says so), footprint graphics, and the
board outline on Edge.Cuts stroked at odb_render.EDGE_W like the ODB++ profile. Text is not
drawn (counted). "Drill" is every pad and via hole. Coordinates are KiCad's own (y down);
roundup.py aligns them to the ODB++ frame through the outline bboxes.
"""

from __future__ import annotations

import math
import re

import raster
from odb_render import EDGE_W

_TOK = re.compile(r'\(|\)|"(?:[^"\\]|\\.)*"|[^\s()"]+')


def parse_sexpr(text: str):
    stack: list = [[]]
    for m in _TOK.finditer(text):
        t = m.group(0)
        if t == "(":
            stack.append([])
        elif t == ")":
            if len(stack) > 1:
                done = stack.pop()
                stack[-1].append(done)
        elif t[0] == '"':
            stack[-1].append(t[1:-1].replace('\\"', '"').replace("\\\\", "\\"))
        else:
            stack[-1].append(t)
    root = stack[0]
    return root[0] if root else []


def _kids(node, name):
    return [c for c in node[1:] if isinstance(c, list) and c and c[0] == name]


def _kid(node, name):
    for c in node[1:]:
        if isinstance(c, list) and c and c[0] == name:
            return c
    return None


def _xy(node, name, default=(0.0, 0.0)):
    k = _kid(node, name)
    return (float(k[1]), float(k[2])) if k and len(k) >= 3 else default


def _width(node) -> float:
    st = _kid(node, "stroke")
    w = _kid(st, "width") if st else _kid(node, "width")
    return float(w[1]) if w and len(w) > 1 else 0.0


def _filled(node) -> bool:
    f = _kid(node, "fill")
    if not f or len(f) < 2:
        return False
    v = f[1] if isinstance(f[1], str) else (f[1][1] if isinstance(f[1], list) and len(f[1]) > 1 else "")
    return v in ("yes", "solid")


def _layers_of(node) -> list:
    out = []
    for k in _kids(node, "layer") + _kids(node, "layers"):
        out.extend(str(x) for x in k[1:] if isinstance(x, str))
    return out


def _pts(node) -> list:
    """(pts (xy ..) (arc (start)(mid)(end)) ..) -> point list."""
    p = _kid(node, "pts")
    out = []
    if not p:
        return out
    for c in p[1:]:
        if isinstance(c, list) and c and c[0] == "xy":
            out.append((float(c[1]), float(c[2])))
        elif isinstance(c, list) and c and c[0] == "arc":
            out.extend(arc3(_xy(c, "start"), _xy(c, "mid"), _xy(c, "end"), 50.0)[1:])
    return out


def arc3(s, m, e, ppm):
    """Points along the arc through start, mid, end."""
    (x1, y1), (x2, y2), (x3, y3) = s, m, e
    d = 2 * (x1 * (y2 - y3) + x2 * (y3 - y1) + x3 * (y1 - y2))
    if abs(d) < 1e-12:
        return [s, e]
    ux = ((x1 * x1 + y1 * y1) * (y2 - y3) + (x2 * x2 + y2 * y2) * (y3 - y1) + (x3 * x3 + y3 * y3) * (y1 - y2)) / d
    uy = ((x1 * x1 + y1 * y1) * (x3 - x2) + (x2 * x2 + y2 * y2) * (x1 - x3) + (x3 * x3 + y3 * y3) * (x2 - x1)) / d
    r = math.hypot(x1 - ux, y1 - uy)
    a1, a2, a3 = (math.atan2(y - uy, x - ux) for x, y in (s, m, e))
    ccw = lambda a, b: (b - a) % (2 * math.pi)  # noqa: E731
    sweep = ccw(a1, a3) if ccw(a1, a2) < ccw(a1, a3) else -((a1 - a3) % (2 * math.pi))
    if math.hypot(x3 - x1, y3 - y1) < 1e-9:
        sweep = 2 * math.pi
    return raster.arc_points(ux, uy, r, a1, sweep, ppm)


def _rot(px, py, deg):
    """KiCad rotation (degrees, counter-clockwise on screen, y down)."""
    a = math.radians(deg)
    c, s = math.cos(a), math.sin(a)
    return (px * c + py * s, -px * s + py * c)


class KicadBoard:
    def __init__(self, text: str):
        self.tree = parse_sexpr(text)
        self.stats = {"text": 0, "unfilled_zones": 0, "custom_pads": 0, "chamfered_pads": 0}
        lays = _kid(self.tree, "layers")
        self.layer_names = []  # canonical names, stack order
        self.copper = []
        if lays:
            for L in lays[1:]:
                if isinstance(L, list) and len(L) >= 3:
                    self.layer_names.append(str(L[1]))
                    if str(L[1]).endswith(".Cu"):
                        self.copper.append((int(L[0]), str(L[1])))
        self.copper = [n for _, n in sorted(self.copper)]
        self._ops = None

    # -- every item -> (layer, value, contours) ------------------------------------------------
    def _expand(self, layers: list) -> list:
        out = []
        for L in layers:
            if L in ("*.Cu", "F&B.Cu"):
                out.extend(self.copper if L == "*.Cu" else [c for c in ("F.Cu", "B.Cu") if c in self.copper])
            elif L.startswith("*."):
                out.extend(["F." + L[2:], "B." + L[2:]])
            else:
                out.append(L)
        return out

    def ops(self, ppm: float) -> dict:
        """{layer: [contours, ...]} (all positive) built once per ppm."""
        if self._ops is not None and self._ops[0] == ppm:
            return self._ops[1]
        by: dict = {}

        def add(layer, contours):
            if contours:
                by.setdefault(layer, []).append(contours)

        def graphic(node, tf, layer_override=None):
            kind = node[0][3:] if node[0].startswith(("gr_", "fp_")) else node[0]
            layers = [layer_override] if layer_override else _layers_of(node)
            w = _width(node)
            edge = any(L == "Edge.Cuts" for L in layers)
            if edge:
                w = EDGE_W
            fill = _filled(node) and not edge
            regions = []
            if kind == "line":
                (x0, y0), (x1, y1) = tf(*_xy(node, "start")), tf(*_xy(node, "end"))
                regions.append([raster.stroke(x0, y0, x1, y1, w, ppm)])
            elif kind == "arc":
                pts = [tf(x, y) for x, y in arc3(_xy(node, "start"), _xy(node, "mid"), _xy(node, "end"), ppm)]
                for a, b in zip(pts, pts[1:]):
                    regions.append([raster.stroke(a[0], a[1], b[0], b[1], w, ppm)])
            elif kind in ("circle", "rect", "poly"):
                if kind == "circle":
                    c = _xy(node, "center")
                    e = _xy(node, "end")
                    r = math.hypot(e[0] - c[0], e[1] - c[1])
                    pts = [tf(x, y) for x, y in raster.circle(c[0], c[1], r, ppm)]
                elif kind == "rect":
                    (x0, y0), (x1, y1) = _xy(node, "start"), _xy(node, "end")
                    pts = [tf(x, y) for x, y in ((x0, y0), (x1, y0), (x1, y1), (x0, y1))]
                else:
                    pts = [tf(x, y) for x, y in _pts(node)]
                if fill and len(pts) >= 3:
                    regions.append([pts])
                if w > 0 and len(pts) >= 2:
                    for i in range(len(pts)):
                        a, b = pts[i], pts[(i + 1) % len(pts)]
                        regions.append([raster.stroke(a[0], a[1], b[0], b[1], w, ppm)])
            elif kind in ("text", "text_box", "dimension"):
                self.stats["text"] += 1
                return
            for L in self._expand(layers):
                for reg in regions:
                    add(L, reg)

        ident = lambda x, y: (x, y)  # noqa: E731
        for node in self.tree[1:]:
            if not isinstance(node, list) or not node:
                continue
            t = node[0]
            if t in ("gr_line", "gr_arc", "gr_circle", "gr_rect", "gr_poly", "gr_text", "gr_text_box", "dimension"):
                graphic(node, ident)
            elif t == "segment":
                (x0, y0), (x1, y1) = _xy(node, "start"), _xy(node, "end")
                w = float(_kid(node, "width")[1])
                for L in _layers_of(node):
                    add(L, [raster.stroke(x0, y0, x1, y1, w, ppm)])
            elif t == "arc":
                w = float(_kid(node, "width")[1])
                pts = arc3(_xy(node, "start"), _xy(node, "mid"), _xy(node, "end"), ppm)
                for L in _layers_of(node):
                    for a, b in zip(pts, pts[1:]):
                        add(L, [raster.stroke(a[0], a[1], b[0], b[1], w, ppm)])
            elif t == "via":
                x, y = _xy(node, "at")
                size = float(_kid(node, "size")[1])
                drill = _kid(node, "drill")
                ls = _layers_of(node)
                span = self.copper
                if len(ls) >= 2 and ls[0] in self.copper and ls[1] in self.copper:
                    i0, i1 = sorted((self.copper.index(ls[0]), self.copper.index(ls[1])))
                    span = self.copper[i0:i1 + 1]
                for L in span:
                    add(L, [raster.circle(x, y, size / 2, ppm)])
                if drill and len(drill) > 1:
                    add("Drill", [raster.circle(x, y, float(drill[1]) / 2, ppm)])
            elif t == "zone":
                fills = _kids(node, "filled_polygon")
                if fills:
                    for fp in fills:
                        L = _kid(fp, "layer")
                        pts = _pts(fp)
                        if L and len(pts) >= 3:
                            add(str(L[1]), [pts])
                else:
                    poly = _kid(node, "polygon")
                    pts = _pts(poly) if poly else []
                    if len(pts) >= 3 and not _kid(node, "keepout"):
                        self.stats["unfilled_zones"] += 1
                        for L in _layers_of(node):
                            add(L, [pts])
            elif t in ("footprint", "module"):
                self._footprint(node, add, graphic, ppm)
        self._ops = (ppm, by)
        return by

    def _footprint(self, fp, add, graphic, ppm):
        at = _kid(fp, "at")
        fx, fy = float(at[1]), float(at[2])
        frot = float(at[3]) if len(at) > 3 else 0.0

        def tf(px, py):
            rx, ry = _rot(px, py, frot)
            return (fx + rx, fy + ry)
        for g in fp[1:]:
            if isinstance(g, list) and g and g[0] in ("fp_line", "fp_arc", "fp_circle", "fp_rect", "fp_poly", "fp_text", "fp_text_box"):
                graphic(g, tf)
        for pad in _kids(fp, "pad"):
            self._pad(pad, tf, frot, add, ppm)

    def _pad(self, pad, tf, frot, add, ppm):
        shape = str(pad[3]) if len(pad) > 3 else "rect"
        at = _kid(pad, "at")
        px, py = float(at[1]), float(at[2])
        prot = float(at[3]) if len(at) > 3 else 0.0  # absolute (already includes the footprint's)
        cx, cy = tf(px, py)
        size = _kid(pad, "size")
        w, h = (float(size[1]), float(size[2])) if size else (0.0, 0.0)
        mm = _kid(pad, "solder_mask_margin")
        pm = _kid(pad, "solder_paste_margin")
        margins = {"Mask": float(mm[1]) if mm else 0.0, "Paste": float(pm[1]) if pm else 0.0}

        def outline(g):
            """the pad's own shape grown by g (KiCad: solder_mask_margin / solder_paste_margin)"""
            if shape == "circle":
                return [raster.circle(0, 0, max(w / 2 + g, 0), ppm)]
            if shape == "oval":
                ww, hh = w + 2 * g, h + 2 * g
                return [raster.stroke(-(ww - hh) / 2, 0, (ww - hh) / 2, 0, hh, ppm)] if ww >= hh else [raster.stroke(0, -(hh - ww) / 2, 0, (hh - ww) / 2, ww, ppm)]
            if shape in ("roundrect", "chamfered_rect"):
                rr = _kid(pad, "roundrect_rratio")
                r = (float(rr[1]) if rr else 0.0) * min(w, h)
                return _rrect(w + 2 * g, h + 2 * g, max(r + g, 0), ppm)
            if shape == "rect":   # KiCad grows a rectangle with rounded corners of radius g
                return _rrect(w + 2 * g, h + 2 * g, g, ppm) if g > 0 else [[(-w / 2 - g, -h / 2 - g), (w / 2 + g, -h / 2 - g), (w / 2 + g, h / 2 + g), (-w / 2 - g, h / 2 + g)]]
            return None           # trapezoid / custom: drawn without the margin
        if shape == "chamfered_rect":
            self.stats["chamfered_pads"] += 1
        local = []
        if shape == "circle":
            local = [raster.circle(0, 0, w / 2, ppm)]
        elif shape == "oval":
            local = [raster.stroke(-(w - h) / 2, 0, (w - h) / 2, 0, h, ppm)] if w >= h else [raster.stroke(0, -(h - w) / 2, 0, (h - w) / 2, w, ppm)]
        elif shape in ("roundrect", "chamfered_rect"):
            rr = _kid(pad, "roundrect_rratio")
            r = (float(rr[1]) if rr else 0.0) * min(w, h)
            local = _rrect(w, h, r, ppm)
        elif shape == "trapezoid":
            d = _kid(pad, "rect_delta")
            dx, dy = (float(d[1]), float(d[2])) if d else (0.0, 0.0)
            local = [[(-w / 2 - dy / 2, -h / 2 - dx / 2), (w / 2 + dy / 2, -h / 2 + dx / 2), (w / 2 - dy / 2, h / 2 - dx / 2), (-w / 2 + dy / 2, h / 2 + dx / 2)]]
        elif shape == "custom":
            self.stats["custom_pads"] += 1
            opt = _kid(pad, "options")
            anchor = _kid(opt, "anchor") if opt else None
            local = [raster.circle(0, 0, w / 2, ppm)] if (anchor and anchor[1] == "circle") else [[(-w / 2, -h / 2), (w / 2, -h / 2), (w / 2, h / 2), (-w / 2, h / 2)]]
        else:
            local = [[(-w / 2, -h / 2), (w / 2, -h / 2), (w / 2, h / 2), (-w / 2, h / 2)]]
        regions = [local]
        prims = _kid(pad, "primitives")
        if prims:
            for g in prims[1:]:
                if not isinstance(g, list):
                    continue
                gw = _width(g)
                if g[0] == "gr_poly":
                    pts = _pts(g)
                    if len(pts) >= 3:
                        regions.append([pts])
                    if gw > 0:
                        for i in range(len(pts)):
                            a, b = pts[i], pts[(i + 1) % len(pts)]
                            regions.append([raster.stroke(a[0], a[1], b[0], b[1], gw, ppm)])
                elif g[0] == "gr_circle":
                    c, e = _xy(g, "center"), _xy(g, "end")
                    r = math.hypot(e[0] - c[0], e[1] - c[1])
                    if _filled(g) or gw == 0:
                        regions.append([raster.circle(c[0], c[1], r + gw / 2, ppm)])
                    else:
                        regions.append([raster.circle(c[0], c[1], r + gw / 2, ppm), raster.circle(c[0], c[1], max(r - gw / 2, 0), ppm)])
                elif g[0] == "gr_line":
                    (x0, y0), (x1, y1) = _xy(g, "start"), _xy(g, "end")
                    regions.append([raster.stroke(x0, y0, x1, y1, gw, ppm)])
                elif g[0] == "gr_rect":
                    (x0, y0), (x1, y1) = _xy(g, "start"), _xy(g, "end")
                    regions.append([[(x0, y0), (x1, y0), (x1, y1), (x0, y1)]])
        world = []
        for reg in regions:
            world.append([[(cx + rx, cy + ry) for rx, ry in (_rot(x, y, prot) for x, y in c)] for c in reg])
        layers = self._expand(_layers_of(pad))
        for L in layers:
            g = margins.get(L.split(".")[-1], 0.0) if L[:2] in ("F.", "B.") else 0.0
            grown = outline(g) if g else None
            if grown is not None:
                regs = [grown] + regions[1:]
                for reg in regs:
                    add(L, [[(cx + rx, cy + ry) for rx, ry in (_rot(x, y, prot) for x, y in c)] for c in reg])
                continue
            for reg in world:
                add(L, reg)
        drill = _kid(pad, "drill")
        if drill and len(drill) > 1:
            if drill[1] == "oval":
                dw = float(drill[2])
                dh = float(drill[3]) if len(drill) > 3 and not isinstance(drill[3], list) else dw
                hole = [raster.stroke(-(dw - dh) / 2, 0, (dw - dh) / 2, 0, dh, ppm)] if dw >= dh else [raster.stroke(0, -(dh - dw) / 2, 0, (dh - dw) / 2, dw, ppm)]
            else:
                d = float(drill[1])
                hole = [raster.circle(0, 0, d / 2, ppm)]
            o = _kid(drill, "offset")
            ox, oy = (float(o[1]), float(o[2])) if o else (0.0, 0.0)
            add("Drill", [[(cx + rx, cy + ry) for rx, ry in (_rot(x + ox, y + oy, prot) for x, y in c)] for c in hole])

    def has_layer(self, name: str, ppm: float) -> bool:
        return bool(self.ops(ppm).get(name))

    def edge_bbox(self, ppm: float = 20.0):
        regs = self.ops(ppm).get("Edge.Cuts") or []
        b = raster.bbox([c for reg in regs for c in reg])
        if b is None:
            return None
        h = EDGE_W / 2  # strokes are EDGE_W wide; the bbox of the centre line is what the profile bbox is
        return (b[0] + h, b[1] + h, b[2] - h, b[3] - h)

    def render(self, name: str, view: raster.View) -> raster.Bitmap:
        bm = raster.Bitmap(view.w, view.h)
        for reg in self.ops(view.ppm).get(name, []):
            bm.fill(view.px(reg), 1)
        return bm


def _rrect(w, h, r, ppm):
    r = max(0.0, min(r, w / 2, h / 2))
    if r <= 1e-9:
        return [[(-w / 2, -h / 2), (w / 2, -h / 2), (w / 2, h / 2), (-w / 2, h / 2)]]
    pts = []
    for sx, sy, a0 in ((1, 1, 0.0), (-1, 1, math.pi / 2), (-1, -1, math.pi), (1, -1, 1.5 * math.pi)):
        pts.extend(raster.arc_points(sx * (w / 2 - r), sy * (h / 2 - r), r, a0, math.pi / 2, ppm))
    return [pts]
