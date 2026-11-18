#!/usr/bin/env python3
"""
Oracle Comparison Tool for odb2kicad Validation

Compares structured data from ODB++ oracle (either Valor Viewer or direct parse)
against odb2kicad converter output to detect parser bugs.

Usage:
    python compare_oracle.py <odb_path> <kicad_pcb_path> [--tolerance 0.001]
"""

import sys
import json
import re
from pathlib import Path
from dataclasses import dataclass
from typing import Dict, List, Any, Optional, Tuple
import math
from valor_oracle import DirectOdbOracle, BBox


@dataclass
class ComparisonResult:
    passed: bool
    category: str
    message: str
    expected: Any = None
    actual: Any = None
    tolerance_used: float = 0.0


class KiCadPcbParser:
    """Simple parser for .kicad_pcb files to extract comparable data"""

    def __init__(self, filepath: str):
        self.filepath = Path(filepath)
        self.content = ""
        self.data: Dict[str, Any] = {}

    def parse(self) -> Dict[str, Any]:
        """Parse the KiCad PCB file and extract metrics"""
        with open(self.filepath, 'r', encoding='latin-1') as f:
            self.content = f.read()

        self.data = {
            'footprints': self._parse_footprints(),
            'segments': self._parse_segments(),
            'vias': self._parse_vias(),
            'zones': self._parse_zones(),
            'gr_lines': self._parse_gr_lines(),
            'gr_arcs': self._parse_gr_arcs(),
            'layers': self._parse_layers(),
            'nets': self._parse_nets(),
            'general': self._parse_general(),
            'edge_cuts_bbox': self._parse_edge_cuts_bbox()
        }

        return self.data

    def _parse_footprints(self) -> List[Dict]:
        """Extract footprint (component) data"""
        footprints = []

        # Match footprint blocks - need to handle multi-line
        # (footprint "name" (layer "X") ... (at x y [angle]) ...)
        # The pattern needs to span across newlines
        pattern = r'\(footprint\s+"([^"]+)"[^\(]*(?:\([^)]+\)\s*)*\(at\s+([-\d.]+)\s+([-\d.]+)(?:\s+([-\d.]+))?\)'

        for match in re.finditer(pattern, self.content, re.DOTALL):
            fp = {
                'package': match.group(1),
                'x': float(match.group(2)),
                'y': float(match.group(3)),
                'rotation': float(match.group(4)) if match.group(4) else 0.0
            }

            # Try to find reference designator
            # Look for (property "Reference" "XX") within the footprint block
            start = match.start()
            # Find the matching closing paren (simplified - look for next footprint or end)
            end = self.content.find('(footprint', start + 1)
            if end == -1:
                end = len(self.content)
            block = self.content[start:end]

            ref_match = re.search(r'\(property\s+"Reference"\s+"([^"]+)"', block)
            if ref_match:
                fp['refdes'] = ref_match.group(1)
            else:
                # Try fp_text reference format
                ref_match = re.search(r'\(fp_text\s+reference\s+"([^"]+)"', block)
                if ref_match:
                    fp['refdes'] = ref_match.group(1)

            # Extract pads with their positions and net assignments
            pads = []
            for pad_match in re.finditer(r'\(pad\s+"([^"]+)"[^)]*\)', block):
                pad_start = pad_match.start()
                # Find the full pad block - look for closing paren at same nesting level
                pad_end = block.find('(pad ', pad_start + 1)
                if pad_end == -1:
                    pad_end = len(block)
                pad_block = block[pad_start:pad_end]

                pad = {'pin': pad_match.group(1)}

                # Extract pad position (at x y [angle])
                pad_at_match = re.search(r'\(at\s+([-\d.]+)\s+([-\d.]+)', pad_block)
                if pad_at_match:
                    pad['x'] = float(pad_at_match.group(1))
                    pad['y'] = float(pad_at_match.group(2))
                else:
                    pad['x'] = 0.0
                    pad['y'] = 0.0

                # Extract net from pad
                pad_net_match = re.search(r'\(net\s+(\d+)\s+"([^"]*)"\)', pad_block)
                if pad_net_match:
                    pad['net'] = int(pad_net_match.group(1))
                    pad['net_name'] = pad_net_match.group(2)
                else:
                    pad['net'] = 0
                    pad['net_name'] = ''
                pads.append(pad)
            fp['pads'] = pads

            footprints.append(fp)

        # If regex didn't work, try simpler line-by-line approach
        if not footprints:
            # Find all footprint blocks by looking for (footprint and (at patterns
            fp_starts = [m.start() for m in re.finditer(r'\(footprint\s+"', self.content)]
            for start in fp_starts:
                end = self.content.find('(footprint', start + 1)
                if end == -1:
                    end = len(self.content)
                block = self.content[start:end]

                # Extract package name
                pkg_match = re.search(r'\(footprint\s+"([^"]+)"', block)
                if not pkg_match:
                    continue

                # Extract position
                at_match = re.search(r'\(at\s+([-\d.]+)\s+([-\d.]+)(?:\s+([-\d.]+))?\)', block)
                if not at_match:
                    continue

                fp = {
                    'package': pkg_match.group(1),
                    'x': float(at_match.group(1)),
                    'y': float(at_match.group(2)),
                    'rotation': float(at_match.group(3)) if at_match.group(3) else 0.0
                }

                # Extract reference
                ref_match = re.search(r'\(property\s+"Reference"\s+"([^"]+)"', block)
                if ref_match:
                    fp['refdes'] = ref_match.group(1)
                else:
                    ref_match = re.search(r'\(fp_text\s+reference\s+"([^"]+)"', block)
                    if ref_match:
                        fp['refdes'] = ref_match.group(1)

                # Extract pads with their positions and net assignments
                pads = []
                for pad_match in re.finditer(r'\(pad\s+"([^"]+)"[^)]*\)', block):
                    pad_start = pad_match.start()
                    # Find the full pad block - look for closing paren at same nesting level
                    pad_end = block.find('(pad ', pad_start + 1)
                    if pad_end == -1:
                        pad_end = len(block)
                    pad_block = block[pad_start:pad_end]

                    pad = {'pin': pad_match.group(1)}

                    # Extract pad position (at x y [angle])
                    pad_at_match = re.search(r'\(at\s+([-\d.]+)\s+([-\d.]+)', pad_block)
                    if pad_at_match:
                        pad['x'] = float(pad_at_match.group(1))
                        pad['y'] = float(pad_at_match.group(2))
                    else:
                        pad['x'] = 0.0
                        pad['y'] = 0.0

                    # Extract net from pad
                    pad_net_match = re.search(r'\(net\s+(\d+)\s+"([^"]*)"\)', pad_block)
                    if pad_net_match:
                        pad['net'] = int(pad_net_match.group(1))
                        pad['net_name'] = pad_net_match.group(2)
                    else:
                        pad['net'] = 0
                        pad['net_name'] = ''
                    pads.append(pad)
                fp['pads'] = pads

                footprints.append(fp)

        return footprints

    def _parse_segments(self) -> List[Dict]:
        """Extract track/trace segments with net assignments"""
        segments = []

        # (segment (start x y) (end x y) (width w) (layer "layer") (net n))
        pattern = r'\(segment\s+\(start\s+([-\d.]+)\s+([-\d.]+)\)\s+\(end\s+([-\d.]+)\s+([-\d.]+)\)\s+\(width\s+([-\d.]+)\)\s+\(layer\s+"([^"]+)"\)\s+\(net\s+(\d+)\)'

        for match in re.finditer(pattern, self.content):
            segments.append({
                'start': (float(match.group(1)), float(match.group(2))),
                'end': (float(match.group(3)), float(match.group(4))),
                'width': float(match.group(5)),
                'layer': match.group(6),
                'net': int(match.group(7))
            })

        return segments

    def _parse_vias(self) -> List[Dict]:
        """Extract vias with net assignments"""
        vias = []

        # (via (at x y) (size s) (drill d) (layers "l1" "l2") (net n))
        # Need to match the full via structure to get net
        pattern = r'\(via\s+\(at\s+([-\d.]+)\s+([-\d.]+)\)\s+\(size\s+([-\d.]+)\)\s+\(drill\s+([-\d.]+)\)\s+\(layers\s+"[^"]+"\s+"[^"]+"\)\s+\(net\s+(\d+)\)'

        for match in re.finditer(pattern, self.content):
            vias.append({
                'x': float(match.group(1)),
                'y': float(match.group(2)),
                'size': float(match.group(3)),
                'drill': float(match.group(4)),
                'net': int(match.group(5))
            })

        return vias

    def _parse_zones(self) -> List[Dict]:
        """Extract copper zones with net assignments"""
        zones = []

        # Find zone blocks and extract net info
        # Zone format: (zone (net N) (net_name "name") (layer "layer") ...)
        zone_starts = [m.start() for m in re.finditer(r'\(zone\s+\(net\s+', self.content)]

        for start in zone_starts:
            # Find end of this zone (next zone or end of zones section)
            end = self.content.find('(zone ', start + 1)
            if end == -1:
                end = len(self.content)
            block = self.content[start:end]

            zone = {}

            # Extract net ID
            net_match = re.search(r'\(net\s+(\d+)\)', block)
            if net_match:
                zone['net'] = int(net_match.group(1))
            else:
                zone['net'] = 0

            # Extract net name
            net_name_match = re.search(r'\(net_name\s+"([^"]*)"\)', block)
            if net_name_match:
                zone['net_name'] = net_name_match.group(1)

            # Extract layer
            layer_match = re.search(r'\(layer\s+"([^"]+)"\)', block)
            if layer_match:
                zone['layer'] = layer_match.group(1)

            zones.append(zone)

        return zones

    def _parse_gr_lines(self) -> List[Dict]:
        """Extract graphic lines"""
        lines = []

        # (gr_line (start x y) (end x y) (layer "layer") ...)
        pattern = r'\(gr_line\s+\(start\s+([-\d.]+)\s+([-\d.]+)\)\s+\(end\s+([-\d.]+)\s+([-\d.]+)\)\s+[^)]*\(layer\s+"([^"]+)"\)'

        for match in re.finditer(pattern, self.content):
            lines.append({
                'start': (float(match.group(1)), float(match.group(2))),
                'end': (float(match.group(3)), float(match.group(4))),
                'layer': match.group(5)
            })

        return lines

    def _parse_gr_arcs(self) -> List[Dict]:
        """Extract graphic arcs"""
        arcs = []

        # (gr_arc (start x y) (mid x y) (end x y) (layer "layer") ...)
        pattern = r'\(gr_arc\s+\(start\s+([-\d.]+)\s+([-\d.]+)\)'

        for match in re.finditer(pattern, self.content):
            arcs.append({
                'start': (float(match.group(1)), float(match.group(2)))
            })

        return arcs

    def _parse_edge_cuts_bbox(self) -> Optional[Dict[str, float]]:
        """The extent of everything on Edge.Cuts (board gr_* items and footprints' fp_* items, the
        latter placed by their footprint's position and angle), KiCad frame (Y down), mm.
        Arcs count with their bulge, circles with their radius. None when Edge.Cuts is empty."""
        box = BBox()
        text = self.content
        for m in re.finditer(r'\((gr_line|gr_arc|gr_circle|gr_rect|gr_poly)\b', text):
            item = _sexpr_block(text, m.start())
            if '(layer "Edge.Cuts")' in item:
                _add_edge_item(box, m.group(1)[3:], item, lambda x, y: (x, y))
        for m in re.finditer(r'\(footprint\s+"', text):
            fp = _sexpr_block(text, m.start())
            if '"Edge.Cuts"' not in fp:
                continue
            at = re.search(r'\(at\s+([-\d.]+)\s+([-\d.]+)(?:\s+([-\d.]+))?\)', fp)
            fx, fy = float(at.group(1)), float(at.group(2))
            ang = float(at.group(3)) if at.group(3) else 0.0
            place = lambda x, y, fx=fx, fy=fy, ang=ang: _footprint_to_board(fx, fy, ang, x, y)
            for g in re.finditer(r'\((fp_line|fp_arc|fp_circle|fp_rect|fp_poly)\b', fp):
                item = _sexpr_block(fp, g.start())
                if '(layer "Edge.Cuts")' in item:
                    _add_edge_item(box, g.group(1)[3:], item, place)
        return box.scaled(1.0)

    def _parse_layers(self) -> List[str]:
        """Extract layer names used"""
        layers = set()

        # Find all layer references
        for match in re.finditer(r'\(layer\s+"([^"]+)"\)', self.content):
            layers.add(match.group(1))

        return sorted(list(layers))

    def _parse_nets(self) -> List[Dict]:
        """Extract net definitions"""
        nets = []

        # (net N "name")
        pattern = r'\(net\s+(\d+)\s+"([^"]*)"\)'

        for match in re.finditer(pattern, self.content):
            nets.append({
                'number': int(match.group(1)),
                'name': match.group(2)
            })

        return nets

    def _parse_general(self) -> Dict:
        """Extract general board info"""
        info = {
            'thickness': None
        }

        # (general (thickness x))
        match = re.search(r'\(general\s+\(thickness\s+([-\d.]+)\)', self.content)
        if match:
            info['thickness'] = float(match.group(1))

        return info


