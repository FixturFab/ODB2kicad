#!/usr/bin/env python3
"""
Sanitize ODB++ directories for safe inclusion in test suites.

Replaces all identifying names/metadata with generic tokens while preserving
geometry exactly. Optionally chunks boards into smaller standalone test cases.

Usage:
    python3 tools/sanitize_odb.py /path/to/odb samples/sanitized
    python3 tools/sanitize_odb.py /path/to/odb samples/chunk --bbox 90 100 150 120
    python3 tools/sanitize_odb.py /path/to/odb samples/grid --grid 3x2
"""

import argparse
import os
import re
import shutil
import sys
from pathlib import Path


# ---------------------------------------------------------------------------
# NameMap — deterministic sequential renaming
# ---------------------------------------------------------------------------

class NameMap:
    """Maps original names to sequential generic names."""

    def __init__(self, prefix, width=0):
        self._map = {}
        self._prefix = prefix
        self._width = width
        self._counter = 0

    def get(self, original):
        if original in self._map:
            return self._map[original]
        name = f"{self._prefix}{str(self._counter).zfill(self._width)}" if self._width else f"{self._prefix}{self._counter}"
        self._map[original] = name
        self._counter += 1
        return name

    def items(self):
        return self._map.items()

    def __contains__(self, key):
        return key in self._map

    def __getitem__(self, key):
        return self._map[key]

    def __len__(self):
        return len(self._map)


# ---------------------------------------------------------------------------
# Shared name maps (cross-file consistency)
# ---------------------------------------------------------------------------

class SanitizeContext:
    def __init__(self):
        self.net_names = NameMap("NET_", 3)
        self.pkg_names = NameMap("PKG_", 3)
        self.refdes_names = NameMap("CMP_", 4)
        self.footprint_names = NameMap("FP_", 3)
        self.string_names = NameMap("STR_", 3)


# ---------------------------------------------------------------------------
# Per-file sanitizers
# ---------------------------------------------------------------------------

def sanitize_info(src_path, dst_path, ctx):
    """Sanitize misc/info — scrub JOB_NAME, dates, SAVE_APP; preserve UNITS, ODB_VERSION."""
    lines = src_path.read_text().splitlines()
    out = []
    for line in lines:
        if line.startswith("JOB_NAME="):
            out.append("JOB_NAME=sanitized")
        elif line.startswith("CREATION_DATE="):
            out.append("CREATION_DATE=20000101.000000")
        elif line.startswith("SAVE_DATE="):
            out.append("SAVE_DATE=20000101.000000")
        elif line.startswith("SAVE_APP="):
            out.append("SAVE_APP=sanitized")
        elif line.startswith("ODB_SOURCE="):
            out.append("ODB_SOURCE=sanitized")
        else:
            out.append(line)
    dst_path.parent.mkdir(parents=True, exist_ok=True)
    dst_path.write_text("\n".join(out) + "\n")


def sanitize_eda_data(src_path, dst_path, ctx):
    """Sanitize eda/data — replace NET/PKG names, preserve all geometry."""
    lines = src_path.read_text().splitlines()
    out = []
    for line in lines:
        stripped = line.strip()
        if stripped.startswith("HDR "):
            out.append("HDR sanitized")
        elif stripped.startswith("NET "):
            # NET <name> — replace name, keep index from preceding #NET comment
            name = stripped[4:].strip()
            if name == "$NONE$":
                out.append("NET $NONE$ ")
            else:
                out.append(f"NET {ctx.net_names.get(name)} ")
        elif stripped.startswith("PKG "):
            # PKG <name> <geometry...>
            parts = stripped.split(None, 1)
            rest = parts[1] if len(parts) > 1 else ""
            # Split: first token after PKG is name, rest is geometry
            tokens = rest.split(None, 1)
            if tokens:
                pkg_name = tokens[0]
                geom = tokens[1] if len(tokens) > 1 else ""
                out.append(f"PKG {ctx.pkg_names.get(pkg_name)} {geom}")
            else:
                out.append(line)
        else:
            out.append(line)
    dst_path.parent.mkdir(parents=True, exist_ok=True)
    dst_path.write_text("\n".join(out) + "\n")


