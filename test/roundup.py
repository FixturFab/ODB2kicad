#!/usr/bin/env python3
"""ODB++ round-up, phase 1: odb2kicad (and any other converter) vs an independent
ODB++ reference, per board and per layer, headless on Linux, Windows or the PC lane.

    python3 test/roundup.py <odb_dir_or_archive>... --out <dir>
        [--converter LABEL=native:<odb2kicad binary>]   (repeatable; default odb2kicad=native:odb2kicad/build/odb2kicad)
        [--converter LABEL=pcb:<file.kicad_pcb | dir holding <job>.kicad_pcb>]   (a board some other tool made)
        [--dpi 508] [--max-px 2400] [--tol-px 1] [--no-images]

Per job, in <out>/<job>/:
  report.json   per layer: reference ink vs each converter's ink -- xor_ratio (raw XOR / union),
                missing_ratio (reference ink farther than tol_px from the converter's, / reference ink),
                extra_ratio (the converse, / converter ink); compare_oracle.py's structural diff
                (components, feature counts, bbox, pin-1 positions); the outline alignment; and,
                when the Siemens viewer is reachable, its LIMITS / SYMS_HIST per layer.
  sheet.png     one row per layer: reference | <converter> | diff (grey both, red missing, green extra) ...
  layers/*.png  the same cells at full resolution (index.html shows them)
<out>/index.html  every job and layer, with a "better: ref / <converter> / same" choice and a comment
                  per layer and converter; "Download verdicts.json" saves them (no server).

Reference side: odb_render.py (valor_oracle.parse_feature_geometry records, ODB++ symbols,
polarity and units). Converter side: kicad_render.py. Both through raster.py at one scale; the
two frames are aligned by the outline (ODB++ profile bbox <-> KiCad Edge.Cuts bbox), and the
offset / size mismatch of that alignment is reported, not hidden.

``compare_job`` and ``metrics_only`` are the library entry points the PC-lane recipe
(dev-cloud/pc-recipes/odb-shootout, ``roundup: true``) calls: there the boards come from its own
converters and only ``metrics_only``'s numbers leave the PC.

Untrusted input: archives are unpacked with no absolute paths, no "..", no links, into a scratch
folder under --out (or --scratch); run with ``python3 -I``.
"""

from __future__ import annotations

import argparse
import html
import json
import os
import shutil
import socket
import subprocess
import sys
import tarfile
import tempfile
import time
import zipfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
if str(HERE) not in sys.path:
    sys.path.insert(0, str(HERE))  # python -I does not add the script's folder

import raster  # noqa: E402
from kicad_render import KicadBoard  # noqa: E402
from odb_render import OdbJob, layer_class  # noqa: E402
from valor_oracle import DirectOdbOracle, ValorClient, find_odb_root, parse_info_file  # noqa: E402

HARNESS_VERSION = "0.1.0"
CLASSES = ("cu_outer", "cu_inner", "mask", "paste", "silk", "edge", "drill", "doc")
ARCHIVE_EXT = (".tgz", ".tar.gz", ".zip", ".tar")


def log(msg: str) -> None:
    print(time.strftime("%H:%M:%S"), msg, flush=True)


# ----------------------------------------------------------------------------------------------
# inputs
# ----------------------------------------------------------------------------------------------


def safe_extract(archive: Path, dest: Path) -> None:
    """Untar / unzip regular files only, inside dest (the input is data, never trusted)."""
    dest.mkdir(parents=True, exist_ok=True)
    root = dest.resolve()
    if zipfile.is_zipfile(archive):
        with zipfile.ZipFile(archive) as z:
            for m in z.infolist():
                out = (dest / m.filename).resolve()
                if m.is_dir() or not str(out).startswith(str(root) + os.sep):
                    continue
                out.parent.mkdir(parents=True, exist_ok=True)
                with z.open(m) as src, open(out, "wb") as dst:
                    shutil.copyfileobj(src, dst)
        return
    with tarfile.open(archive) as t:
        for m in t.getmembers():
            out = (dest / m.name).resolve()
            if not m.isfile() or not str(out).startswith(str(root) + os.sep):
                continue
            out.parent.mkdir(parents=True, exist_ok=True)
            with t.extractfile(m) as src, open(out, "wb") as dst:
                shutil.copyfileobj(src, dst)


