#!/usr/bin/env python3
"""Reference-side ODB++ layer renderer with text support (cross-parser comparison tool).

Our own reference renderer, built on the round-up's: the text renderer is
test/odb_render_text.py, which test/odb_render.py draws T / V records with, so this tool and the
round-up harness (test/roundup.py) draw the same reference. --shipped-text-grammar draws text
the simpler way some readers do (see odb_render_text.py for the three differences).

Renders an ODB++ job (directory or .tgz/.zip) layer by layer to PNG, extending the repo's
test/odb_render.py with: T/V text drawn from the job's fonts/ (strokes + polygon glyphs),
generic MISC/DOCUMENT layers (any matrix row, not just the KiCad alias table), step selection,
and optional stephdr origin application.

The ODB++ side and any converter's .kicad_pcb side (repo test/kicad_render.py) are then
diff-able with repo test/raster.py compare() / metrics. This file is the reference side for
comparing different ODB++ parsers end-to-end.

Usage:
  python3 render_odb_sheet.py <odb-root-or-archive> [--step NAME] [--layer NAME]... 
                              [--ppm 40] [--out DIR] [--sheet FILE.png] [--apply-origin]
                              [--shipped-text-grammar] [--kicad BOARD.kicad_pcb]
                              [--repo ODB2KICAD_REPO_ROOT]

--kicad compares a converter's board with the reference on the same view (KiCad x, y = ODB++
x, -y), layer by layer: xor / missing / extra as in test/roundup.py, a <layer>__diff.png each
(grey both, red missing, green extra), the numbers in render_report.json. Unlike the round-up,
whose view is the board outline, --layer grows the view to the layer's content, so text and
drawings beside the board (process maps) are compared too.

--repo points at the odb2kicad checkout (its test/ dir is put on sys.path); default: the
checkout this file is in, else $ODB2KICAD_REPO. Run with `python3 -I`: inputs are untrusted.
"""
from __future__ import annotations

import argparse
import json
import math
import os
import re
import sys
import tarfile
import tempfile
import zipfile
from pathlib import Path



def find_repo(root_hint: str | None) -> Path:
    cands = []
    if root_hint:
        cands.append(Path(root_hint))
    if os.environ.get("ODB2KICAD_REPO"):
        cands.append(Path(os.environ["ODB2KICAD_REPO"]))
    p = Path(__file__).resolve().parent
    while p != p.parent:
        cands.append(p)
        cands.append(p / "ODB2kicad")
        p = p.parent
    for c in cands:
        if (c / "test" / "odb_render.py").is_file():
            return c
    raise SystemExit("cannot locate the ODB2kicad repo (pass --repo /path or set ODB2KICAD_REPO)")


def unpack(archive: Path) -> Path:
    """Extract .tgz/.tar/.zip to a temp dir (regular files only, no traversal). -> job root."""
    tmp = Path(tempfile.mkdtemp(prefix="odbrender_"))
    if archive.suffix in (".zip",):
        with zipfile.ZipFile(archive) as z:
            for i in z.infolist():
                if i.is_dir() or ".." in i.filename.split("/"):
                    continue
                z.extract(i, tmp)
    else:
        with tarfile.open(archive) as t:
            for m in t.getmembers():
                if not m.isfile() or ".." in m.name.split("/"):
                    continue
                t.extract(m, tmp)
    roots = [d for d in tmp.iterdir() if (d / "matrix" / "matrix").is_file()]
    if not roots and (tmp / "matrix" / "matrix").is_file():
        roots = [tmp]
    if not roots:
        raise SystemExit(f"no ODB++ job root (matrix/matrix) found in {archive.name}")
    return roots[0]