def sanitize_netlist(src_path, dst_path, ctx):
    """Sanitize netlists/cadnet/netlist — replace $idx names, preserve coordinates."""
    lines = src_path.read_text().splitlines()
    out = []
    for line in lines:
        stripped = line.strip()
        if stripped.startswith("$"):
            # $<idx> <name> — replace name
            m = re.match(r'^(\$\d+)\s+(.+)$', stripped)
            if m:
                idx = m.group(1)
                name = m.group(2)
                if name == "$NONE$":
                    out.append(f"{idx} $NONE$")
                else:
                    out.append(f"{idx} {ctx.net_names.get(name)}")
            else:
                out.append(line)
        elif stripped.startswith("H "):
            out.append(line)
        else:
            out.append(line)
    dst_path.parent.mkdir(parents=True, exist_ok=True)
    dst_path.write_text("\n".join(out) + "\n")


def sanitize_components(src_path, dst_path, ctx):
    """Sanitize components — replace refdes/footprint/PRP values, preserve geometry."""
    lines = src_path.read_text().splitlines()
    out = []
    for line in lines:
        stripped = line.strip()
        if stripped.startswith("CMP "):
            # CMP <pkg_idx> <x> <y> <rot> <mirror> <refdes> <footprint> <;attrs>
            # We need to replace refdes and footprint
            m = re.match(
                r'^(CMP\s+\d+\s+[\d.eE+-]+\s+[\d.eE+-]+\s+[\d.eE+-]+\s+\w+)\s+'
                r'(\S+)\s+(\S+)(.*)',
                stripped
            )
            if m:
                prefix = m.group(1)  # CMP <pkg_idx> <x> <y> <rot> <mirror>
                refdes = m.group(2)
                footprint = m.group(3)
                rest = m.group(4)    # ;attrs
                new_ref = ctx.refdes_names.get(refdes)
                new_fp = ctx.footprint_names.get(footprint)
                out.append(f"{prefix} {new_ref} {new_fp}{rest}")
            else:
                out.append(line)
        elif stripped.startswith("PRP "):
            # PRP <key> '<value>' — blank the value
            m = re.match(r"^(PRP\s+\S+)\s+'.*'$", stripped)
            if m:
                out.append(f"{m.group(1)} ''")
            else:
                out.append(line)
        else:
            # TOP, #, UNITS, @, etc. — preserve as-is (all geometry)
            out.append(line)
    dst_path.parent.mkdir(parents=True, exist_ok=True)
    dst_path.write_text("\n".join(out) + "\n")


def sanitize_features(src_path, dst_path, ctx):
    """Sanitize features — replace &idx text strings, preserve $idx symbols and all geometry."""
    lines = src_path.read_text().splitlines()
    out = []
    for line in lines:
        stripped = line.strip()
        if stripped.startswith("&"):
            # &<idx> <text> — replace text
            m = re.match(r'^(&\d+)\s+(.+)$', stripped)
            if m:
                idx = m.group(1)
                text = m.group(2)
                out.append(f"{idx} {ctx.string_names.get(text)}")
            else:
                out.append(line)
        else:
            out.append(line)
    dst_path.parent.mkdir(parents=True, exist_ok=True)
    dst_path.write_text("\n".join(out) + "\n")


def copy_verbatim(src_path, dst_path):
    """Copy a file verbatim."""
    dst_path.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src_path, dst_path)


# ---------------------------------------------------------------------------
# Orchestrator — discover structure and apply sanitizers
# ---------------------------------------------------------------------------

def discover_steps(odb_root):
    """Return list of step names found under steps/."""
    steps_dir = odb_root / "steps"
    if not steps_dir.is_dir():
        return []
    return [d.name for d in steps_dir.iterdir() if d.is_dir()]


def discover_layers(step_dir):
    """Return list of layer directory names."""
    layers_dir = step_dir / "layers"
    if not layers_dir.is_dir():
        return []
    return [d.name for d in layers_dir.iterdir() if d.is_dir()]