def job_name(p: Path) -> str:
    n = p.name
    for ext in ARCHIVE_EXT:
        if n.lower().endswith(ext):
            return n[: -len(ext)]
    return n


def prepare(inp: Path, scratch: Path) -> tuple:
    """-> (job name, ODB++ root folder). Archives are unpacked into scratch/<job>/."""
    name = job_name(inp)
    if inp.is_dir():
        root = find_odb_root(inp)
    else:
        dest = scratch / name
        if dest.exists():
            shutil.rmtree(dest)
        safe_extract(inp, dest)
        root = find_odb_root(dest)
    if root is None:
        raise ValueError(f"{name}: no matrix/matrix (not an ODB++ job)")
    return name, root


def convert_native(binary: Path, odb_root: Path, out_pcb: Path, timeout_s: int = 600) -> dict:
    t0 = time.time()
    try:
        r = subprocess.run([str(binary), str(odb_root), str(out_pcb)], capture_output=True, text=True, timeout=timeout_s)
        ok = r.returncode == 0 and out_pcb.is_file() and out_pcb.stat().st_size > 0
        return {"ok": ok, "error": None if ok else "convert_failed", "ms": int((time.time() - t0) * 1000),
                "stderr_tail": r.stderr[-2000:]}
    except subprocess.TimeoutExpired:
        return {"ok": False, "error": "timeout", "ms": timeout_s * 1000}


# ----------------------------------------------------------------------------------------------
# optional third column: the Siemens ODB++ Viewer through server.pl (valor_oracle.ValorClient)
# ----------------------------------------------------------------------------------------------


def siemens_column(job: OdbJob, odb_names: list) -> dict:
    """LIMITS + SYMS_HIST per layer from a running Siemens viewer, or {'skipped': why}.

    Needs ODBVIEWER_HOME set, an X display (DISPLAY, e.g. from Xvfb), server.pl listening on
    ODBVIEWER_PORT (56753), and the job already in the viewer's database under ODBVIEWER_JOB
    (default: the job folder's name). Info files are written to ODBVIEWER_TMP, which must be a
    folder both this process and the viewer can read (default: a temp dir here).
    """
    if not os.environ.get("ODBVIEWER_HOME"):
        return {"skipped": "ODBVIEWER_HOME not set"}
    if not os.environ.get("DISPLAY") and sys.platform != "win32":
        return {"skipped": "no X display (start Xvfb and export DISPLAY)"}
    host = os.environ.get("ODBVIEWER_HOST", "localhost")
    port = int(os.environ.get("ODBVIEWER_PORT", "56753"))
    try:
        socket.create_connection((host, port), timeout=3).close()
    except OSError as err:
        return {"skipped": f"viewer server not reachable on {host}:{port} ({err.__class__.__name__})"}
    cli = ValorClient(host, port)
    if not cli.connect():
        return {"skipped": "connect failed"}
    tmp = Path(os.environ.get("ODBVIEWER_TMP") or tempfile.mkdtemp(prefix="odbviewer-"))
    jname = os.environ.get("ODBVIEWER_JOB") or job.root.name
    out: dict = {"job": jname, "step": job.step, "layers": {}}
    try:
        st, res = cli.COM(f"open_job, job={jname}")
        if st != 0:
            return {"skipped": f"open_job failed ({res[:80]})"}
        cli.COM(f"open_entity, job={jname}, type=step, name={job.step}, iconic=yes")
        for lay in odb_names:
            ent = f"{jname}/{job.step}/{lay}"
            row: dict = {}
            for kind, extra in (("LIMITS", "units=mm, "), ("SYMS_HIST", "")):
                f = tmp / f"{lay}.{kind.lower()}.txt"
                st, _ = cli.COM(f"info, out_file={f.as_posix()}, write_mode=replace, {extra}args=-t layer -e {ent} -d {kind}")
                if st == 0 and f.is_file():
                    row[kind.lower()] = parse_info_file(str(f))
            out["layers"][lay] = row
    finally:
        cli.disconnect()
    return out


# ----------------------------------------------------------------------------------------------
# one job
# ----------------------------------------------------------------------------------------------


def _layer_order(names: set) -> list:
    def key(n):
        if n == "F.Cu":
            return (0, 0)
        if n.startswith("In") and n.endswith(".Cu"):
            return (0, int(n[2:-3]))
        if n == "B.Cu":
            return (0, 999)
        order = ["F.Mask", "B.Mask", "F.Paste", "B.Paste", "F.SilkS", "B.SilkS", "Edge.Cuts", "Drill"]
        return (1, order.index(n)) if n in order else (2, n)
    return sorted(names, key=key)


