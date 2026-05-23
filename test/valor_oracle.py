#!/usr/bin/env python3
"""
Valor ODB++ Viewer Oracle Client

Connects to the Siemens ODB++ Viewer via TCP socket to extract
structured data for validating odb2kicad conversions.

The viewer must be running with server.pl active on port 56753.

Usage:
    python valor_oracle.py <odb_path> [--host localhost] [--port 56753]
"""

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
        return 'INCH'  # Default to inch (mils) if not specified

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
        if profile_path.exists():
            step_data['profile'] = self._parse_profile(profile_path)

        return step_data

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
        if features_path.exists():
            layer_data.update(self._parse_features(features_path))

        # Parse components (full data including position/rotation)
        components_path = layer_dir / 'components'
        if components_path.exists():
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

        with open(features_path, 'r') as f:
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
        units = 'MM'  # default
        scale = 1.0
        current_comp = None

        with open(components_path, 'r') as f:
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
        """Parse board profile/outline"""
        # Simplified - just check if it exists and has content
        with open(profile_path, 'r') as f:
            content = f.read()
            return {
                'exists': True,
                'has_content': len(content.strip()) > 0
            }


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