def sanitize_job(src, dst):
    """Sanitize an entire ODB++ job directory."""
    src = Path(src)
    dst = Path(dst)
    ctx = SanitizeContext()

    if dst.exists():
        shutil.rmtree(dst)
    dst.mkdir(parents=True)

    # Pass 1: Collect net names from eda/data first (to ensure consistent mapping)
    # We process eda/data before netlist so that NET names get the same mapping
    steps = discover_steps(src)
    if not steps:
        print(f"Error: no steps found in {src}/steps/", file=sys.stderr)
        sys.exit(1)

    for step_name in steps:
        step_src = src / "steps" / step_name
        step_dst = dst / "steps" / step_name

        # --- eda/data ---
        eda_src = step_src / "eda" / "data"
        if eda_src.is_file():
            sanitize_eda_data(eda_src, step_dst / "eda" / "data", ctx)

        # --- netlists/cadnet/netlist ---
        netlist_src = step_src / "netlists" / "cadnet" / "netlist"
        if netlist_src.is_file():
            sanitize_netlist(netlist_src, step_dst / "netlists" / "cadnet" / "netlist", ctx)

        # --- layers ---
        for layer_name in discover_layers(step_src):
            layer_src = step_src / "layers" / layer_name
            layer_dst = step_dst / "layers" / layer_name

            # components file
            comp_src = layer_src / "components"
            if comp_src.is_file():
                sanitize_components(comp_src, layer_dst / "components", ctx)

            # features file
            feat_src = layer_src / "features"
            if feat_src.is_file():
                sanitize_features(feat_src, layer_dst / "features", ctx)

            # Copy anything else in the layer dir verbatim
            for f in layer_src.iterdir():
                if f.name not in ("components", "features") and f.is_file():
                    copy_verbatim(f, layer_dst / f.name)

        # --- Copy other step contents verbatim ---
        for item in step_src.iterdir():
            dst_item = step_dst / item.name
            if item.name in ("eda", "netlists", "layers"):
                continue  # already handled
            if item.is_dir():
                if not dst_item.exists():
                    shutil.copytree(item, dst_item)
            elif item.is_file():
                copy_verbatim(item, dst_item)

    # --- misc/info ---
    info_src = src / "misc" / "info"
    if info_src.is_file():
        sanitize_info(info_src, dst / "misc" / "info", ctx)

    # --- Copy everything else at top level verbatim ---
    for item in src.iterdir():
        dst_item = dst / item.name
        if item.name in ("steps", "misc"):
            continue  # already handled
        if dst_item.exists():
            continue
        if item.is_dir():
            shutil.copytree(item, dst_item)
        elif item.is_file():
            copy_verbatim(item, dst_item)

    # Copy misc/ files other than info
    misc_src = src / "misc"
    if misc_src.is_dir():
        for item in misc_src.iterdir():
            if item.name == "info":
                continue
            dst_item = dst / "misc" / item.name
            if not dst_item.exists():
                if item.is_dir():
                    shutil.copytree(item, dst_item)
                elif item.is_file():
                    copy_verbatim(item, dst_item)

    print(f"Sanitized: {src} -> {dst}")
    print(f"  Nets: {len(ctx.net_names)}, Packages: {len(ctx.pkg_names)}, "
          f"Components: {len(ctx.refdes_names)}, Footprints: {len(ctx.footprint_names)}, "
          f"Strings: {len(ctx.string_names)}")
    return ctx


# ---------------------------------------------------------------------------
# Chunking — extract a bounding box region as standalone ODB++ test case
# ---------------------------------------------------------------------------

def parse_profile(profile_path):
    """Parse profile to get board outline bounding box. Returns (x_min, y_min, x_max, y_max)."""
    xs, ys = [], []
    for line in profile_path.read_text().splitlines():
        stripped = line.strip()
        if stripped.startswith("OS ") or stripped.startswith("OB "):
            parts = stripped.split()
            if len(parts) >= 3:
                try:
                    xs.append(float(parts[1]))
                    ys.append(float(parts[2]))
                except ValueError:
                    pass
    if not xs:
        return None
    return min(xs), min(ys), max(xs), max(ys)


def point_in_bbox(x, y, bbox, margin=1.0):
    """Check if point is within bbox (with margin)."""
    return (bbox[0] - margin <= x <= bbox[2] + margin and
            bbox[1] - margin <= y <= bbox[3] + margin)