def structural(odb_root: Path, pcb: Path) -> list:
    """compare_oracle.py's structural diff, as plain dicts."""
    import compare_oracle  # noqa: PLC0415 - heavy; only when there is a board
    od = DirectOdbOracle(str(odb_root)).parse()
    kd = compare_oracle.KiCadPcbParser(str(pcb)).parse()
    res = compare_oracle.OracleComparator(tolerance=0.01).compare(od, kd)
    return [{"category": r.category, "passed": bool(r.passed), "message": r.message,
             "expected": r.expected, "actual": r.actual} for r in res]


def compare_job(odb_root: Path, boards: dict, out_dir: Path, *, dpi: float = 508.0, max_px: int = 2400,
                tol_px: int = 1, images: bool = True, siemens: bool = True, structural_diff: bool = True) -> dict:
    """The round-up for one job. boards: {label: {'pcb': Path | None, 'convert': {...}}}.

    Writes report.json (+ sheet.png and layers/*.png with images=True) under out_dir and returns
    the report. ``metrics_only(report)`` is what may leave a private machine.
    """
    out_dir.mkdir(parents=True, exist_ok=True)
    t0 = time.time()
    job = OdbJob(odb_root)
    pb = job.profile_bbox()
    loaded: dict = {}
    for label, b in boards.items():
        if b.get("pcb") and Path(b["pcb"]).is_file():
            try:
                loaded[label] = KicadBoard(Path(b["pcb"]).read_text(encoding="utf-8", errors="replace"))
            except Exception as err:  # noqa: BLE001 - an unreadable board is a result
                b.setdefault("convert", {})["parse_error"] = str(err)[:200]
    no_profile = pb is None
    if no_profile:  # no profile: a 100 mm window at the origin, flagged in the report
        pb = (-50.0, -50.0, 50.0, 50.0)
    bx0, by0, bx1, by1 = pb
    bw, bh = bx1 - bx0, by1 - by0
    margin = max(1.0, 0.03 * max(bw, bh))
    ppm = min(dpi / 25.4, max_px / (max(bw, bh) + 2 * margin))
    ref_view = raster.View(bx0 - margin, by1 + margin, bw + 2 * margin, bh + 2 * margin, ppm, True)

    report: dict = {"harness": HARNESS_VERSION, "job": out_dir.name, "step": job.step, "px_per_mm": round(ppm, 4),
                    "image_px": [ref_view.w, ref_view.h], "profile_bbox_mm": [round(v, 4) for v in pb],
                    "tol_px": tol_px, "no_profile": no_profile, "layer_map": job.layers, "boards": {}, "layers": []}
    views: dict = {}
    for label, b in boards.items():
        info = {"convert": b.get("convert") or {}, "align": None}
        kb = loaded.get(label)
        if kb is not None:
            eb = kb.edge_bbox(ppm)
            # odb2kicad's convention is KiCad (x, y) = ODB++ (x, -y). The outline bbox moves the frame
            # only when it has the profile's size (a converter that shifts the board is still compared);
            # an outline of another size (missing, extra or wrong edges) is reported, and the frame
            # falls back to the convention, so one bad edge does not misalign every other layer.
            x0, y_top = bx0 - margin, -(by1 + margin)
            info["align"] = {"edge": eb is not None, "used": "convention"}
            if eb is not None:
                dw, dh = (eb[2] - eb[0]) - bw, (eb[3] - eb[1]) - bh
                info["align"].update({"dx_mm": round(eb[0] - bx0, 4), "dy_mm": round(eb[1] + by1, 4),
                                      "dw_mm": round(dw, 4), "dh_mm": round(dh, 4)})
                if abs(dw) <= max(0.05, 0.002 * bw) and abs(dh) <= max(0.05, 0.002 * bh):
                    x0, y_top = eb[0] - margin, eb[1] - margin
                    info["align"]["used"] = "edge"
            views[label] = raster.View(x0, y_top, bw + 2 * margin, bh + 2 * margin, ppm, False)
            info["kicad_stats"] = dict(kb.stats)
            if structural_diff:
                try:
                    info["structural"] = structural(odb_root, Path(b["pcb"]))
                except Exception as err:  # noqa: BLE001
                    info["structural"] = [{"category": "error", "passed": False, "message": str(err)[:200]}]
        report["boards"][label] = info

    ref_names = {L["kicad"] for L in job.layers} | {"Edge.Cuts"}
    names = set(ref_names)
    for kb in loaded.values():  # converter layers the reference has no row for, in the compared classes
        for n, regs in kb.ops(ppm).items():
            if regs and layer_class(n) != "doc":
                names.add(n)
    names = _layer_order(names)
    if images:
        (out_dir / "layers").mkdir(exist_ok=True)
    rows_img = []
    for i, name in enumerate(names):
        ref_bm, drawn = job.render(name, ref_view) if name in ref_names else (raster.Bitmap(ref_view.w, ref_view.h), [])
        lay = {"kicad": name, "class": layer_class(name), "odb": drawn, "ref_px": ref_bm.count(), "by": {}}
        ref_mask = ref_bm.as_int()
        cells = [("ref", [(ref_mask, raster.INK["ref"])])] if images else []
        for label in boards:
            kb = loaded.get(label)
            if kb is None:
                lay["by"][label] = None
                continue
            got = kb.render(name, views[label])
            cmp = raster.compare(ref_bm, got, tol_px)
            lay["by"][label] = {k: v for k, v in cmp.items() if not k.startswith("_")}
            lay["by"][label]["present"] = cmp["got_px"] > 0
            if images:
                a_, b_, miss, extra = cmp["_masks"]
                cells.append((label, [(b_, raster.INK["got"])]))
                cells.append((label + " diff", [(a_ & b_, raster.BOTH), (miss, raster.MISS), (extra, raster.EXTRA)]))
        if images:
            stem = f"{i:02d}_{name.replace('.', '_')}"
            files = {}
            for cname, masks in cells:
                fn = f"{stem}_{cname.replace(' ', '_')}.png"
                raster._from_masks(ref_view.w, ref_view.h, masks).save_png(out_dir / "layers" / fn)
                files[cname] = f"layers/{fn}"
            lay["images"] = files
            if lay["ref_px"] or any(v and v["got_px"] for v in lay["by"].values()):  # empty layers: table only
                rows_img.append((name, lay, cells, ref_view.w, ref_view.h))
        report["layers"].append(lay)
        log(f"  {name:10s} ref {lay['ref_px']:>9d}px  " + "  ".join(
            f"{lb}: xor {v['xor_ratio']:.4f} miss {v['missing_ratio'] if v['missing_ratio'] is not None else '-'} extra {v['extra_ratio']:.4f}"
            if v else f"{lb}: no board" for lb, v in lay["by"].items()))
    report["reference_stats"] = {k: (dict(v) if isinstance(v, dict) else v) for k, v in job.stats.items()}
    report["siemens"] = siemens_column(job, [L["odb"] for L in job.layers]) if siemens else {"skipped": "disabled"}
    if "skipped" in report["siemens"]:
        log(f"  siemens: skipped ({report['siemens']['skipped']})")
    report["ms"] = int((time.time() - t0) * 1000)
    if images and rows_img:
        contact_sheet(rows_img, list(boards), out_dir / "sheet.png")
    (out_dir / "report.json").write_text(json.dumps(report, indent=1, default=str), encoding="utf-8")
    return report


