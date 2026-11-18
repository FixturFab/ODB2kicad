#!/usr/bin/env python3
"""
Valor ODB++ Viewer Oracle Client

Connects to the Siemens ODB++ Viewer via TCP socket to extract
structured data for validating odb2kicad conversions.

The viewer must be running with server.pl active on port 56753.

Usage:
    python valor_oracle.py <odb_path> [--host localhost] [--port 56753]
"""

import io
import socket
import os
import sys
import json
import tempfile
import re
from pathlib import Path
from typing import Dict, List, Any, Optional
from dataclasses import dataclass, asdict


@dataclass
class LayerInfo:
    name: str
    type: str
    context: str
    polarity: str
    row: int


@dataclass
class LayerLimits:
    xmin: float
    xmax: float
    ymin: float
    ymax: float


@dataclass
class SymbolHistogram:
    symbols: List[str]
    counts: List[int]


@dataclass
class ComponentInfo:
    refdes: str
    package: str
    x: float
    y: float
    rotation: float
    mirror: bool


@dataclass
class OracleData:
    """Structured data extracted from ODB++ Viewer for comparison"""
    job_name: str
    step_name: str
    layers: List[LayerInfo]
    layer_limits: Dict[str, LayerLimits]
    symbol_histograms: Dict[str, SymbolHistogram]
    component_count: int
    # Add more fields as needed


class ValorClient:
    """Client for communicating with Valor/Genesis via TCP socket"""

    DIR_PREFIX = '@%#%@'

    def __init__(self, host: str = 'localhost', port: int = 56753):
        self.host = host
        self.port = port
        self.sock: Optional[socket.socket] = None
        self.connected = False

    def connect(self) -> bool:
        """Connect to the Valor server"""
        try:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.sock.settimeout(10)
            self.sock.connect((self.host, self.port))
            self.connected = True
            print(f"Connected to Valor server at {self.host}:{self.port}")
            return True
        except socket.error as e:
            print(f"Failed to connect: {e}")
            return False

    def disconnect(self):
        """Close the connection"""
        if self.sock:
            self.sock.close()
            self.connected = False

    def _send_command(self, cmd_type: str, command: str) -> tuple:
        """Send a command and receive response"""
        if not self.connected:
            raise RuntimeError("Not connected to server")

        # Format: @%#%@CMD command\n
        msg = f"{self.DIR_PREFIX}{cmd_type} {command}\n"
        self.sock.sendall(msg.encode('utf-8'))

        # Receive response (status line + result line)
        # For COM: 2 lines, for PAUSE/MOUSE: 3 lines
        lines_expected = 3 if cmd_type in ('PAUSE', 'MOUSE') else 2

        response = b''
        lines_received = 0
        while lines_received < lines_expected:
            chunk = self.sock.recv(4096)
            if not chunk:
                break
            response += chunk
            lines_received = response.count(b'\n')

        lines = response.decode('utf-8').strip().split('\n')
        status = int(lines[0]) if lines[0].lstrip('-').isdigit() else -1
        result = lines[1] if len(lines) > 1 else ''

        return status, result

    def COM(self, command: str) -> tuple:
        """Send a COM command"""
        return self._send_command('COM', command)

    def get_environment(self) -> Dict[str, str]:
        """Get environment variables from the server"""
        if not self.connected:
            raise RuntimeError("Not connected to server")

        msg = f"{self.DIR_PREFIX}GETENVIRONMENT\n"
        self.sock.sendall(msg.encode('utf-8'))

        env = {}
        response = b''
        while True:
            chunk = self.sock.recv(4096)
            response += chunk
            if b'END\n' in response:
                break

        for line in response.decode('utf-8').split('\n'):
            if line == 'END':
                break
            if '=' in line:
                key, value = line.split('=', 1)
                env[key] = value

        return env


