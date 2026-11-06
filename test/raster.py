"""Dependency-free 1-bit rasterizer, ink metrics and PNG writer for the ODB++ round-up.

Both sides of the round-up (the ODB++ reference and every converter's .kicad_pcb) are drawn by this
one module, so a pixel difference is a geometry difference, never a renderer difference. Pure
Python on purpose: the PC lane runs recipes as ``python -I`` with whatever the gate's interpreter
has, so Pillow is not assumed (it is not used at all).

Geometry arrives as *regions*: a list of closed contours in mm, filled even-odd (a donut is two
circles, a surface is its islands and holes). ``View`` maps mm to pixels; ``Bitmap.fill`` draws a
region with a value (1 = positive ink, 0 = clear, which is how negative ODB++ features erase).
"""

from __future__ import annotations

import math
import struct
import zlib

# ----------------------------------------------------------------------------------------------
# geometry helpers (mm)
# ----------------------------------------------------------------------------------------------


def seg_count(r_mm: float, px_per_mm: float, sweep: float = 2 * math.pi) -> int:
    """Vertices for an arc of radius r so the chord error stays under ~0.25 px."""
    r_px = max(r_mm * px_per_mm, 0.5)
    n = int(math.ceil(sweep / (2 * math.acos(max(-1.0, 1 - 0.25 / r_px))))) if r_px > 0.25 else 4
    return max(4, min(n, 256))


def circle(cx: float, cy: float, r: float, ppm: float) -> list:
    n = seg_count(r, ppm)
    return [(cx + r * math.cos(2 * math.pi * i / n), cy + r * math.sin(2 * math.pi * i / n)) for i in range(n)]


def arc_points(cx: float, cy: float, r: float, a0: float, sweep: float, ppm: float) -> list:
    """Points from angle a0 sweeping by ``sweep`` radians (sign = direction), both ends included."""
    n = max(1, seg_count(r, ppm, abs(sweep)))
    return [(cx + r * math.cos(a0 + sweep * i / n), cy + r * math.sin(a0 + sweep * i / n)) for i in range(n + 1)]


def stroke(xs: float, ys: float, xe: float, ye: float, w: float, ppm: float, square: bool = False) -> list:
    """A line of width w with round caps (or square caps that extend w/2) as one contour."""
    h = w / 2.0
    dx, dy = xe - xs, ye - ys
    ln = math.hypot(dx, dy)
    if square:
        if ln < 1e-12:
            return [(xs - h, ys - h), (xs + h, ys - h), (xs + h, ys + h), (xs - h, ys + h)]
        ux, uy = dx / ln * h, dy / ln * h
        nx, ny = -uy, ux
        return [(xs - ux + nx, ys - uy + ny), (xe + ux + nx, ye + uy + ny), (xe + ux - nx, ye + uy - ny), (xs - ux - nx, ys - uy - ny)]
    if ln < 1e-12:
        return circle(xs, ys, h, ppm)
    a = math.atan2(dy, dx)
    return arc_points(xe, ye, h, a - math.pi / 2, math.pi, ppm) + arc_points(xs, ys, h, a + math.pi / 2, math.pi, ppm)


def arc_stroke(xs, ys, xe, ye, xc, yc, cw: bool, w: float, ppm: float, square: bool = False) -> list:
    """An arc (start, end, centre, direction; start == end is a full circle) stroked with width w.

    Returns a region (list of contours): a full circle is a ring (two contours), otherwise one
    contour = outer edge + end cap + inner edge reversed + start cap.
    """
    r = math.hypot(xs - xc, ys - yc)
    h = w / 2.0
    a0 = math.atan2(ys - yc, xs - xc)
    a1 = math.atan2(ye - yc, xe - xc)
    if math.hypot(xe - xs, ye - ys) < 1e-9:
        sweep = -2 * math.pi if cw else 2 * math.pi
    else:
        sweep = a1 - a0
        if cw and sweep > 0:
            sweep -= 2 * math.pi
        elif not cw and sweep < 0:
            sweep += 2 * math.pi
    if r < 1e-9:
        return [circle(xc, yc, h, ppm)]
    if abs(abs(sweep) - 2 * math.pi) < 1e-9:
        if r - h <= 0:
            return [circle(xc, yc, r + h, ppm)]
        return [circle(xc, yc, r + h, ppm), circle(xc, yc, r - h, ppm)]
    ri = max(r - h, 0.0)
    outer = arc_points(xc, yc, r + h, a0, sweep, ppm)
    inner = arc_points(xc, yc, ri, a0, sweep, ppm)[::-1]
    sgn = 1 if sweep > 0 else -1
    pe = (xc + r * math.cos(a0 + sweep), yc + r * math.sin(a0 + sweep))
    if square:
        return [outer + inner]
    cap_e = arc_points(pe[0], pe[1], h, a0 + sweep, sgn * math.pi, ppm)
    cap_s = arc_points(xs, ys, h, a0 + math.pi, sgn * math.pi, ppm)
    return [outer + cap_e[1:-1] + inner + cap_s[1:-1]]