def chunk_components(src_path, dst_path, bbox):
    """Filter components to those within bbox. Returns set of included (cmp_idx, pkg_idx) and used net indices."""
    lines = src_path.read_text().splitlines()
    out = []
    included_cmps = set()
    used_nets = set()
    used_pkgs = set()
    current_cmp_idx = None
    current_pkg_idx = None
    include_current = False
    cmp_lines = []

    def flush_cmp():
        nonlocal include_current, cmp_lines, current_cmp_idx, current_pkg_idx
        if include_current and cmp_lines:
            included_cmps.add(current_cmp_idx)
            used_pkgs.add(current_pkg_idx)
            out.extend(cmp_lines)
        cmp_lines = []
        include_current = False

    for line in lines:
        stripped = line.strip()
        if stripped.startswith("# CMP "):
            flush_cmp()
            m = re.match(r'^# CMP (\d+)', stripped)
            if m:
                current_cmp_idx = int(m.group(1))
            cmp_lines = [line]
        elif stripped.startswith("CMP "):
            m = re.match(
                r'^CMP\s+(\d+)\s+([\d.eE+-]+)\s+([\d.eE+-]+)',
                stripped
            )
            if m:
                current_pkg_idx = int(m.group(1))
                x, y = float(m.group(2)), float(m.group(3))
                include_current = point_in_bbox(x, y, bbox)
            cmp_lines.append(line)
        elif stripped.startswith("TOP "):
            cmp_lines.append(line)
            if include_current:
                # Extract net index from TOP line
                # TOP <pin_idx> <x> <y> <rot> <mirror> <net_idx> <subnet_idx> <pin_num>
                parts = stripped.split()
                if len(parts) >= 7:
                    try:
                        used_nets.add(int(parts[6]))
                    except ValueError:
                        pass
        elif stripped == "#":
            cmp_lines.append(line)
        elif current_cmp_idx is not None:
            cmp_lines.append(line)
        else:
            out.append(line)

    flush_cmp()

    dst_path.parent.mkdir(parents=True, exist_ok=True)
    dst_path.write_text("\n".join(out) + "\n")
    return included_cmps, used_nets, used_pkgs


def chunk_features(src_path, dst_path, bbox):
    """Filter features to those within/overlapping bbox."""
    lines = src_path.read_text().splitlines()
    out = []
    in_surface = False
    surface_lines = []
    surface_xs = []
    surface_ys = []

    def flush_surface():
        nonlocal surface_lines, surface_xs, surface_ys, in_surface
        if surface_xs:
            s_bbox = (min(surface_xs), min(surface_ys), max(surface_xs), max(surface_ys))
            # Check overlap with chunk bbox
            if not (s_bbox[2] < bbox[0] - 1 or s_bbox[0] > bbox[2] + 1 or
                    s_bbox[3] < bbox[1] - 1 or s_bbox[1] > bbox[3] + 1):
                out.extend(surface_lines)
        surface_lines = []
        surface_xs = []
        surface_ys = []
        in_surface = False

    feature_count = 0
    in_header = True

    for line in lines:
        stripped = line.strip()

        # Always include header lines (UNITS, #, F, $, @, &, empty)
        if in_header:
            if (stripped == "" or stripped.startswith("#") or stripped.startswith("UNITS") or
                    stripped.startswith("F ") or stripped.startswith("$") or
                    stripped.startswith("@") or stripped.startswith("&")):
                out.append(line)
                continue
            else:
                in_header = False

        # Surface start
        if stripped.startswith("S "):
            flush_surface()
            in_surface = True
            surface_lines = [line]
            continue

        if in_surface:
            surface_lines.append(line)
            if stripped.startswith("OS ") or stripped.startswith("OB "):
                parts = stripped.split()
                if len(parts) >= 3:
                    try:
                        surface_xs.append(float(parts[1]))
                        surface_ys.append(float(parts[2]))
                    except ValueError:
                        pass
            if stripped == "SE":
                flush_surface()
            continue

        # Point features: P x y ...
        if stripped.startswith("P "):
            parts = stripped.split()
            if len(parts) >= 3:
                try:
                    x, y = float(parts[1]), float(parts[2])
                    if point_in_bbox(x, y, bbox):
                        out.append(line)
                        feature_count += 1
                except ValueError:
                    out.append(line)
            continue

        # Line features: L x1 y1 x2 y2 ...
        if stripped.startswith("L "):
            parts = stripped.split()
            if len(parts) >= 5:
                try:
                    x1, y1 = float(parts[1]), float(parts[2])
                    x2, y2 = float(parts[3]), float(parts[4])
                    if point_in_bbox(x1, y1, bbox) or point_in_bbox(x2, y2, bbox):
                        out.append(line)
                        feature_count += 1
                except ValueError:
                    out.append(line)
            continue

        # Arc features: A x1 y1 x2 y2 xc yc ...
        if stripped.startswith("A "):
            parts = stripped.split()
            if len(parts) >= 7:
                try:
                    x1, y1 = float(parts[1]), float(parts[2])
                    x2, y2 = float(parts[3]), float(parts[4])
                    if point_in_bbox(x1, y1, bbox) or point_in_bbox(x2, y2, bbox):
                        out.append(line)
                        feature_count += 1
                except ValueError:
                    out.append(line)
            continue

        # Anything else, keep
        out.append(line)

    flush_surface()

    # Update feature count in header
    final = []
    for line in out:
        if line.strip().startswith("F "):
            final.append(f"F {feature_count}")
        else:
            final.append(line)

    dst_path.parent.mkdir(parents=True, exist_ok=True)
    dst_path.write_text("\n".join(final) + "\n")