class ValorOracle:
    """
    Oracle for extracting structured data from Valor ODB++ Viewer.

    This uses the viewer as an independent implementation to validate
    odb2kicad parser output.
    """

    def __init__(self, client: ValorClient):
        self.client = client
        self.job_name: Optional[str] = None
        self.step_name: Optional[str] = None

    def open_job(self, job_name: str) -> bool:
        """Open a job in the viewer"""
        status, result = self.client.COM(f"open_job, job={job_name}")
        if status == 0:
            self.job_name = job_name
            return True
        print(f"Failed to open job: {result}")
        return False

    def open_step(self, step_name: str) -> bool:
        """Open a step for editing"""
        if not self.job_name:
            raise RuntimeError("No job open")

        status, result = self.client.COM(
            f"open_entity, job={self.job_name}, type=step, name={step_name}, iconic=no"
        )
        if status == 0:
            self.step_name = step_name
            return True
        print(f"Failed to open step: {result}")
        return False

    def get_matrix_info(self) -> List[LayerInfo]:
        """Get layer matrix information"""
        if not self.job_name:
            raise RuntimeError("No job open")

        # Query matrix for layer definitions
        # info,out_file=<file>,args=-t matrix -e job/matrix -d ROW
        status, result = self.client.COM(
            f"info, out_file=$GENESIS_TMP/oracle_info.txt, write_mode=replace, "
            f"args=-t matrix -e {self.job_name}/matrix -d ROW"
        )

        # The result is written to a file - we'd need to parse it
        # For now, return empty list as this requires file access
        # In practice, we'd need the server to return the file contents
        return []

    def get_layer_limits(self, layer_name: str) -> Optional[LayerLimits]:
        """Get bounding box for a layer"""
        if not self.job_name or not self.step_name:
            raise RuntimeError("No job/step open")

        entity_path = f"{self.job_name}/{self.step_name}/{layer_name}"
        status, result = self.client.COM(
            f"info, out_file=$GENESIS_TMP/oracle_limits.txt, write_mode=replace, "
            f"units=mm, args=-t layer -e {entity_path} -d LIMITS"
        )

        # Would need to parse the output file
        return None

    def get_symbol_histogram(self, layer_name: str) -> Optional[SymbolHistogram]:
        """Get symbol usage histogram for a layer"""
        if not self.job_name or not self.step_name:
            raise RuntimeError("No job/step open")

        entity_path = f"{self.job_name}/{self.step_name}/{layer_name}"
        status, result = self.client.COM(
            f"info, out_file=$GENESIS_TMP/oracle_syms.txt, write_mode=replace, "
            f"args=-t layer -e {entity_path} -d SYMS_HIST -p symbol+count"
        )

        return None


def parse_info_file(filepath: str) -> Dict[str, Any]:
    """
    Parse a Valor info output file (csh format).

    Format:
        set gVARNAME = 'value'
        set gARRAY = ('val1' 'val2' 'val3')
    """
    result = {}

    with open(filepath, 'r') as f:
        for line in f:
            line = line.strip()
            if not line or not line.startswith('set '):
                continue

            # Parse: set gVARNAME = value
            match = re.match(r"set\s+(\S+)\s*=\s*(.*)", line)
            if not match:
                continue

            varname = match.group(1)
            value = match.group(2).strip()

            # Check if it's an array (starts with '(')
            if value.startswith('(') and value.endswith(')'):
                # Parse array: ('val1' 'val2' 'val3')
                value = value[1:-1]  # Remove parens
                # Split on quotes, handling escaped quotes
                items = re.findall(r"'([^']*)'", value)
                result[varname] = items
            else:
                # Scalar value - remove quotes if present
                value = value.strip("'\"")
                result[varname] = value

    return result


# Alternative approach: Direct ODB++ file parsing for oracle
# Since the TCP approach requires the viewer to be running with server.pl,
# we can also parse ODB++ files directly for comparison