def xform(contours: list, fn) -> list:
    return [[fn(x, y) for x, y in c] for c in contours]


def bbox(contours: list):
    xs = [x for c in contours for x, _ in c]
    ys = [y for c in contours for _, y in c]
    return (min(xs), min(ys), max(xs), max(ys)) if xs else None


# ----------------------------------------------------------------------------------------------
# view + bitmap
# ----------------------------------------------------------------------------------------------


class View:
    """mm -> pixel. ``y_up`` True for ODB++ (y grows up), False for KiCad (y grows down).

    (x0, y_top) is the mm coordinate of the image's top-left corner in that side's own frame.
    """

    def __init__(self, x0: float, y_top: float, w_mm: float, h_mm: float, ppm: float, y_up: bool):
        self.x0, self.y_top, self.ppm, self.y_up = x0, y_top, ppm, y_up
        self.w = max(1, int(math.ceil(w_mm * ppm)))
        self.h = max(1, int(math.ceil(h_mm * ppm)))

    def px(self, contours: list) -> list:
        p, x0, yt = self.ppm, self.x0, self.y_top
        if self.y_up:
            return [[((x - x0) * p, (yt - y) * p) for x, y in c] for c in contours]
        return [[((x - x0) * p, (y - yt) * p) for x, y in c] for c in contours]


class Bitmap:
    """w*h bytes, 1 = ink. Rows top to bottom."""

    def __init__(self, w: int, h: int):
        self.w, self.h = w, h
        self.buf = bytearray(w * h)
        self._ones = b"\x01" * w
        self._zeros = bytes(w)

    def fill(self, contours_px: list, value: int = 1) -> None:
        """Even-odd scanline fill, sampled at pixel centres."""
        edges = []
        ymin, ymax = float("inf"), float("-inf")
        for c in contours_px:
            n = len(c)
            if n < 3:
                continue
            for i in range(n):
                x0, y0 = c[i]
                x1, y1 = c[(i + 1) % n]
                if y0 == y1:
                    continue
                if y0 > y1:
                    x0, y0, x1, y1 = x1, y1, x0, y0
                edges.append((y0, y1, x0, (x1 - x0) / (y1 - y0)))
                if y0 < ymin:
                    ymin = y0
                if y1 > ymax:
                    ymax = y1
        if not edges:
            return
        r0 = max(0, int(math.ceil(ymin - 0.5)))
        r1 = min(self.h - 1, int(math.ceil(ymax - 0.5)) - 1)
        if r1 < r0:
            return
        edges.sort()
        w, buf = self.w, self.buf
        src = self._ones if value else self._zeros
        active: list = []
        ei, ne = 0, len(edges)
        for row in range(r0, r1 + 1):
            yc = row + 0.5
            while ei < ne and edges[ei][0] <= yc:
                active.append(edges[ei])
                ei += 1
            if not active:
                continue
            active = [e for e in active if e[1] > yc]
            xs = sorted(e[2] + (yc - e[0]) * e[3] for e in active)
            off = row * w
            for k in range(0, len(xs) - 1, 2):
                a = int(math.ceil(xs[k] - 0.5))
                b = int(math.ceil(xs[k + 1] - 0.5))
                if a < 0:
                    a = 0
                if b > w:
                    b = w
                if b > a:
                    buf[off + a:off + b] = src[:b - a]

    def count(self) -> int:
        return self.buf.count(1)

    def as_int(self) -> int:
        return int.from_bytes(self.buf, "big")


# ----------------------------------------------------------------------------------------------
# metrics
# ----------------------------------------------------------------------------------------------