def chunk_eda_data(src_path, dst_path, used_nets, used_pkgs):
    """Filter eda/data to only include referenced nets and packages."""
    lines = src_path.read_text().splitlines()
    out = []
    current_net_idx = None
    include_net = True
    current_pkg_idx = None
    include_pkg = True
    in_pkg_block = False

    for line in lines:
        stripped = line.strip()

        # Track NET sections
        m = re.match(r'^#NET\s+(\d+)', stripped)
        if m:
            current_net_idx = int(m.group(1))
            include_net = current_net_idx in used_nets
            in_pkg_block = False
            if include_net:
                out.append(line)
            continue

        if current_net_idx is not None and not in_pkg_block:
            if stripped.startswith("NET ") or stripped.startswith("SNT ") or stripped.startswith("FID "):
                if include_net:
                    out.append(line)
                continue

        # Track PKG sections
        m = re.match(r'^#\s*PKG\s+(\d+)', stripped)
        if m:
            current_pkg_idx = int(m.group(1))
            include_pkg = current_pkg_idx in used_pkgs
            in_pkg_block = True
            current_net_idx = None
            if include_pkg:
                out.append(line)
            continue

        if in_pkg_block:
            if include_pkg:
                out.append(line)
            # PKG block ends at next #NET or #PKG or end of file
            continue

        # Header/comment lines
        out.append(line)

    dst_path.parent.mkdir(parents=True, exist_ok=True)
    dst_path.write_text("\n".join(out) + "\n")


def chunk_netlist(src_path, dst_path, used_nets):
    """Filter netlist to only include referenced nets."""
    lines = src_path.read_text().splitlines()
    out = []
    for line in lines:
        stripped = line.strip()
        if stripped.startswith("$"):
            m = re.match(r'^\$(\d+)\s+', stripped)
            if m:
                idx = int(m.group(1))
                if idx in used_nets:
                    out.append(line)
                continue
        elif stripped and stripped[0].isdigit():
            # Net point row: <net_idx> ...
            parts = stripped.split()
            if parts:
                try:
                    idx = int(parts[0])
                    if idx in used_nets:
                        out.append(line)
                    continue
                except ValueError:
                    pass
        out.append(line)
    dst_path.parent.mkdir(parents=True, exist_ok=True)
    dst_path.write_text("\n".join(out) + "\n")


def make_bbox_profile(dst_path, bbox):
    """Create a simple rectangular profile from bbox."""
    x1, y1, x2, y2 = bbox
    content = f"""UNITS=MM
#
#Num Features
#
F 1
#
#Layer features
#
S P 0
OB {x1} {y1} I
OS {x1} {y1}
OS {x2} {y1}
OS {x2} {y2}
OS {x1} {y2}
OS {x1} {y1}
OE
SE
"""
    dst_path.parent.mkdir(parents=True, exist_ok=True)
    dst_path.write_text(content)


def chunk_job(src, dst, bbox):
    """Extract a bounding box region from a (already sanitized) ODB++ directory."""
    src = Path(src)
    dst = Path(dst)

    if dst.exists():
        shutil.rmtree(dst)

    # Copy everything first, then overwrite filtered files
    shutil.copytree(src, dst)

    steps = discover_steps(src)
    for step_name in steps:
        step_src = src / "steps" / step_name
        step_dst = dst / "steps" / step_name

        # Chunk components — also gives us which nets/pkgs are referenced
        used_nets = set()
        used_pkgs = set()
        for layer_name in discover_layers(step_src):
            comp_src = step_src / "layers" / layer_name / "components"
            if comp_src.is_file():
                _, nets, pkgs = chunk_components(
                    comp_src, step_dst / "layers" / layer_name / "components", bbox
                )
                used_nets.update(nets)
                used_pkgs.update(pkgs)

        # Always include net 0 ($NONE$)
        used_nets.add(0)

        # Chunk features in all layers
        for layer_name in discover_layers(step_src):
            feat_src = step_src / "layers" / layer_name / "features"
            if feat_src.is_file():
                chunk_features(feat_src, step_dst / "layers" / layer_name / "features", bbox)

        # Chunk eda/data
        eda_src = step_src / "eda" / "data"
        if eda_src.is_file():
            chunk_eda_data(eda_src, step_dst / "eda" / "data", used_nets, used_pkgs)

        # Chunk netlist
        netlist_src = step_src / "netlists" / "cadnet" / "netlist"
        if netlist_src.is_file():
            chunk_netlist(netlist_src, step_dst / "netlists" / "cadnet" / "netlist", used_nets)

        # Replace profile with bbox rectangle
        profile_dst = step_dst / "profile"
        make_bbox_profile(profile_dst, bbox)

    print(f"Chunked: {src} -> {dst} (bbox: {bbox})")
    print(f"  Included nets: {sorted(used_nets)}, packages: {sorted(used_pkgs)}")