def contact_sheet(rows: list, labels: list, path: Path, cell_w: int = 360) -> None:
    """reference | conv | conv diff | ... ; one row per layer, a title strip on each row."""
    ncol = 1 + 2 * len(labels)
    w0, h0 = rows[0][3], rows[0][4]
    f = max(1, -(-w0 // cell_w))
    cw, ch = w0 // f, h0 // f
    title_h, pad = 22, 6
    W = pad + ncol * (cw + pad)
    H = pad + 26 + len(rows) * (title_h + ch + pad)
    sheet = raster.Image(W, H, (8, 10, 16))
    heads = ["REFERENCE (ODB++)"] + [x for lb in labels for x in (lb, lb + " DIFF")]
    for c, t in enumerate(heads):
        sheet.text(pad + c * (cw + pad), pad + 4, t[: max(4, cw // 12)], (200, 200, 210), 2)
    y = pad + 26
    for name, lay, cells, w, h in rows:
        sub = "  ".join(f"{lb} XOR {v['xor_ratio']:.3f}" if v else f"{lb} NO BOARD" for lb, v in lay["by"].items())
        sheet.text(pad, y + 4, f"{name}  {sub}"[: W // 12], (230, 230, 120), 2)
        by_name = dict(cells)
        order = ["ref"] + [x for lb in labels for x in (lb, lb + " diff")]
        for c, key in enumerate(order):
            masks = by_name.get(key)
            if masks is None:
                sheet.rect(pad + c * (cw + pad), y + title_h, pad + c * (cw + pad) + cw, y + title_h + ch, (40, 20, 20))
                continue
            small = []
            for m, col in masks:
                sm, w2, h2 = raster.shrink(m, w, h, f)
                small.append((sm, col))
            sheet.paste(raster._from_masks(w // f, h // f, small), pad + c * (cw + pad), y + title_h)
        y += title_h + ch + pad
    sheet.save_png(path)


# ----------------------------------------------------------------------------------------------
# numbers only (what the PC lane's outbox carries)
# ----------------------------------------------------------------------------------------------


def metrics_only(report: dict) -> dict:
    """Per converter and layer class: pooled ratios and counts. No names, paths or geometry."""
    out: dict = {"ref_layers": sum(1 for L in report["layers"] if L["ref_px"] > 0),
                 "ref_unsupported_symbols": sum(report["reference_stats"]["unsupported_symbols"].values()),
                 "ref_text": report["reference_stats"]["text"], "ref_ms": report["ms"], "boards": {}}
    for label in report["boards"]:
        agg: dict = {}
        worst = 0.0
        missing_layers = extra_layers = 0
        for L in report["layers"]:
            v = L["by"].get(label)
            if not v:
                continue
            a = agg.setdefault(L["class"], {"ref": 0, "got": 0, "xor": 0, "union": 0, "miss": 0, "extra": 0})
            a["ref"] += v["ref_px"]
            a["got"] += v["got_px"]
            a["xor"] += v["xor_px"]
            a["union"] += int(round(v["xor_px"] / v["xor_ratio"])) if v["xor_ratio"] else (v["ref_px"] if v["xor_px"] == 0 else 0)
            a["miss"] += v["missing_px"]
            a["extra"] += v["extra_px"]
            if v["ref_px"] > 0 and v["got_px"] == 0:
                missing_layers += 1
            if v["ref_px"] == 0 and v["got_px"] > 0:
                extra_layers += 1
            if L["class"] != "doc":
                worst = max(worst, v["xor_ratio"])
        cls = {}
        for c, a in agg.items():
            cls[c] = {"xor": round(a["xor"] / a["union"], 6) if a["union"] else 0.0,
                      "miss": round(a["miss"] / a["ref"], 6) if a["ref"] else None,
                      "extra": round(a["extra"] / a["got"], 6) if a["got"] else 0.0}
        b = report["boards"][label]
        al = b.get("align") or {}
        struct = b.get("structural") or []
        out["boards"][label] = {"classes": cls, "worst_xor": round(worst, 6), "layers_missing": missing_layers,
                                "layers_extra": extra_layers, "edge": bool(al.get("edge")),
                                "frame_from_edge": al.get("used") == "edge",
                                "align_dx_mm": al.get("dx_mm"), "align_dy_mm": al.get("dy_mm"),
                                "size_dw_mm": al.get("dw_mm"), "size_dh_mm": al.get("dh_mm"),
                                "structural_failed": sum(1 for r in struct if not r.get("passed")),
                                "unfilled_zones": (b.get("kicad_stats") or {}).get("unfilled_zones")}
    return out


# ----------------------------------------------------------------------------------------------
# index.html
# ----------------------------------------------------------------------------------------------

INDEX_JS = r"""
const KEY = 'odb-roundup-verdicts:' + document.title;
const saved = JSON.parse(localStorage.getItem(KEY) || '{}');
function collect() {
  const out = {};
  document.querySelectorAll('[data-v]').forEach(box => {
    const id = box.dataset.v;
    const pick = box.querySelector('input[type=radio]:checked');
    const note = box.querySelector('textarea').value.trim();
    if (pick || note) out[id] = {better: pick ? pick.value : null, comment: note};
  });
  return out;
}
function persist() { localStorage.setItem(KEY, JSON.stringify(collect())); }
document.querySelectorAll('[data-v]').forEach(box => {
  const s = saved[box.dataset.v];
  if (s) {
    if (s.better) { const r = box.querySelector(`input[value="${s.better}"]`); if (r) r.checked = true; }
    box.querySelector('textarea').value = s.comment || '';
  }
  box.addEventListener('change', persist); box.addEventListener('input', persist);
});
document.getElementById('dl').onclick = () => {
  const doc = {generated: new Date().toISOString(), verdicts: collect()};
  const a = document.createElement('a');
  a.href = URL.createObjectURL(new Blob([JSON.stringify(doc, null, 1)], {type: 'application/json'}));
  a.download = 'verdicts.json'; a.click();
};
"""


def write_index(out: Path, reports: list) -> None:
    e = html.escape
    parts = ["<!doctype html><meta charset=utf-8><title>ODB++ round-up</title><style>"
             "body{font:13px system-ui;background:#0b1220;color:#ddd;margin:16px}a{color:#8cf}"
             "table{border-collapse:collapse;margin:6px 0}td,th{padding:2px 8px;border-bottom:1px solid #333;text-align:right}"
             ".row{display:flex;gap:8px;align-items:flex-start;margin:6px 0 14px}.row figure{margin:0;flex:1;min-width:0}"
             ".row img{width:100%;image-rendering:pixelated;background:#121620}figcaption{font-size:11px;color:#aaa}"
             ".v{border:1px solid #334;padding:6px;margin:4px 0;font-size:12px}.v textarea{width:100%;height:36px;background:#111;color:#ddd}"
             "h3{margin:18px 0 4px}.bad{color:#f88}.ok{color:#8f8}#dl{position:sticky;top:0;padding:6px 12px;font-size:14px}"
             "</style>",
             "<button id=dl>Download verdicts.json</button>",
             "<h1>ODB++ round-up</h1><p>Reference = the ODB++ features drawn straight from the files (test/odb_render.py). "
             "Diff: grey = both, <span style=color:#f55>red = missing from the converter</span>, "
             "<span style=color:#4d8>green = extra in the converter</span>. Your picks are kept in this browser and "
             "saved with the button.</p>"]
    for rep in reports:
        job = rep["job"]
        labels = list(rep["boards"])
        parts.append(f"<h2 id='{e(job)}'>{e(job)}</h2><table><tr><th>layer</th><th>class</th><th>ref px</th>"
                     + "".join(f"<th>{e(lb)} xor</th><th>miss</th><th>extra</th>" for lb in labels) + "</tr>")
        for L in rep["layers"]:
            cells = ""
            for lb in labels:
                v = L["by"].get(lb)
                if not v:
                    cells += "<td colspan=3>no board</td>"
                    continue
                cls = "bad" if v["xor_ratio"] > 0.05 else "ok"
                miss = "-" if v["missing_ratio"] is None else f"{v['missing_ratio']:.4f}"
                cells += f"<td class={cls}>{v['xor_ratio']:.4f}</td><td>{miss}</td><td>{v['extra_ratio']:.4f}</td>"
            parts.append(f"<tr><td><a href='#{e(job)}-{e(L['kicad'])}'>{e(L['kicad'])}</a></td><td>{e(L['class'])}</td>"
                         f"<td>{L['ref_px']}</td>{cells}</tr>")
        parts.append("</table>")
        for lb, b in rep["boards"].items():
            al = b.get("align") or {}
            c = b.get("convert") or {}
            parts.append(f"<p>{e(lb)}: convert {'ok' if c.get('ok', True) else e(str(c.get('error')))}"
                         f"{' in %d ms' % c['ms'] if c.get('ms') else ''}; outline "
                         f"{('dx %.3f dy %.3f dw %.3f dh %.3f mm, frame from the %s' % (al['dx_mm'], al['dy_mm'], al['dw_mm'], al['dh_mm'], 'outline' if al.get('used') == 'edge' else "convention (x, -y): outline size differs")) if al.get('edge') else 'missing (frame from the convention x, -y)'}"
                         f"; structural fails {sum(1 for r in (b.get('structural') or []) if not r.get('passed'))}</p>")
        for L in rep["layers"]:
            if not L.get("images"):
                continue
            lid = f"{job}/{L['kicad']}"
            parts.append(f"<h3 id='{e(job)}-{e(L['kicad'])}'>{e(job)} · {e(L['kicad'])} <small>({e(', '.join(L['odb']) or 'no ODB++ layer')})</small></h3><div class=row>")
            for key, src in L["images"].items():
                parts.append(f"<figure><img loading=lazy src='{e(job)}/{e(src)}'><figcaption>{e(key)}</figcaption></figure>")
            parts.append("</div>")
            for lb in labels:
                vid = e(f"{lid}::{lb}")
                parts.append(f"<div class=v data-v='{vid}'>{e(lb)} — better: "
                             + "".join(f"<label><input type=radio name='{vid}' value='{val}'> {txt}</label> "
                                       for val, txt in (("ref", "reference"), (e(lb), e(lb)), ("same", "same")))
                             + "<textarea placeholder='what looks wrong, and where'></textarea></div>")
    parts.append(f"<script>{INDEX_JS}</script>")
    (out / "index.html").write_text("\n".join(parts), encoding="utf-8")


# ----------------------------------------------------------------------------------------------
# CLI
# ----------------------------------------------------------------------------------------------


def parse_converters(specs: list) -> dict:
    out = {}
    for s in specs:
        label, _, rest = s.partition("=")
        kind, _, path = rest.partition(":")
        if not label or kind not in ("native", "pcb") or not path:
            raise SystemExit(f"--converter {s!r}: expected LABEL=native:<binary> or LABEL=pcb:<file|dir>")
        out[label] = (kind, Path(path))
    return out


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("inputs", nargs="+", type=Path, help="ODB++ folders or archives (.tgz/.tar.gz/.zip/.tar)")
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--converter", action="append", default=[])
    ap.add_argument("--scratch", type=Path, help="where archives are unpacked (default <out>/.scratch)")
    ap.add_argument("--dpi", type=float, default=508.0, help="raster resolution (default 508 = 20 px/mm)")
    ap.add_argument("--max-px", type=int, default=2400, help="cap on the longest image side")
    ap.add_argument("--tol-px", type=int, default=1, help="missing/extra ignore ink within this many px of the other side")
    ap.add_argument("--no-images", action="store_true", help="numbers only (report.json without sheet / layer PNGs)")
    ap.add_argument("--no-siemens", action="store_true")
    ap.add_argument("--timeout", type=int, default=600)
    a = ap.parse_args(argv)

    convs = parse_converters(a.converter or [f"odb2kicad=native:{HERE.parent / 'odb2kicad' / 'build' / 'odb2kicad'}"])
    for label, (kind, p) in convs.items():
        if kind == "native" and not p.is_file():
            raise SystemExit(f"{label}: {p} not found (build: cmake -S odb2kicad -B odb2kicad/build && cmake --build odb2kicad/build -j)")
    a.out.mkdir(parents=True, exist_ok=True)
    scratch = a.scratch or (a.out / ".scratch")
    reports, failed = [], 0
    for inp in a.inputs:
        try:
            name, root = prepare(inp, scratch)
        except Exception as err:  # noqa: BLE001
            log(f"{inp}: {err}")
            failed += 1
            continue
        jd = a.out / name
        jd.mkdir(parents=True, exist_ok=True)
        log(f"{name}: {root}")
        boards = {}
        for label, (kind, p) in convs.items():
            if kind == "native":
                pcb = jd / f"{label}.kicad_pcb"
                c = convert_native(p, root, pcb, a.timeout)
                log(f"  {label}: {'ok' if c['ok'] else c['error']} in {c['ms']} ms")
                boards[label] = {"pcb": pcb if c["ok"] else None, "convert": c}
            else:
                pcb = p / f"{name}.kicad_pcb" if p.is_dir() else p
                boards[label] = {"pcb": pcb if pcb.is_file() else None, "convert": {"ok": pcb.is_file(), "error": None if pcb.is_file() else "missing"}}
        try:
            reports.append(compare_job(root, boards, jd, dpi=a.dpi, max_px=a.max_px, tol_px=a.tol_px,
                                       images=not a.no_images, siemens=not a.no_siemens))
        except Exception as err:  # noqa: BLE001
            import traceback
            traceback.print_exc()
            log(f"{name}: reference failed: {err}")
            failed += 1
    write_index(a.out, reports)
    (a.out / "metrics.json").write_text(json.dumps({r["job"]: metrics_only(r) for r in reports}, indent=1), encoding="utf-8")
    log(f"done: {len(reports)} job(s), {failed} failed -> {a.out / 'index.html'}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