def _popcount(v: int) -> int:
    return v.bit_count() if hasattr(v, "bit_count") else bin(v).count("1")


def _dilate(v: int, w: int, h: int, r: int) -> int:
    """Grow ink by r pixels (a square of side 2r+1). Pixels are bytes 0/1 in a big-endian int."""
    if r <= 0:
        return v
    # mask of every pixel's low bit, so shifted bits that land in a neighbour's high bits are dropped
    lsb = int.from_bytes(b"\x01" * (w * h), "big")
    out = v
    for _ in range(r):  # horizontal
        out = out | ((out << 8) & lsb) | (out >> 8)
    # a horizontal shift wraps row ends into the next row; at r <= 2 px on a margin-padded image
    # that only ever touches the empty border, so it is not corrected
    col = out
    for _ in range(r):  # vertical
        col = col | ((col << (8 * w)) & ((1 << (8 * w * h)) - 1)) | (col >> (8 * w))
    return col & lsb


def compare(ref: Bitmap, got: Bitmap, tol_px: int = 1) -> dict:
    """XOR ratio (raw, of the union) and tolerant missing / extra ink (beyond tol_px of the other side)."""
    a, b = ref.as_int(), got.as_int()
    ra, rb = _popcount(a), _popcount(b)
    union = _popcount(a | b)
    xor = _popcount(a ^ b)
    da = _dilate(a, ref.w, ref.h, tol_px)
    db = _dilate(b, ref.w, ref.h, tol_px)
    missing = _popcount(a & ~db)
    extra = _popcount(b & ~da)
    return {
        "ref_px": ra, "got_px": rb, "xor_px": xor,
        "xor_ratio": round(xor / union, 6) if union else 0.0,
        "missing_px": missing, "extra_px": extra,
        "missing_ratio": round(missing / ra, 6) if ra else (0.0 if not rb else None),
        "extra_ratio": round(extra / rb, 6) if rb else 0.0,
        "_masks": (a, b, a & ~db, b & ~da),
    }


# ----------------------------------------------------------------------------------------------
# images (RGB rows) + PNG
# ----------------------------------------------------------------------------------------------

BG = (18, 22, 32)
INK = {"ref": (235, 200, 90), "got": (110, 190, 250)}
BOTH, MISS, EXTRA = (90, 90, 100), (250, 70, 70), (70, 220, 120)


class Image:
    def __init__(self, w: int, h: int, color=BG):
        self.w, self.h = w, h
        self.px = bytearray(bytes(color) * (w * h))

    def paste(self, other: "Image", x: int, y: int) -> None:
        for row in range(other.h):
            ty = y + row
            if not 0 <= ty < self.h:
                continue
            ow = min(other.w, self.w - x)
            if ow <= 0:
                return
            s = row * other.w * 3
            d = (ty * self.w + x) * 3
            self.px[d:d + ow * 3] = other.px[s:s + ow * 3]

    def rect(self, x0: int, y0: int, x1: int, y1: int, color) -> None:
        for y in range(max(0, y0), min(self.h, y1)):
            d = (y * self.w + max(0, x0)) * 3
            n = min(self.w, x1) - max(0, x0)
            if n > 0:
                self.px[d:d + n * 3] = bytes(color) * n

    def text(self, x: int, y: int, s: str, color=(230, 230, 230), scale: int = 2) -> None:
        cx = x
        for ch in s.upper():
            g = FONT.get(ch, FONT["?"])
            for gy, bits in enumerate(g):
                for gx in range(5):
                    if bits >> (4 - gx) & 1:
                        self.rect(cx + gx * scale, y + gy * scale, cx + (gx + 1) * scale, y + (gy + 1) * scale, color)
            cx += 6 * scale

    def save_png(self, path) -> None:
        raw = b"".join(b"\x00" + bytes(self.px[r * self.w * 3:(r + 1) * self.w * 3]) for r in range(self.h))
        def chunk(t, d):
            return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
        data = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", self.w, self.h, 8, 2, 0, 0, 0)) \
            + chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b"")
        with open(path, "wb") as f:
            f.write(data)