class DirectOdbOracle:
    """
    Direct ODB++ file parser that serves as an independent oracle.

    This parses ODB++ files using a separate implementation from odb2kicad
    to validate the converter's output.
    """

    def __init__(self, odb_path: str):
        self.odb_path = Path(odb_path)
        self.data: Dict[str, Any] = {}

    def parse(self) -> Dict[str, Any]:
        """Parse the ODB++ directory and extract structured data"""
        self.data = {
            'job_name': self._parse_job_name(),
            'units': self._parse_units(),
            'matrix': self._parse_matrix(),
            'step_order': self._parse_matrix_steps(),
            'steps': {}
        }

        # Find steps
        steps_dir = self.odb_path / 'steps'
        if steps_dir.exists():
            for step_dir in steps_dir.iterdir():
                if step_dir.is_dir():
                    self.data['steps'][step_dir.name] = self._parse_step(step_dir)

        return self.data

    def _parse_job_name(self) -> str:
        """Extract job name from misc/info"""
        info_path = self.odb_path / 'misc' / 'info'
        if info_path.exists():
            with open(info_path, 'r') as f:
                for line in f:
                    if line.startswith('PRODUCT_MODEL_NAME='):
                        return line.split('=', 1)[1].strip()
                    if line.startswith('JOB_NAME='):
                        return line.split('=', 1)[1].strip()
        return self.odb_path.name

    def _parse_units(self) -> str:
        """Extract units from misc/info (MM or INCH)"""
        info_path = self.odb_path / 'misc' / 'info'
        if info_path.exists():
            with open(info_path, 'r') as f:
                for line in f:
                    if line.startswith('UNITS='):
                        return line.split('=', 1)[1].strip().upper()
        return 'INCH'  # Default to inches if not specified

    def _parse_matrix(self) -> List[Dict]:
        """Parse matrix/matrix file for layer definitions"""
        matrix_path = self.odb_path / 'matrix' / 'matrix'
        layers = []

        if not matrix_path.exists():
            return layers

        current_layer = {}
        with open(matrix_path, 'r') as f:
            for line in f:
                line = line.strip()

                if line == 'LAYER {':
                    current_layer = {}
                elif line == '}':
                    if current_layer:
                        layers.append(current_layer)
                    current_layer = {}
                elif '=' in line:
                    key, value = line.split('=', 1)
                    current_layer[key.strip().lower()] = value.strip()

        return layers

    def _parse_matrix_steps(self) -> List[str]:
        """The matrix's STEP names in COL order: the converter's first step is the first of these
        that has a folder"""
        text = read_odb_text(self.odb_path / 'matrix' / 'matrix') or ''
        steps, cur, inside = [], {}, False
        for line in text.splitlines():
            line = line.strip()
            if line.startswith('STEP') and line.endswith('{'):
                cur, inside = {}, True
            elif line == '}' and inside:
                if cur.get('NAME'):
                    steps.append((int(cur.get('COL', '0') or 0) if str(cur.get('COL', '0')).isdigit() else 0, cur['NAME']))
                inside = False
            elif inside and '=' in line:
                k, v = line.split('=', 1)
                cur[k.strip().upper()] = v.strip()
        return [n for _, n in sorted(steps, key=lambda t: t[0])]

    def _parse_step(self, step_dir: Path) -> Dict:
        """Parse a step directory"""
        step_data = {
            'name': step_dir.name,
            'layers': {},
            'profile': None,
            'eda_data': None
        }

        # Parse layers
        layers_dir = step_dir / 'layers'
        if layers_dir.exists():
            for layer_dir in layers_dir.iterdir():
                if layer_dir.is_dir():
                    step_data['layers'][layer_dir.name] = self._parse_layer(layer_dir)

        # Parse profile (board outline)
        profile_path = step_dir / 'profile'
        if odb_file(profile_path):
            step_data['profile'] = self._parse_profile(profile_path)

        # The stephdr origin moves the whole step, as the converter does
        ox, oy = self._parse_step_origin(step_dir)
        step_data['origin'] = (ox, oy)
        if ox or oy:
            bbox = (step_data['profile'] or {}).get('bbox')
            if bbox:
                bbox.update(xmin=bbox['xmin'] + ox, xmax=bbox['xmax'] + ox,
                            ymin=bbox['ymin'] + oy, ymax=bbox['ymax'] + oy)
            for layer in step_data['layers'].values():
                for comp in layer.get('components', []):
                    comp['x'] += ox
                    comp['y'] += oy
                    for term in comp['terminals']:  # TOP records are board coordinates
                        term['x'] += ox
                        term['y'] += oy

        return step_data

    def _job_units_factor(self, units: Optional[str]) -> float:
        """mm per file unit: the file's own UNITS, else misc/info's, else INCH (the converter's rule)"""
        u = (units or self.data.get('units') or 'INCH').upper()
        return 1.0 if u.startswith('MM') else 25.4

    def _parse_step_origin(self, step_dir: Path) -> tuple:
        """stephdr X_ORIGIN / Y_ORIGIN in mm (0 when absent or not a number)"""
        text = read_odb_text(step_dir / 'stephdr') or ''
        units, ox, oy = None, 0.0, 0.0
        for line in text.splitlines():
            key, _, val = line.strip().partition('=')
            try:
                if key == 'UNITS':
                    units = val.strip()
                elif key == 'X_ORIGIN':
                    ox = float(val)
                elif key == 'Y_ORIGIN':
                    oy = float(val)
            except ValueError:
                pass
        k = self._job_units_factor(units)
        return ox * k, oy * k

    def _parse_layer(self, layer_dir: Path) -> Dict:
        """Parse a layer directory"""
        layer_data = {
            'name': layer_dir.name,
            'feature_count': 0,
            'component_count': 0,
            'limits': None,
            'symbols': {}
        }

        # Count features
        features_path = layer_dir / 'features'
        if odb_file(features_path):
            layer_data.update(self._parse_features(features_path))

        # Parse components (full data including position/rotation)
        components_path = layer_dir / 'components'
        if odb_file(components_path):
            components = self._parse_components(components_path)
            layer_data['component_count'] = len(components)
            layer_data['components'] = components

        return layer_data

    def _parse_features(self, features_path: Path) -> Dict:
        """Parse features file and extract statistics"""
        result = {
            'feature_count': 0,
            'pad_count': 0,
            'line_count': 0,
            'arc_count': 0,
            'surface_count': 0,
            'symbols': {},
            'limits': {'xmin': float('inf'), 'ymin': float('inf'),
                      'xmax': float('-inf'), 'ymax': float('-inf')}
        }

        symbol_defs = {}  # idx -> symbol_name

        with io.StringIO(read_odb_text(features_path) or '') as f:
            for line in f:
                line = line.strip()

                # Symbol definition: $0 symbol_name
                if line.startswith('$'):
                    parts = line.split(None, 1)
                    if len(parts) == 2:
                        idx = int(parts[0][1:])
                        symbol_defs[idx] = parts[1]

                # Pad: P x y ...
                elif line.startswith('P '):
                    result['pad_count'] += 1
                    result['feature_count'] += 1
                    parts = line.split()
                    if len(parts) >= 3:
                        x, y = float(parts[1]), float(parts[2])
                        self._update_limits(result['limits'], x, y)

                # Line: L xs ys xe ye ...
                elif line.startswith('L '):
                    result['line_count'] += 1
                    result['feature_count'] += 1
                    parts = line.split()
                    if len(parts) >= 5:
                        xs, ys = float(parts[1]), float(parts[2])
                        xe, ye = float(parts[3]), float(parts[4])
                        self._update_limits(result['limits'], xs, ys)
                        self._update_limits(result['limits'], xe, ye)

                # Arc: A xs ys xe ye xc yc ...
                elif line.startswith('A '):
                    result['arc_count'] += 1
                    result['feature_count'] += 1
                    parts = line.split()
                    if len(parts) >= 7:
                        xs, ys = float(parts[1]), float(parts[2])
                        xe, ye = float(parts[3]), float(parts[4])
                        self._update_limits(result['limits'], xs, ys)
                        self._update_limits(result['limits'], xe, ye)

                # Surface: S P ...
                elif line.startswith('S '):
                    result['surface_count'] += 1
                    result['feature_count'] += 1

        # Clean up limits if no features found
        if result['limits']['xmin'] == float('inf'):
            result['limits'] = None

        result['symbols'] = symbol_defs
        return result

    def _update_limits(self, limits: Dict, x: float, y: float):
        """Update bounding box limits"""
        limits['xmin'] = min(limits['xmin'], x)
        limits['ymin'] = min(limits['ymin'], y)
        limits['xmax'] = max(limits['xmax'], x)
        limits['ymax'] = max(limits['ymax'], y)

    def _parse_components(self, components_path: Path) -> List[Dict]:
        """Parse components file and extract full component data including terminals"""
        components = []
        units = 'INCH'  # ODB++ default is INCH when not specified
        scale = 25.4  # Convert inches to mm
        current_comp = None

        with io.StringIO(read_odb_text(components_path) or '') as f:
            for line in f:
                line = line.strip()

                # Units line
                if line.startswith('UNITS='):
                    units = line.split('=')[1].strip()
                    scale = 25.4 if units == 'INCH' else 1.0

                # CMP record: CMP pkgIdx x y rotation mirror refdes footprint ;attrs
                elif line.startswith('CMP '):
                    # Save previous component if exists
                    if current_comp:
                        components.append(current_comp)

                    # Separate attrs from main part
                    main_part = line
                    if ';' in line:
                        main_part = line.split(';')[0]

                    parts = main_part[4:].split()
                    if len(parts) >= 7:
                        current_comp = {
                            'pkg_idx': int(parts[0]),
                            'x': float(parts[1]) * scale,
                            'y': float(parts[2]) * scale,
                            'rotation': float(parts[3]),
                            'mirror': parts[4],  # 'N' or 'M'
                            'refdes': parts[5],
                            'footprint': parts[6],
                            'terminals': []
                        }
                    else:
                        current_comp = None

                # TOP record: TOP termIdx x y rotation mirror netIdx symIdx pinNum
                elif line.startswith('TOP ') and current_comp:
                    parts = line[4:].split()
                    if len(parts) >= 8:
                        term = {
                            'term_idx': int(parts[0]),
                            'x': float(parts[1]) * scale,
                            'y': float(parts[2]) * scale,
                            'rotation': float(parts[3]),
                            'mirror': parts[4],
                            'net_idx': int(parts[5]),
                            'sym_idx': int(parts[6]),
                            'pin_num': parts[7]
                        }
                        current_comp['terminals'].append(term)

            # Don't forget the last component
            if current_comp:
                components.append(current_comp)

        return components

    def _parse_profile(self, profile_path: Path) -> Dict:
        """Parse the board profile (outline): its extent in mm, ODB++ frame (Y up).

        bbox is None when the profile draws nothing. Arcs count with their bulge, not just
        their end points.
        """
        text = read_odb_text(profile_path) or ''
        geo = parse_feature_geometry(profile_path)
        has_units = re.search(r'^\s*(UNITS\s*=|U\s)', text, re.M) is not None
        k = self._job_units_factor(geo['units'] if has_units else None)
        box = BBox()
        for r in geo['records']:
            if r[0] == 'S':
                for _hole, contour in r[2]:
                    prev = None
                    for seg in contour:
                        if seg[0] == 'S':
                            prev = (seg[1], seg[2])
                            box.add(*prev)
                        elif prev is not None:  # ('C', xe, ye, xc, yc, cw)
                            box.add_arc(prev, (seg[1], seg[2]), (seg[3], seg[4]), seg[5])
                            prev = (seg[1], seg[2])
            elif r[0] == 'L':
                box.add(r[1], r[2])
                box.add(r[3], r[4])
            elif r[0] == 'A':
                box.add_arc((r[1], r[2]), (r[3], r[4]), (r[5], r[6]), r[9])
            elif r[0] in ('P', 'T', 'B'):
                box.add(r[1], r[2])
        return {
            'exists': True,
            'has_content': len(text.strip()) > 0,
            'bbox': box.scaled(k),
        }