def _converted_step(oracle: Dict):
    """(name, data) of the step the converter writes first: the matrix's first STEP with a folder,
    else the first folder by name."""
    steps = oracle.get('steps', {})
    if not steps:
        return None, None
    by_lower = {n.lower(): n for n in steps}
    for name in oracle.get('step_order', []):
        if name.lower() in by_lower:
            n = by_lower[name.lower()]
            return n, steps[n]
    n = sorted(steps)[0]
    return n, steps[n]


def _sexpr_block(text: str, start: int) -> str:
    """The balanced s-expression that opens at text[start] ('('), quoted strings respected."""
    depth, i, n, quoted = 0, start, len(text), False
    while i < n:
        c = text[i]
        if quoted:
            if c == '\\':
                i += 1
            elif c == '"':
                quoted = False
        elif c == '"':
            quoted = True
        elif c == '(':
            depth += 1
        elif c == ')':
            depth -= 1
            if depth == 0:
                return text[start:i + 1]
        i += 1
    return text[start:]


def _footprint_to_board(fx: float, fy: float, angle: float, lx: float, ly: float) -> Tuple[float, float]:
    """A footprint-local point (KiCad frame, Y down) on the board: turned by the footprint's angle
    (counter-clockwise on screen, degrees), then moved to its position. Local pad / item positions
    of a back-side footprint are stored already flipped, so no mirror is needed here."""
    a = math.radians(angle)
    c, s = math.cos(a), math.sin(a)
    return fx + lx * c + ly * s, fy - lx * s + ly * c