# ---------------------------------------------------------------------------
# Grid helper
# ---------------------------------------------------------------------------

def grid_chunks(src, dst_prefix, cols, rows):
    """Divide board into grid cells and produce one chunk per cell."""
    src = Path(src)
    steps = discover_steps(src)
    if not steps:
        print("Error: no steps found", file=sys.stderr)
        sys.exit(1)

    # Find profile in first step
    profile_path = src / "steps" / steps[0] / "profile"
    if not profile_path.is_file():
        print(f"Error: no profile at {profile_path}", file=sys.stderr)
        sys.exit(1)

    board_bbox = parse_profile(profile_path)
    if board_bbox is None:
        print("Error: could not parse profile bounding box", file=sys.stderr)
        sys.exit(1)

    x_min, y_min, x_max, y_max = board_bbox
    cell_w = (x_max - x_min) / cols
    cell_h = (y_max - y_min) / rows

    print(f"Board bbox: ({x_min}, {y_min}) - ({x_max}, {y_max})")
    print(f"Grid: {cols}x{rows}, cell size: {cell_w:.2f} x {cell_h:.2f}")

    chunk_idx = 0
    for row in range(rows):
        for col in range(cols):
            cx_min = x_min + col * cell_w
            cy_min = y_min + row * cell_h
            cx_max = cx_min + cell_w
            cy_max = cy_min + cell_h
            bbox = (cx_min, cy_min, cx_max, cy_max)
            chunk_dst = Path(f"{dst_prefix}-{chunk_idx}")
            chunk_job(src, chunk_dst, bbox)
            chunk_idx += 1


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="Sanitize ODB++ directories for safe test inclusion."
    )
    parser.add_argument("src", help="Source ODB++ directory")
    parser.add_argument("dst", help="Destination directory")
    parser.add_argument(
        "--bbox", nargs=4, type=float, metavar=("X_MIN", "Y_MIN", "X_MAX", "Y_MAX"),
        help="Chunk by bounding box (applied after sanitization)"
    )
    parser.add_argument(
        "--grid", type=str, metavar="COLSxROWS",
        help="Auto-grid chunking, e.g. 3x2 (applied after sanitization)"
    )
    parser.add_argument(
        "--no-sanitize", action="store_true",
        help="Skip sanitization, only chunk (src must already be sanitized)"
    )
    args = parser.parse_args()

    src = Path(args.src).resolve()
    dst = Path(args.dst).resolve()

    if not src.is_dir():
        print(f"Error: source is not a directory: {src}", file=sys.stderr)
        sys.exit(1)

    if args.no_sanitize:
        sanitized = src
    else:
        # If chunking, sanitize to a temp location then chunk
        if args.bbox or args.grid:
            import tempfile
            tmp = Path(tempfile.mkdtemp()) / "sanitized"
            sanitize_job(src, tmp)
            sanitized = tmp
        else:
            sanitize_job(src, dst)
            return

    if args.bbox:
        bbox = tuple(args.bbox)
        chunk_job(sanitized, dst, bbox)
    elif args.grid:
        m = re.match(r'^(\d+)x(\d+)$', args.grid)
        if not m:
            print(f"Error: invalid grid format '{args.grid}', expected COLSxROWS (e.g. 3x2)",
                  file=sys.stderr)
            sys.exit(1)
        cols, rows = int(m.group(1)), int(m.group(2))
        grid_chunks(sanitized, str(dst), cols, rows)


if __name__ == "__main__":
    main()