class BBox:
    """An axis-aligned extent that grows by points and circular arcs (any frame, any units)."""

    def __init__(self):
        self.xmin = self.ymin = float('inf')
        self.xmax = self.ymax = float('-inf')

    def add(self, x: float, y: float):
        self.xmin, self.xmax = min(self.xmin, x), max(self.xmax, x)
        self.ymin, self.ymax = min(self.ymin, y), max(self.ymax, y)

    def add_arc(self, start, end, center, cw: bool):
        """An arc from start to end about center; cw: clockwise in a Y-up frame (counter-clockwise
        on screen in a Y-down one). start == end is a full circle."""
        import math
        sx, sy = start
        ex, ey = end
        cx, cy = center
        r = math.hypot(sx - cx, sy - cy)
        self.add(sx, sy)
        self.add(ex, ey)
        a0 = math.atan2(sy - cy, sx - cx)
        a1 = math.atan2(ey - cy, ex - cx)
        # sweep counter-clockwise from a0 (for a clockwise arc, from a1 to a0)
        lo, hi = (a1, a0) if cw else (a0, a1)
        sweep = (hi - lo) % (2 * math.pi)
        if abs(sx - ex) < 1e-9 and abs(sy - ey) < 1e-9:
            sweep = 2 * math.pi
        for q in range(4):  # the axis extremes the sweep passes through
            a = q * math.pi / 2
            if (a - lo) % (2 * math.pi) <= sweep + 1e-12:
                self.add(cx + r * math.cos(a), cy + r * math.sin(a))

    def add_circle(self, cx: float, cy: float, r: float):
        self.add(cx - r, cy - r)
        self.add(cx + r, cy + r)

    def empty(self) -> bool:
        return self.xmin == float('inf')

    def scaled(self, k: float) -> Optional[Dict[str, float]]:
        if self.empty():
            return None
        return {'xmin': self.xmin * k, 'ymin': self.ymin * k, 'xmax': self.xmax * k, 'ymax': self.ymax * k}