def _xy(item: str, key: str) -> Optional[Tuple[float, float]]:
    m = re.search(r'\(' + key + r'\s+([-\d.eE+]+)\s+([-\d.eE+]+)\)', item)
    return (float(m.group(1)), float(m.group(2))) if m else None


def _add_arc3(box: BBox, s: Tuple[float, float], m: Tuple[float, float], e: Tuple[float, float]):
    """A KiCad arc by start / mid / end."""
    ax, ay = s
    bx, by = m
    cx, cy = e
    d = 2 * (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by))
    if abs(d) < 1e-12:  # collinear: a straight segment
        for p in (s, m, e):
            box.add(*p)
        return
    ux = ((ax * ax + ay * ay) * (by - cy) + (bx * bx + by * by) * (cy - ay) + (cx * cx + cy * cy) * (ay - by)) / d
    uy = ((ax * ax + ay * ay) * (cx - bx) + (bx * bx + by * by) * (ax - cx) + (cx * cx + cy * cy) * (bx - ax)) / d
    cross = (bx - ax) * (cy - by) - (by - ay) * (cx - bx)
    box.add_arc(s, e, (ux, uy), cross < 0)  # cw in the angle-increasing sense of atan2


def _add_edge_item(box: BBox, kind: str, item: str, place):
    """One Edge.Cuts item (line, arc, circle, rect, poly) into box, its points through place()."""
    P = lambda p: place(*p)
    if kind == 'line':
        for k in ('start', 'end'):
            p = _xy(item, k)
            if p:
                box.add(*P(p))
    elif kind == 'rect':
        s, e = _xy(item, 'start'), _xy(item, 'end')
        if s and e:
            for p in ((s[0], s[1]), (e[0], s[1]), (e[0], e[1]), (s[0], e[1])):
                box.add(*P(p))
    elif kind == 'circle':
        c, e = _xy(item, 'center'), _xy(item, 'end')
        if c and e:
            cx, cy = P(c)
            box.add_circle(cx, cy, math.hypot(e[0] - c[0], e[1] - c[1]))
    elif kind == 'arc':
        s, m, e = _xy(item, 'start'), _xy(item, 'mid'), _xy(item, 'end')
        if s and m and e:
            _add_arc3(box, P(s), P(m), P(e))
    elif kind == 'poly':
        for a in re.finditer(r'\(arc\s', item):
            arc = _sexpr_block(item, a.start())
            s, m, e = _xy(arc, 'start'), _xy(arc, 'mid'), _xy(arc, 'end')
            if s and m and e:
                _add_arc3(box, P(s), P(m), P(e))
        for x, y in re.findall(r'\(xy\s+([-\d.eE+]+)\s+([-\d.eE+]+)\)', item):
            box.add(*P((float(x), float(y))))


