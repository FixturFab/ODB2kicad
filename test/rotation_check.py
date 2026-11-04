#!/usr/bin/env python3
"""Which reading of the CMP rotation does an ODB++ job itself support?

    python3 -I test/rotation_check.py <odb_job_dir>...

Every component record carries its rotation, its mirror flag, and TOP (toeprint) records with
the board position of each pin. eda/data carries the same package's pins in the package frame.
The right reading of the rotation is the one that maps the package pins onto the toeprints. Per
part this tries: clockwise (the spec), counter-clockwise, and no rotation at all (0 deg, the
reading of the converter Studio vendors), each with the mirror flag applied (x -> -x first) and,
for mirrored parts, without it (kicad-cli writes bottom packages already flipped). A part fits a
reading when every pin lands within 10 um.

Reads the job only; no converter involved. Run it on public or made-up jobs.
"""
from __future__ import annotations

import collections
import glob
import math
import os
import sys

TOL_MM = 0.01


def _units(lines) -> float:
    for line in lines:
        if line.startswith("UNITS="):
            return 25.4 if "INCH" in line.upper() else 1.0
    return 25.4  # the spec's default


def _read(path):
    with open(path, errors="replace") as f:
        return f.read().splitlines()


def check(root: str) -> dict:
    eda_files = glob.glob(os.path.join(root, "steps", "*", "eda", "data"))
    if not eda_files:
        return {}
    step = eda_files[0].rsplit(os.sep + "eda" + os.sep, 1)[0]
    eda = _read(eda_files[0])
    k = _units(eda)
    pkgs: list = []
    for line in eda:
        t = line.split(";")[0].split()
        if not t:
            continue
        if t[0] == "PKG":
            pkgs.append([])
        elif t[0] == "PIN" and pkgs:
            pkgs[-1].append((float(t[3]) * k, float(t[4]) * k))
    per: collections.Counter = collections.Counter()
    fits: collections.Counter = collections.Counter()
    for side in ("top", "bot"):
        f = os.path.join(step, "layers", f"comp_+_{side}", "components")
        if not os.path.exists(f):
            continue
        lines = _read(f)
        kc = _units(lines)
        comps, cur = [], None
        for line in lines:
            t = line.split(";")[0].split()
            if not t:
                continue
            if t[0] == "CMP":
                cur = dict(pkg=int(t[1]), x=float(t[2]) * kc, y=float(t[3]) * kc, rot=float(t[4]),
                           mirror=t[5] == "M", pins=[])
                comps.append(cur)
            elif t[0] == "TOP" and cur is not None:
                cur["pins"].append((int(t[1]), float(t[2]) * kc, float(t[3]) * kc))
        for c in comps:
            pk = pkgs[c["pkg"]] if c["pkg"] < len(pkgs) else []
            if len(pk) < 2 or not c["pins"]:
                per["(no pins to check)"] += 1
                continue

            def worst(sense: int, mirror: bool) -> float:
                a = math.radians(c["rot"]) * sense
                e = 0.0
                for pi, X, Y in c["pins"]:
                    if pi >= len(pk):
                        return math.inf
                    px, py = pk[pi]
                    if mirror:
                        px = -px
                    # sense +1: clockwise with y up
                    rx, ry = px * math.cos(a) + py * math.sin(a), -px * math.sin(a) + py * math.cos(a)
                    e = max(e, math.hypot(c["x"] + rx - X, c["y"] + ry - Y))
                return e

            key = ("rotated" if abs(c["rot"] % 360) > 1e-6 else "at 0 deg") + (", mirrored" if c["mirror"] else "")
            per[key] += 1
            for name, sense in (("cw", 1), ("ccw", -1), ("0 deg", 0)):
                if worst(sense, c["mirror"]) < TOL_MM:
                    fits[(key, name)] += 1
            if c["mirror"] and worst(1, False) < TOL_MM:
                fits[(key, "cw, package already flipped")] += 1
    return {"parts": per, "fits": fits}


def main(argv) -> int:
    for root in argv:
        r = check(root)
        print(os.path.basename(root.rstrip("/")))
        if not r:
            print("  no eda/data")
            continue
        for key, n in sorted(r["parts"].items()):
            if key.startswith("("):
                print(f"  {key}: {n}")
                continue
            cells = ", ".join(f"{h} {r['fits'][(key, h)]}" for h in ("cw", "ccw", "0 deg", "cw, package already flipped")
                              if h != "cw, package already flipped" or "mirrored" in key)
            print(f"  {key:22s} {n:4d} parts fit: {cells}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