# ----------------------------------------------------------------------------------------------
# Feature geometry: the records themselves, not just counts, for the round-up's
# reference rasterizer (odb_render.py). Kept next to DirectOdbOracle so both read ODB++ the same way.
# ----------------------------------------------------------------------------------------------

def _num(tok: str) -> float:
    return float(tok)


def unix_decompress(data: bytes) -> Optional[bytes]:
    """UNIX compress (LZW, magic 1f 9d) -> the original bytes; None when it is not such a stream.
    ODB++ stores any file of a job this way under <name>.z.

    Codes are packed low bit first, 9 bits wide at first and wider when the table outgrows them;
    in block mode code 256 clears the table. At either event the stream skips to the next
    multiple of eight codes of the old width, counted from where that width began."""
    if len(data) < 3 or data[0] != 0x1F or data[1] != 0x9D:
        return None
    max_bits, block = data[2] & 0x1F, bool(data[2] & 0x80)
    if not 9 <= max_bits <= 16:
        return None
    max_max = 1 << max_bits
    prefix = [0] * max_max
    suffix = list(range(256)) + [0] * (max_max - 256)
    out = bytearray()
    total = len(data) * 8
    pos = start = 24
    n_bits = 9
    max_code = (1 << n_bits) - 1
    free = 257 if block else 256
    old = -1
    fin = 0
    while pos + n_bits <= total:
        if free > max_code:  # the table has outgrown the code width
            rem = (pos - start) % (n_bits * 8)
            pos += (n_bits * 8 - rem) if rem else 0
            start = pos
            n_bits += 1
            max_code = max_max if n_bits == max_bits else (1 << n_bits) - 1
            continue
        b = pos >> 3
        code = (int.from_bytes(data[b:b + 3], "little") >> (pos & 7)) & ((1 << n_bits) - 1)
        pos += n_bits
        if old == -1:
            if code >= 256:
                return None
            fin = old = code
            out.append(fin)
            continue
        if code == 256 and block:
            free = 256
            rem = (pos - start) % (n_bits * 8)
            pos += (n_bits * 8 - rem) if rem else 0
            start = pos
            n_bits = 9
            max_code = (1 << n_bits) - 1
            continue
        in_code = code
        stack = []
        if code >= free:
            if code > free:
                return None
            stack.append(fin)
            code = old
        while code >= 256:
            stack.append(suffix[code])
            code = prefix[code]
        fin = suffix[code]
        stack.append(fin)
        out.extend(reversed(stack))
        if free < max_max:
            prefix[free] = old
            suffix[free] = fin
            free += 1
        old = in_code
    return bytes(out)