class OracleComparator:
    """Compare oracle data to KiCad converter output"""

    def __init__(self, tolerance: float = 0.001):
        self.tolerance = tolerance  # mm
        self.results: List[ComparisonResult] = []

    def compare(self, oracle_data: Dict, kicad_data: Dict) -> List[ComparisonResult]:
        """Run all comparisons and return results"""
        self.results = []

        # Compare component/footprint counts
        self._compare_component_counts(oracle_data, kicad_data)

        # Compare component positions
        self._compare_component_positions(oracle_data, kicad_data)

        # Compare feature counts by layer
        self._compare_feature_counts(oracle_data, kicad_data)

        # Compare bounding boxes
        self._compare_bounding_boxes(oracle_data, kicad_data)

        # Compare trace counts
        self._compare_trace_counts(oracle_data, kicad_data)

        # Check net assignments (diagnostic)
        self._compare_net_assignments(kicad_data)

        # Compare pin 1 positions (rotation detection)
        self._compare_pin_positions(oracle_data, kicad_data)

        return self.results

    def _compare_component_counts(self, oracle: Dict, kicad: Dict):
        """Compare total component counts"""
        # Count components from oracle (all component layers)
        oracle_count = 0
        for step_name, step_data in oracle.get('steps', {}).items():
            for layer_name, layer_data in step_data.get('layers', {}).items():
                oracle_count += layer_data.get('component_count', 0)

        # board-only footprints the converter makes for copper that is no component's (free pads,
        # outline holes: library "odb2kicad:...") are not components
        kicad_count = sum(1 for fp in kicad.get('footprints', [])
                          if not str(fp.get('package', fp.get('name', ''))).startswith('odb2kicad:'))

        passed = oracle_count == kicad_count
        self.results.append(ComparisonResult(
            passed=passed,
            category='component_count',
            message=f"Component count: oracle={oracle_count}, kicad={kicad_count}",
            expected=oracle_count,
            actual=kicad_count
        ))

    def _compare_component_positions(self, oracle: Dict, kicad: Dict):
        """Compare individual component positions and rotations"""
        # Build a map of KiCad components by refdes
        kicad_comps = {}
        for fp in kicad.get('footprints', []):
            if 'refdes' in fp:
                kicad_comps[fp['refdes']] = fp

        # Build a map of oracle components by refdes
        oracle_comps = {}
        for step_name, step_data in oracle.get('steps', {}).items():
            for layer_name, layer_data in step_data.get('layers', {}).items():
                for comp in layer_data.get('components', []):
                    oracle_comps[comp['refdes']] = comp

        if not oracle_comps:
            return  # No component data available

        # Compare each component
        position_errors = []
        rotation_errors = []
        missing_in_kicad = []
        tolerance_mm = 0.5  # Position tolerance in mm
        rotation_tolerance = 1.0  # Rotation tolerance in degrees

        for refdes, oracle_comp in oracle_comps.items():
            if refdes not in kicad_comps:
                missing_in_kicad.append(refdes)
                continue

            kicad_comp = kicad_comps[refdes]

            # Compare position (KiCad Y is inverted from ODB++)
            ox, oy = oracle_comp['x'], oracle_comp['y']
            kx, ky = kicad_comp['x'], kicad_comp['y']
            # Note: KiCad Y might be negated - check the actual transformation
            dx = abs(ox - kx)
            dy = abs(oy - (-ky))  # KiCad Y is typically negated

            if dx > tolerance_mm or dy > tolerance_mm:
                position_errors.append({
                    'refdes': refdes,
                    'oracle_pos': (ox, oy),
                    'kicad_pos': (kx, ky),
                    'delta': (dx, dy)
                })

            # Compare rotation: the CMP rotation is clockwise, KiCad's footprint angle
            # counter-clockwise, so the two add up to a full turn (docs/rotation.md)
            orot = oracle_comp.get('rotation', 0.0)
            krot = kicad_comp.get('rotation', 0.0)
            rot_diff = (krot + orot) % 360
            rot_diff = min(rot_diff, 360 - rot_diff)

            if rot_diff > rotation_tolerance:
                rotation_errors.append({
                    'refdes': refdes,
                    'oracle_rotation': orot,
                    'kicad_rotation': krot,
                    'difference': rot_diff
                })

        # Report position errors
        if position_errors:
            sample = position_errors[:5]  # Show first 5
            sample_str = ', '.join([f"{e['refdes']}: delta=({e['delta'][0]:.2f}, {e['delta'][1]:.2f})mm"
                                    for e in sample])
            if len(position_errors) > 5:
                sample_str += f", ... and {len(position_errors) - 5} more"
            self.results.append(ComparisonResult(
                passed=False,
                category='component_positions',
                message=f"Position mismatch for {len(position_errors)} components: {sample_str}",
                expected=f"{len(oracle_comps)} components positioned correctly",
                actual=f"{len(position_errors)} have position errors"
            ))
        else:
            self.results.append(ComparisonResult(
                passed=True,
                category='component_positions',
                message=f"All {len(oracle_comps)} component positions match within {tolerance_mm}mm",
                expected=len(oracle_comps),
                actual=len(oracle_comps)
            ))

        # Report rotation errors
        if rotation_errors:
            sample = rotation_errors[:5]  # Show first 5
            sample_str = ', '.join([f"{e['refdes']}: oracle={e['oracle_rotation']:.1f} vs kicad={e['kicad_rotation']:.1f}"
                                    for e in sample])
            if len(rotation_errors) > 5:
                sample_str += f", ... and {len(rotation_errors) - 5} more"
            self.results.append(ComparisonResult(
                passed=False,
                category='component_rotations',
                message=f"Rotation mismatch for {len(rotation_errors)} components: {sample_str}",
                expected=f"{len(oracle_comps)} components rotated correctly",
                actual=f"{len(rotation_errors)} have rotation errors"
            ))
        else:
            self.results.append(ComparisonResult(
                passed=True,
                category='component_rotations',
                message=f"All {len(oracle_comps)} component rotations match within {rotation_tolerance} deg",
                expected=len(oracle_comps),
                actual=len(oracle_comps)
            ))

    def _compare_feature_counts(self, oracle: Dict, kicad: Dict):
        """Compare feature counts by type"""
        # Oracle feature counts (all layers)
        oracle_lines = 0
        oracle_pads = 0
        oracle_arcs = 0

        for step_name, step_data in oracle.get('steps', {}).items():
            for layer_name, layer_data in step_data.get('layers', {}).items():
                oracle_lines += layer_data.get('line_count', 0)
                oracle_pads += layer_data.get('pad_count', 0)
                oracle_arcs += layer_data.get('arc_count', 0)

        # KiCad feature counts
        kicad_lines = len(kicad.get('gr_lines', [])) + len(kicad.get('segments', []))
        kicad_arcs = len(kicad.get('gr_arcs', []))

        # Pads are part of footprints in KiCad - harder to count directly
        # For now, compare lines and arcs

        self.results.append(ComparisonResult(
            passed=True,  # Just informational for now
            category='feature_counts',
            message=f"Oracle: {oracle_lines} lines, {oracle_arcs} arcs, {oracle_pads} pads | "
                   f"KiCad: {kicad_lines} lines/segments, {kicad_arcs} arcs",
            expected={'lines': oracle_lines, 'arcs': oracle_arcs, 'pads': oracle_pads},
            actual={'lines': kicad_lines, 'arcs': kicad_arcs}
        ))

    def _compare_bounding_boxes(self, oracle: Dict, kicad: Dict):
        """The board outline: the profile's extent (the converted step's, stephdr origin applied)
        against the extent of Edge.Cuts, edge by edge (KiCad Y is ODB++ Y negated)."""
        step_name, step = _converted_step(oracle)
        prof = ((step or {}).get('profile') or {}).get('bbox')
        edge = kicad.get('edge_cuts_bbox')
        tol = max(self.tolerance, 0.01)
        if not prof:
            self.results.append(ComparisonResult(
                passed=True,  # nothing to compare against: informational
                category='bounding_box',
                message=f"Step {step_name or '?'} has no profile; Edge.Cuts "
                        + ("is empty" if edge is None else
                           f"spans {edge['xmax'] - edge['xmin']:.3f}x{edge['ymax'] - edge['ymin']:.3f}mm")))
            return
        if edge is None:
            self.results.append(ComparisonResult(
                passed=False, category='bounding_box',
                message=f"Profile of step {step_name} spans {prof['xmax'] - prof['xmin']:.3f}x"
                        f"{prof['ymax'] - prof['ymin']:.3f}mm but Edge.Cuts is empty",
                expected=prof, actual=None, tolerance_used=tol))
            return
        # Edge.Cuts in the ODB++ frame (Y up)
        k = {'xmin': edge['xmin'], 'xmax': edge['xmax'], 'ymin': -edge['ymax'], 'ymax': -edge['ymin']}
        diffs = {side: k[side] - prof[side] for side in ('xmin', 'ymin', 'xmax', 'ymax')}
        worst = max(abs(v) for v in diffs.values())
        passed = worst <= tol
        w_o, h_o = prof['xmax'] - prof['xmin'], prof['ymax'] - prof['ymin']
        w_k, h_k = k['xmax'] - k['xmin'], k['ymax'] - k['ymin']
        msg = (f"Profile vs Edge.Cuts: profile={w_o:.3f}x{h_o:.3f}mm at ({prof['xmin']:.3f}, {prof['ymin']:.3f}), "
               f"edge={w_k:.3f}x{h_k:.3f}mm at ({k['xmin']:.3f}, {k['ymin']:.3f}), worst edge off by {worst:.4f}mm")
        if not passed:
            msg += " (" + ", ".join(f"{side} {v:+.4f}" for side, v in diffs.items() if abs(v) > tol) + ")"
        self.results.append(ComparisonResult(
            passed=passed, category='bounding_box', message=msg,
            expected={'width': w_o, 'height': h_o, **prof},
            actual={'width': w_k, 'height': h_k, **k},
            tolerance_used=tol))

    def _compare_trace_counts(self, oracle: Dict, kicad: Dict):
        """Compare trace/segment counts"""
        kicad_segments = len(kicad.get('segments', []))

        # Oracle line count from copper layers
        oracle_lines = 0
        for step_name, step_data in oracle.get('steps', {}).items():
            for layer_name, layer_data in step_data.get('layers', {}).items():
                # Only count copper layers
                if 'cu' in layer_name.lower():
                    oracle_lines += layer_data.get('line_count', 0)

        self.results.append(ComparisonResult(
            passed=True,  # Informational
            category='trace_count',
            message=f"Copper traces: oracle={oracle_lines}, kicad={kicad_segments}",
            expected=oracle_lines,
            actual=kicad_segments
        ))

    def _compare_net_assignments(self, kicad: Dict):
        """Diagnostic: count elements without net assignments (net=0 or missing)"""
        unassigned = {
            'segments': 0,
            'vias': 0,
            'zones': 0,
            'pads': 0
        }
        totals = {
            'segments': 0,
            'vias': 0,
            'zones': 0,
            'pads': 0
        }

        # Check segments
        for seg in kicad.get('segments', []):
            totals['segments'] += 1
            if seg.get('net', 0) == 0:
                unassigned['segments'] += 1

        # Check vias
        for via in kicad.get('vias', []):
            totals['vias'] += 1
            if via.get('net', 0) == 0:
                unassigned['vias'] += 1

        # Check zones
        for zone in kicad.get('zones', []):
            totals['zones'] += 1
            if zone.get('net', 0) == 0:
                unassigned['zones'] += 1

        # Check pads within footprints
        for fp in kicad.get('footprints', []):
            for pad in fp.get('pads', []):
                totals['pads'] += 1
                if pad.get('net', 0) == 0:
                    unassigned['pads'] += 1

        # Calculate totals
        total_unassigned = sum(unassigned.values())
        total_elements = sum(totals.values())

        # Build detailed message
        details = []
        for elem_type in ['segments', 'vias', 'zones', 'pads']:
            if totals[elem_type] > 0:
                details.append(f"{elem_type}: {unassigned[elem_type]}/{totals[elem_type]}")

        detail_str = ', '.join(details)

        # This is informational but useful as a canary
        # A high ratio of unassigned nets might indicate parsing issues
        passed = True  # Informational - always passes but shows data

        self.results.append(ComparisonResult(
            passed=passed,
            category='net_assignments',
            message=f"Unassigned nets: {total_unassigned}/{total_elements} total ({detail_str})",
            expected={'total': total_elements, 'by_type': totals},
            actual={'unassigned': total_unassigned, 'by_type': unassigned}
        ))

    def _compare_pin_positions(self, oracle: Dict, kicad: Dict):
        """Pin 1 of every component where it lands on the board: the KiCad pad's local position
        turned by the footprint's angle and moved to the footprint's position, against the ODB++
        TOP record of pin 1 (board coordinates). A pin 1 that lands elsewhere means the footprint
        is turned, mirrored or placed wrongly; the angle between the two, seen from the part's
        centre, says by how much.
        """
        oracle_comps = {}
        for step_name, step_data in oracle.get('steps', {}).items():
            for layer_name, layer_data in step_data.get('layers', {}).items():
                for comp in layer_data.get('components', []):
                    if comp.get('terminals'):
                        oracle_comps[comp['refdes']] = comp
        if not oracle_comps:
            return  # No terminal data available

        kicad_fps = {fp['refdes']: fp for fp in kicad.get('footprints', []) if 'refdes' in fp and fp.get('pads')}
        tol = max(self.tolerance, 0.01)
        compared = 0
        misplaced = []
        for refdes, comp in oracle_comps.items():
            fp = kicad_fps.get(refdes)
            if not fp:
                continue
            # pin 1: the terminal named "1", else the part's first TOP record (some jobs name
            # pins <refdes>-<n>); the KiCad pad of the same name
            o1 = next((t for t in comp['terminals'] if t.get('pin_num') == '1'),
                      min(comp['terminals'], key=lambda t: t.get('term_idx', 0)))
            k1 = next((p for p in fp['pads'] if p.get('pin') == o1.get('pin_num')), None)
            if not o1 or not k1:
                continue
            compared += 1
            # ODB++ pin 1 in the KiCad frame
            ox, oy = o1['x'], -o1['y']
            kx, ky = _footprint_to_board(fp['x'], fp['y'], fp.get('rotation', 0.0), k1['x'], k1['y'])
            off = math.hypot(kx - ox, ky - oy)
            if off <= tol:
                continue
            cx, cy = fp['x'], fp['y']
            turn = None
            if math.hypot(ox - cx, oy - cy) > tol and math.hypot(kx - cx, ky - cy) > tol:
                turn = (math.degrees(math.atan2(-(ky - cy), kx - cx) - math.atan2(-(oy - cy), ox - cx))) % 360
            misplaced.append({'refdes': refdes, 'off': off, 'turn': turn,
                              'oracle': (ox, oy), 'kicad': (kx, ky)})

        if misplaced:
            def what(e):
                t = f", turned {e['turn']:.0f} deg" if e['turn'] is not None and min(e['turn'], 360 - e['turn']) > 1 else ""
                return f"{e['refdes']} off by {e['off']:.3f}mm{t}"
            sample = '; '.join(what(e) for e in misplaced[:5])
            if len(misplaced) > 5:
                sample += f"; ... and {len(misplaced) - 5} more"
            self.results.append(ComparisonResult(
                passed=False, category='pin1_positions',
                message=f"Pin 1 lands elsewhere for {len(misplaced)} of {compared} components compared ({len(oracle_comps)} with terminals): {sample}",
                expected=f"{compared} components with pin 1 within {tol}mm",
                actual=f"{len(misplaced)} misplaced", tolerance_used=tol))
        else:
            self.results.append(ComparisonResult(
                passed=True, category='pin1_positions',
                message=f"Pin 1 on the board matches within {tol}mm for all {compared} of {len(oracle_comps)} components with terminals",
                expected=compared, actual=compared, tolerance_used=tol))