def stephdr_origin(step_dir: Path):
    """(x_origin_mm, y_origin_mm) or None."""
    f = step_dir / "stephdr"
    if not f.is_file():
        return None
    x = y = None
    units = "INCH"
    for line in f.read_text(encoding="latin-1").splitlines():
        line = line.strip()
        if line.upper().startswith("UNITS="):
            units = line.split("=", 1)[1].strip().upper()
        m = re.match(r"X_ORIGIN=(-?[\d.]+)", line)
        if m:
            x = float(m.group(1))
        m = re.match(r"Y_ORIGIN=(-?[\d.]+)", line)
        if m:
            y = float(m.group(1))
    if x is None and y is None:
        return None
    s = 25.4 if units == "INCH" else 1.0
    return ((x or 0.0) * s, (y or 0.0) * s)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("odb")
    ap.add_argument("--step")
    ap.add_argument("--layer", action="append", default=[], help="render only this ODB++ layer name (repeatable)")
    ap.add_argument("--ppm", type=float, default=40.0)
    ap.add_argument("--out", default=".")
    ap.add_argument("--sheet")
    ap.add_argument("--apply-origin", action="store_true")
    ap.add_argument("--shipped-text-grammar", action="store_true",
                    help="read T / V records the simpler way (trailing field = mirror flag, glyphs flipped in place)")
    ap.add_argument("--kicad", help="a converter's .kicad_pcb of this job: compared with the reference, layer by layer")
    ap.add_argument("--tol-px", type=int, default=1, help="missing / extra ignore ink within this many px of the other side")
    ap.add_argument("--repo")
    args = ap.parse_args()

    repo = find_repo(args.repo)
    sys.path.insert(0, str(repo / "test"))
    import odb_render  # noqa: E402
    import odb_render_text as txt  # noqa: E402
    import raster  # noqa: E402
    from valor_oracle import odb_file  # noqa: E402

    src = Path(args.odb)
    root = unpack(src) if src.is_file() else src
    if not (root / "matrix" / "matrix").is_file():
        raise SystemExit(f"not an ODB++ job root: {root}")

    job = odb_render.OdbJob(root, step=args.step)
    job.draw_text = not args.shipped_text_grammar  # else the loop below draws it the simpler way
    layers = job.layers
    have = {l["kicad"] for l in layers} | {str(l.get("odb", "")).lower() for l in layers}
    # generic rows: every matrix layer not yet mapped (MISC/DOCUMENT procmaps, etc.)
    for m in job.matrix:
        name = (m.get("name") or "").strip()
        if not name:
            continue
        k = name if m.get("context", "").upper() == "MISC" else name.lower()
        if k not in have and name.lower() not in have and m.get("type", "").upper() in ("DOCUMENT", "COMPONENT"):
            layers.append({"odb": name, "kicad": k, "type": m.get("type", "").upper(),
                           "negative": m.get("polarity", "").upper() == "NEGATIVE"})
            have.add(k)
    if args.layer:
        keep = set()
        for want in args.layer:
            w = want.lower()
            if w in ("edge.cuts", "profile"):
                keep.add("Edge.Cuts")
            for l in layers:
                if l["odb"].lower() == w or l["kicad"].lower() == w:
                    keep.add(l["kicad"])
        layers = [l for l in layers if l["kicad"] in keep]

    origin = stephdr_origin(job.step_dir) if args.apply_origin else None
    bb = job.profile_bbox()
    if bb is None:
        raise SystemExit("no profile contours found (cannot establish the view)")
    xmin, ymin, xmax, ymax = bb
    if args.layer:  # focus mode: grow the view to the selected layers' content (process maps extend past the outline)
        vo = __import__("valor_oracle")
        for l in layers:
            if l["kicad"] == "Edge.Cuts":
                continue
            fdir = job.layer_dir(l["odb"])
            if fdir is None or not odb_file(fdir / "features"):
                continue
            geo = vo.parse_feature_geometry(fdir / "features")
            s = 25.4 if geo["units"] == "INCH" else 1.0
            for rec in geo["records"]:
                t = rec[0]
                if t in ("P", "L", "A", "T", "V"):
                    n = {"P": 2, "L": 4, "A": 6, "T": 2, "V": 2}[t]
                    xs, ys = rec[1:n:2], rec[2:n + 1:2]
                    xmin = min(xmin, min(xs) * s)
                    xmax = max(xmax, max(xs) * s)
                    ymin = min(ymin, min(ys) * s)
                    ymax = max(ymax, max(ys) * s)
                    if t in ("T", "V"):
                        ext = (len(rec[10]) + 1) * max(rec[7], rec[8]) * s  # the string's length, either way
                        xmin -= ext
                        ymax += ext if t == "V" else 0
                        xmax += ext if t != "V" else 0
                elif t == "S":
                    for _hole, c in rec[2]:
                        for seg in c:
                            if seg[0] == "S":
                                xs, ys = [seg[1]], [seg[2]]
                            else:
                                _, xe, ye, xc, yc, _cw = seg
                                xs, ys = [xe, xc], [ye, yc]
                            xmin = min(xmin, min(xs) * s)
                            xmax = max(xmax, max(xs) * s)
                            ymin = min(ymin, min(ys) * s)
                            ymax = max(ymax, max(ys) * s)
    mx = max(0.5, (xmax - xmin) * 0.02)
    ox, oy = (origin or (0.0, 0.0))
    x0, y_top = xmin - mx + ox, ymax + mx + oy
    view = raster.View(x0, y_top, (xmax - xmin) + 2 * mx, (ymax - ymin) + 2 * mx, args.ppm, True)
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    board = board_view = None
    if args.kicad:
        from kicad_render import KicadBoard  # noqa: E402
        board = KicadBoard(Path(args.kicad).read_text(encoding="utf-8", errors="replace"))
        board_view = raster.View(x0, -y_top, (xmax - xmin) + 2 * mx, (ymax - ymin) + 2 * mx, args.ppm, False)

    rendered: list = []
    report = {"job": root.name, "step": job.step, "units": None, "stephdr_origin_mm": origin,
              "view_mm": [round(x0, 4), round(y_top, 4), round(view.w / view.ppm, 4), round(view.h / view.ppm, 4)],
              "ppm": args.ppm, "layers": []}
    pngs = []
    for l in layers:
        kn = l["kicad"]
        before = job.stats["text_strokes"]
        bm, drawn = job.render(kn, view)
        ntext = job.stats["text_strokes"] - before
        if kn != "Edge.Cuts" and args.shipped_text_grammar:
            for dname in drawn:
                fdir = job.layer_dir(dname)
                if fdir is None or not odb_file(fdir / "features"):
                    continue
                feats = fdir / "features"
                geo = __import__("valor_oracle").parse_feature_geometry(feats)
                fscale = 25.4 if geo["units"] == "INCH" else 1.0
                for rec in txt.parse_text_records_shipped(feats):
                    font = job._font(rec["font"])
                    if font is None:
                        continue
                    for val, cs in txt.text_contours(rec, font, fscale, None, view.ppm):
                        bm.fill(view.px(cs), 1 - val if l.get("negative") else val)
                        ntext += 1
        tag = kn.replace(".", "_").replace("/", "_")
        png = out / f"{job.step}__{tag}.png"
        txt.write_png(png, bm.w, bm.h, bm.buf)
        pngs.append(str(png))
        rendered.append((bm.w, bm.h, bytes(bm.buf)))
        entry = {"odb": l.get("odb"), "kicad": kn, "type": l.get("type"),
                 "negative": bool(l.get("negative")), "drawn": drawn,
                 "text_strokes": ntext, "ink_px": bm.count(), "png": str(png)}
        line = f"  {kn:16s} ink={bm.count():9d} px  text_strokes={ntext}  {png.name}"
        if board is not None:
            cmp = raster.compare(bm, board.render(kn, board_view), args.tol_px)
            entry["converter"] = {k: v for k, v in cmp.items() if not k.startswith("_")}
            entry["diff_png"] = str(out / f"{job.step}__{tag}__diff.png")
            raster.diff_image(bm.w, bm.h, cmp).save_png(entry["diff_png"])
            line += f"  | converter: xor {cmp['xor_ratio']:.4f} ({cmp['xor_px']} px) missing {cmp['missing_px']} px extra {cmp['extra_px']} px"
        report["layers"].append(entry)
        print(line)

    if args.sheet and rendered:
        W = max(w for w, _, _ in rendered)
        H = sum(h + 8 for _, h, _ in rendered)
        buf = bytearray([0] * (W * H))
        y = 0
        for w, h, rowbuf in rendered:
            for j in range(h):
                buf[y * W:y * W + w] = rowbuf[j * w:j * w + w]
                y += 1
            y += 8
        txt.write_png(args.sheet, W, H, buf)
        print(f"  sheet -> {args.sheet}")
    report["sheet"] = args.sheet
    rp = out / "render_report.json"
    rp.write_text(json.dumps(report, indent=1))
    print(f"report  -> {rp}")


if __name__ == "__main__":
    main()