def odb_file(path) -> Optional[Path]:
    """``path`` when it is a file, else its compressed twin (<path>.z / .Z), else None."""
    p = Path(path)
    if p.is_file():
        return p
    for ext in (".z", ".Z"):
        q = p.with_name(p.name + ext)
        if q.is_file():
            return q
    return None


def read_odb_text(path) -> Optional[str]:
    """The text of an ODB++ record file, plain or compressed (<path>.z); None when there is none."""
    p = odb_file(path)
    if p is None:
        return None
    raw = p.read_bytes()
    if p.suffix.lower() == ".z" and p != Path(path):
        raw = unix_decompress(raw)
        if raw is None:
            return None
    return raw.decode("latin-1")


def parse_feature_geometry(features_path) -> Dict[str, Any]:
    """Parse an ODB++ ``features`` file (layer, profile or user symbol) into geometry records.

    Returns {'units': 'MM'|'INCH', 'symbols': {idx: (name, unit_override or None)}, 'records': [...]}
    where each record is one of (coordinates in the file's units, as written):
      ('P', x, y, sym, pol, orient, angle)            orient: 0-7 legacy, 8 rotate, 9 mirror+rotate
      ('L', xs, ys, xe, ye, sym, pol)
      ('A', xs, ys, xe, ye, xc, yc, sym, pol, cw)
      ('S', pol, contours)   contours = [(is_hole, [('S', x, y) | ('C', xe, ye, xc, yc, cw), ...])]
      ('T', x, y, pol, font, mirror, angle, xsize, ysize, weight, text, vertical)
                             text (T, or V drawn downwards): x, y the lower left of the first
                             character cell, angle clockwise, weight in font line widths
      ('B', x, y, pol)       barcode (not rasterized; counted)
    ``pol`` is True for positive. Unknown lines are counted in 'skipped'.
    """
    units = 'INCH'
    symbols: Dict[int, tuple] = {}
    records: List[tuple] = []
    skipped = 0
    surf = None
    contour = None
    text = read_odb_text(features_path)
    if text is None:
        return {'units': units, 'symbols': symbols, 'records': records, 'skipped': 0}
    if True:
        for raw in text.splitlines():
            line = raw.strip()
            if not line or line.startswith('#'):
                continue
            body = line.split(';', 1)[0].strip()
            if body.startswith('UNITS='):
                units = body.split('=', 1)[1].strip().upper()
                continue
            if body.startswith('U ') and surf is None:
                units = body.split()[1].upper()
                continue
            tok = body.split()
            t = tok[0]
            try:
                if surf is not None:
                    if t == 'OB':
                        contour = [('S', _num(tok[1]), _num(tok[2]))]
                        surf[2].append((len(tok) > 3 and tok[3].upper() == 'H', contour))
                    elif t == 'OS' and contour is not None:
                        contour.append(('S', _num(tok[1]), _num(tok[2])))
                    elif t == 'OC' and contour is not None:
                        contour.append(('C', _num(tok[1]), _num(tok[2]), _num(tok[3]), _num(tok[4]),
                                        len(tok) > 5 and tok[5].upper() == 'Y'))
                    elif t == 'OE':
                        contour = None
                    elif t == 'SE':
                        records.append(tuple(surf))
                        surf = None
                    continue
                if t.startswith('$'):
                    parts = body.split()
                    idx = int(parts[0][1:])
                    ov = parts[2].upper() if len(parts) > 2 and parts[2].upper() in ('I', 'M') else None
                    symbols[idx] = (parts[1], ov)
                elif t == 'P':
                    orient = int(tok[6]) if len(tok) > 6 else 0
                    ang = _num(tok[7]) if orient in (8, 9) and len(tok) > 7 else 0.0
                    records.append(('P', _num(tok[1]), _num(tok[2]), int(tok[3]), tok[4] != 'N', orient, ang))
                elif t == 'L':
                    records.append(('L', _num(tok[1]), _num(tok[2]), _num(tok[3]), _num(tok[4]), int(tok[5]), tok[6] != 'N'))
                elif t == 'A':
                    # A xs ys xe ye xc yc sym [pol] [dcode] [cw]: the direction token is optional (some
                    # producers do not write it); without it the arc runs counter-clockwise
                    rest = [v.upper() for v in tok[8:]]
                    pol = rest.pop(0) != 'N' if rest and rest[0] in ('P', 'N') else True
                    if rest and re.fullmatch(r'[-+]?[\d.]+', rest[0]):
                        rest.pop(0)
                    records.append(('A', _num(tok[1]), _num(tok[2]), _num(tok[3]), _num(tok[4]), _num(tok[5]), _num(tok[6]),
                                    int(tok[7]), pol, bool(rest) and rest[0] == 'Y'))
                elif t == 'S':
                    surf = ['S', tok[1] != 'N', []]
                elif t in ('T', 'V'):
                    # T x y font pol orient [angle] xsize ysize width 'text' version: cut at the quotes
                    q1, q2 = line.find("'"), line.rfind("'")
                    head = (line[:q1] if q1 >= 0 else body).split()
                    text = line[q1 + 1:q2] if 0 <= q1 < q2 else (head.pop() if len(head) > 9 else '')
                    orient = int(head[5]) if len(head) > 5 else 0
                    k = 6
                    ang = 90.0 * (orient % 4)
                    if orient in (8, 9):
                        ang = _num(head[k])
                        k += 1
                    records.append(('T', _num(head[1]), _num(head[2]), head[4] != 'N' if len(head) > 4 else True,
                                    head[3] if len(head) > 3 else 'standard', orient in (4, 5, 6, 7, 9), ang,
                                    _num(head[k]), _num(head[k + 1]), _num(head[k + 2]), text, t == 'V'))
                elif t == 'B':
                    records.append(('B', _num(tok[1]), _num(tok[2]), True))
                elif t == 'F' or t.startswith('@') or t.startswith('&') or t == 'ID=' or '=' in t:
                    continue
                else:
                    skipped += 1
            except (IndexError, ValueError):
                skipped += 1
    return {'units': units, 'symbols': symbols, 'records': records, 'skipped': skipped}