def redact_message(message: str) -> str:
    """Redact sensitive information from a message."""
    import re

    # Replace component references (R1, C2, U15, etc.)
    message = re.sub(r'\b([RCUQJDLKMT]\d+)\b', 'CMP_XXX', message)

    # Replace net names that look like identifiers
    message = re.sub(r'\bnet[_\s]*["\']?[\w]+["\']?', 'NET_XXX', message, flags=re.IGNORECASE)

    # Replace specific coordinate values with X.XXX
    message = re.sub(r'[-]?\d+\.\d{2,}', 'X.XXX', message)

    # Replace integer coordinates
    message = re.sub(r'(?<=[\s(,=])\d{2,}(?=[\s),mm])', 'XXX', message)

    return message


def redact_value(value: Any) -> Any:
    """Redact sensitive information from a value."""
    if value is None:
        return None
    if isinstance(value, (int, float)):
        return "REDACTED"
    if isinstance(value, str):
        return redact_message(value)
    if isinstance(value, dict):
        return {k: "REDACTED" for k in value.keys()}
    if isinstance(value, list):
        return f"[{len(value)} items]"
    return "REDACTED"


def print_results(results: List[ComparisonResult], verbose: bool = False,
                  redacted: bool = False):
    """Print comparison results in a readable format"""
    passed = sum(1 for r in results if r.passed)
    failed = sum(1 for r in results if not r.passed)
    info = sum(1 for r in results if r.expected is None and r.actual is None)

    print("\n" + "=" * 60)
    print("ORACLE COMPARISON RESULTS" + (" [REDACTED]" if redacted else ""))
    print("=" * 60)

    for result in results:
        if result.passed:
            status = "[PASS]"
        else:
            status = "[FAIL]"

        print(f"\n{status} {result.category}")

        if redacted:
            # Show only category and pass/fail, with redacted message
            print(f"  {redact_message(result.message)}")
        else:
            print(f"  {result.message}")

        if verbose and not result.passed:
            if redacted:
                print(f"  Expected: {redact_value(result.expected)}")
                print(f"  Actual:   {redact_value(result.actual)}")
            else:
                print(f"  Expected: {result.expected}")
                print(f"  Actual:   {result.actual}")
            if result.tolerance_used:
                print(f"  Tolerance: {result.tolerance_used}")

    print("\n" + "-" * 60)
    print(f"Summary: {passed} passed, {failed} failed")
    print("=" * 60)

    return failed == 0


