#!/usr/bin/env python3
"""Footprint angles and absolute pad positions of two converters' boards, part by part.

    python3 -I test/compare_rotation.py <a.kicad_pcb> <b.kicad_pcb>

Prints how many parts agree on the footprint angle, how many on every pad's board position, how
many of b's parts sit at 0 deg, and the angles a uses. Free pads (odb2kicad:FreePad) are skipped.
"""
import math
import re
import sys


def parts(path):
    text = open(path).read()
    out = {}
    for blk in re.split(r'\n  \(footprint ', text)[1:]:
        if blk.startswith('"odb2kicad:FreePad"'):
            continue
        at = re.search(r'\n    \(at ([-\d.]+) ([-\d.]+)(?: ([-\d.]+))?\)', blk)
        ref = re.search(r'\(property "Reference" "([^"]*)"', blk).group(1)
        x, y, a = float(at.group(1)), float(at.group(2)), float(at.group(3) or 0)
        c, s = math.cos(math.radians(a)), math.sin(math.radians(a))
        pads = []
        for p in re.finditer(r'\(pad "([^"]*)" \S+ \S+ \(at ([-\d.]+) ([-\d.]+)', blk):
            px, py = float(p.group(2)), float(p.group(3))
            pads.append((p.group(1), round(x + px * c + py * s, 3), round(y - px * s + py * c, 3)))
        out[ref] = (a % 360, sorted(pads))
    return out


A, B = parts(sys.argv[1]), parts(sys.argv[2])
common = [r for r in A if r in B]
angle = sum(1 for r in common if abs(A[r][0] - B[r][0]) < 0.01)
pos = sum(1 for r in common if A[r][1] == B[r][1])
zero = sum(1 for r in common if abs(B[r][0]) < 0.01)
n = max(len(common), 1)
print(f"parts {len(common)}: angle agrees {angle} ({100 * angle / n:.0f}%), pad positions agree {pos}, "
      f"b at 0 deg {zero}; a's angles: {sorted({round(v[0], 1) for v in A.values()})}")