def find_odb_root(path) -> Optional[Path]:
    """The ODB++ job folder at or under ``path`` (the dir holding matrix/matrix), or None."""
    p = Path(path)
    if (p / 'matrix' / 'matrix').is_file():
        return p
    for m in sorted(p.rglob('matrix')):
        if m.is_file() and m.parent.name == 'matrix':
            return m.parent.parent
    return None


def ci_child(parent: Path, name: str) -> Optional[Path]:
    """parent/name, matched case-insensitively (matrix names are upper case, folders often lower)."""
    exact = parent / name
    if exact.exists():
        return exact
    low = name.lower()
    if parent.is_dir():
        for c in parent.iterdir():
            if c.name.lower() == low:
                return c
    return None


def compare_oracle_to_kicad(oracle_data: Dict, kicad_path: str) -> List[str]:
    """
    Compare oracle data (from ODB++ viewer or direct parse) to
    odb2kicad output.

    Returns list of discrepancies found.
    """
    errors = []

    # Parse KiCad file and compare
    # This would use a KiCad parser - for now just placeholder

    # Example comparisons:
    # - Component count matches
    # - Layer count matches
    # - Bounding box within tolerance
    # - Feature counts match

    return errors


def main():
    import argparse

    parser = argparse.ArgumentParser(description='Valor ODB++ Oracle Client')
    parser.add_argument('odb_path', help='Path to ODB++ directory')
    parser.add_argument('--mode', choices=['tcp', 'direct'], default='direct',
                       help='Oracle mode: tcp (requires running viewer) or direct (parse files)')
    parser.add_argument('--host', default='localhost', help='Valor server host')
    parser.add_argument('--port', type=int, default=56753, help='Valor server port')
    parser.add_argument('--output', '-o', help='Output JSON file')

    args = parser.parse_args()

    if args.mode == 'tcp':
        # TCP mode - connect to running viewer
        client = ValorClient(args.host, args.port)
        if not client.connect():
            print("Failed to connect to Valor server.")
            print("Make sure the viewer is running with server.pl active.")
            sys.exit(1)

        try:
            oracle = ValorOracle(client)
            # Would need to open job and query data
            print("TCP mode not fully implemented - use direct mode")
        finally:
            client.disconnect()

    else:
        # Direct mode - parse ODB++ files directly
        oracle = DirectOdbOracle(args.odb_path)
        data = oracle.parse()

        # Output as JSON
        output = json.dumps(data, indent=2, default=str)

        if args.output:
            with open(args.output, 'w') as f:
                f.write(output)
            print(f"Oracle data written to {args.output}")
        else:
            print(output)


if __name__ == '__main__':
    main()