def _from_masks(w: int, h: int, layers: list) -> Image:
    """layers: [(mask int of 0/1 bytes, colour)], later ones on top."""
    img = Image(w, h)
    n = w * h
    for mask, color in layers:
        if not mask:
            continue
        bs = mask.to_bytes(n, "big")
        c = bytes(color)
        pos = bs.find(1)
        px = img.px
        while pos != -1:
            end = bs.find(0, pos)
            if end == -1:
                end = n
            px[pos * 3:end * 3] = c * (end - pos)
            pos = bs.find(1, end)
    return img


def ink_image(bm: Bitmap, color) -> Image:
    return _from_masks(bm.w, bm.h, [(bm.as_int(), color)])


def diff_image(w: int, h: int, cmp: dict) -> Image:
    a, b, miss, extra = cmp["_masks"]
    return _from_masks(w, h, [(a & b, BOTH), (miss, MISS), (extra, EXTRA)])


def shrink(mask: int, w: int, h: int, f: int):
    """OR-reduce a 0/1-byte mask by an integer factor f (ink survives the shrink). -> (mask, w2, h2)."""
    if f <= 1 or not mask:
        return mask, (w // max(f, 1)), (h // max(f, 1))
    w2, h2 = w // f, h // f
    bs = mask.to_bytes(w * h, "big")
    rows = []
    for y in range(h2):
        acc = 0
        for yy in range(y * f, y * f + f):
            acc |= int.from_bytes(bs[yy * w:yy * w + w2 * f], "big")
        acc0 = acc
        for k in range(1, f):
            acc |= acc0 >> (8 * k)
        rb = acc.to_bytes(w2 * f, "big")[f - 1::f] if f > 1 else acc.to_bytes(w2, "big")
        # after >> the OR of a block lands on its last byte
        rows.append(rb[:w2])
    return int.from_bytes(b"".join(rows), "big"), w2, h2


# 5x7 glyphs for labels (no font files, no Pillow)
FONT = {k: tuple(int(r, 16) for r in v.split()) for k, v in {
    "A": "0E 11 11 1F 11 11 11", "B": "1E 11 11 1E 11 11 1E", "C": "0E 11 10 10 10 11 0E", "D": "1E 11 11 11 11 11 1E",
    "E": "1F 10 10 1E 10 10 1F", "F": "1F 10 10 1E 10 10 10", "G": "0E 11 10 17 11 11 0F", "H": "11 11 11 1F 11 11 11",
    "I": "0E 04 04 04 04 04 0E", "J": "07 02 02 02 02 12 0C", "K": "11 12 14 18 14 12 11", "L": "10 10 10 10 10 10 1F",
    "M": "11 1B 15 15 11 11 11", "N": "11 11 19 15 13 11 11", "O": "0E 11 11 11 11 11 0E", "P": "1E 11 11 1E 10 10 10",
    "Q": "0E 11 11 11 15 12 0D", "R": "1E 11 11 1E 14 12 11", "S": "0F 10 10 0E 01 01 1E", "T": "1F 04 04 04 04 04 04",
    "U": "11 11 11 11 11 11 0E", "V": "11 11 11 11 11 0A 04", "W": "11 11 11 15 15 15 0A", "X": "11 11 0A 04 0A 11 11",
    "Y": "11 11 0A 04 04 04 04", "Z": "1F 01 02 04 08 10 1F", "0": "0E 11 13 15 19 11 0E", "1": "04 0C 04 04 04 04 0E",
    "2": "0E 11 01 02 04 08 1F", "3": "1F 02 04 02 01 11 0E", "4": "02 06 0A 12 1F 02 02", "5": "1F 10 1E 01 01 11 0E",
    "6": "06 08 10 1E 11 11 0E", "7": "1F 01 02 04 08 08 08", "8": "0E 11 11 0E 11 11 0E", "9": "0E 11 11 0F 01 02 0C",
    ".": "00 00 00 00 00 0C 0C", "-": "00 00 00 1F 00 00 00", "_": "00 00 00 00 00 00 1F", "+": "00 04 04 1F 04 04 00",
    ":": "00 0C 0C 00 0C 0C 00", "/": "01 01 02 04 08 10 10", "=": "00 00 1F 00 1F 00 00", "%": "18 19 02 04 08 13 03",
    " ": "00 00 00 00 00 00 00", "?": "0E 11 01 02 04 00 04", "|": "04 04 04 04 04 04 04", "(": "02 04 08 08 08 04 02",
    ")": "08 04 02 02 02 04 08", ",": "00 00 00 00 0C 04 08",
}.items()}