def main():
    import argparse

    parser = argparse.ArgumentParser(description='Compare ODB++ oracle to odb2kicad output')
    parser.add_argument('odb_path', help='Path to ODB++ directory')
    parser.add_argument('kicad_path', help='Path to converted .kicad_pcb file')
    parser.add_argument('--tolerance', type=float, default=0.01,
                       help='Position tolerance in mm (default: 0.01)')
    parser.add_argument('--verbose', '-v', action='store_true',
                       help='Verbose output')
    parser.add_argument('--json', action='store_true',
                       help='Output results as JSON')
    parser.add_argument('--redacted', '-r', action='store_true',
                       help='Redact sensitive design data (coordinates, names) for sharing')

    args = parser.parse_args()

    # Parse oracle data from ODB++
    if not args.redacted:
        print(f"Parsing ODB++ oracle: {args.odb_path}")
    else:
        print("Parsing ODB++ oracle: [REDACTED]")
    oracle = DirectOdbOracle(args.odb_path)
    oracle_data = oracle.parse()

    # Parse KiCad output
    if not args.redacted:
        print(f"Parsing KiCad output: {args.kicad_path}")
    else:
        print("Parsing KiCad output: [REDACTED]")
    kicad_parser = KiCadPcbParser(args.kicad_path)
    kicad_data = kicad_parser.parse()

    # Compare
    print("Comparing...")
    comparator = OracleComparator(tolerance=args.tolerance)
    results = comparator.compare(oracle_data, kicad_data)

    if args.json:
        if args.redacted:
            # Redacted JSON output - only counts and pass/fail
            output = {
                'summary': {
                    'oracle_component_count': sum(
                        layer.get('component_count', 0)
                        for step in oracle_data.get('steps', {}).values()
                        for layer in step.get('layers', {}).values()
                    ),
                    'kicad_footprint_count': len(kicad_data['footprints']),
                    'kicad_segment_count': len(kicad_data['segments']),
                    'kicad_layer_count': len(kicad_data['layers']),
                },
                'results': [
                    {
                        'passed': r.passed,
                        'category': r.category,
                        'error_type': 'mismatch' if not r.passed else None
                    }
                    for r in results
                ]
            }
        else:
            # Calculate unassigned nets for JSON output
            unassigned_segments = sum(1 for s in kicad_data['segments'] if s.get('net', 0) == 0)
            unassigned_vias = sum(1 for v in kicad_data['vias'] if v.get('net', 0) == 0)
            unassigned_zones = sum(1 for z in kicad_data['zones'] if z.get('net', 0) == 0)
            unassigned_pads = sum(1 for fp in kicad_data['footprints']
                                  for p in fp.get('pads', []) if p.get('net', 0) == 0)
            total_pads = sum(len(fp.get('pads', [])) for fp in kicad_data['footprints'])

            output = {
                'oracle': oracle_data,
                'kicad': {
                    'footprint_count': len(kicad_data['footprints']),
                    'segment_count': len(kicad_data['segments']),
                    'via_count': len(kicad_data['vias']),
                    'zone_count': len(kicad_data['zones']),
                    'pad_count': total_pads,
                    'layers': kicad_data['layers'],
                    'net_count': len(kicad_data['nets']),
                    'unassigned_nets': {
                        'segments': unassigned_segments,
                        'vias': unassigned_vias,
                        'zones': unassigned_zones,
                        'pads': unassigned_pads,
                        'total': unassigned_segments + unassigned_vias + unassigned_zones + unassigned_pads
                    }
                },
                'results': [
                    {
                        'passed': r.passed,
                        'category': r.category,
                        'message': r.message,
                        'expected': r.expected,
                        'actual': r.actual
                    }
                    for r in results
                ]
            }
        print(json.dumps(output, indent=2, default=str))
    else:
        all_passed = print_results(results, verbose=args.verbose, redacted=args.redacted)
        sys.exit(0 if all_passed else 1)


if __name__ == '__main__':
    main()
