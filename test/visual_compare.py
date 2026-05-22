#!/usr/bin/env python3
"""
Visual Comparison Tool for odb2kicad Validation

Captures screenshots from ODB++ Viewer and KiCad for visual comparison.
Images can be compared using Claude's vision capabilities.

Requirements:
    - Siemens ODB++ Viewer installed
    - KiCad with kicad-cli
    - pywinauto (pip install pywinauto)
    - pillow (pip install pillow)

Usage:
    python visual_compare.py <odb_path> <kicad_path> [--output-dir ./output]
"""

import os
import sys
import time
import argparse
import subprocess
import shutil
from pathlib import Path
from typing import Optional, Tuple
from datetime import datetime


def find_kicad_cli() -> Optional[Path]:
    """Find kicad-cli executable"""
    candidates = [
        Path(r"C:\Program Files\KiCad\8.0\bin\kicad-cli.exe"),
        Path(r"C:\Program Files\KiCad\9.0\bin\kicad-cli.exe"),
        Path(r"C:\Program Files\KiCad\bin\kicad-cli.exe"),
    ]

    for path in candidates:
        if path.exists():
            return path

    # Try PATH
    result = shutil.which("kicad-cli")
    if result:
        return Path(result)

    return None


def find_odb_viewer() -> Optional[Path]:
    """Find ODB++ Viewer executable"""
    candidates = [
        Path(r"C:\SiemensEDA\Valor\ODB++_Viewer\odbviewer_2510\edir\get\odbviewer.exe"),
    ]

    for path in candidates:
        if path.exists():
            return path

    return None


def render_kicad_pcb(kicad_path: Path, output_path: Path, side: str = 'top',
                     width: int = 2000, height: int = 2000) -> Tuple[bool, str]:
    """
    Render a KiCad PCB to an image using kicad-cli.

    Args:
        kicad_path: Path to .kicad_pcb file
        output_path: Path for output image (.png or .jpg)
        side: 'top' or 'bottom'
        width: Image width in pixels
        height: Image height in pixels

    Returns:
        Tuple of (success, message)
    """
    kicad_cli = find_kicad_cli()
    if not kicad_cli:
        return False, "kicad-cli not found. Install KiCad or add to PATH."

    try:
        cmd = [
            str(kicad_cli),
            'pcb', 'render',
            '--output', str(output_path),
            '--side', side,
            '--width', str(width),
            '--height', str(height),
            '--background', 'opaque',
            str(kicad_path)
        ]

        result = subprocess.run(cmd, capture_output=True, text=True, timeout=120)

        if result.returncode != 0:
            return False, f"kicad-cli failed: {result.stderr}"

        if not output_path.exists():
            return False, "Output file not created"

        return True, f"Rendered to {output_path}"

    except subprocess.TimeoutExpired:
        return False, "kicad-cli timed out"
    except Exception as e:
        return False, f"Error: {e}"


def capture_odb_viewer_screenshot(odb_path: Path, output_path: Path,
                                   timeout: int = 30) -> Tuple[bool, str]:
    """
    Capture a screenshot from the ODB++ Viewer.

    This uses pywinauto to automate the viewer.
    The viewer must be installed and accessible.

    Args:
        odb_path: Path to ODB++ directory
        output_path: Path for output screenshot
        timeout: Timeout in seconds

    Returns:
        Tuple of (success, message)
    """
    try:
        from pywinauto.application import Application
        from pywinauto import Desktop
    except ImportError:
        return False, "pywinauto not installed. Run: pip install pywinauto"

    viewer_exe = find_odb_viewer()
    if not viewer_exe:
        return False, "ODB++ Viewer not found"

    try:
        # Check if viewer is already running
        try:
            app = Application(backend='uia').connect(path=str(viewer_exe), timeout=2)
            print("Connected to existing ODB++ Viewer instance")
        except:
            # Launch the viewer
            print(f"Launching ODB++ Viewer: {viewer_exe}")
            app = Application(backend='uia').start(str(viewer_exe))
            time.sleep(5)  # Wait for startup

        # Find main window
        main_window = app.window(title_re=".*ODB.*", visible_only=True)
        main_window.wait('visible', timeout=timeout)

        # TODO: Load the ODB++ file if not already loaded
        # This would require navigating File->Open menu

        # Wait for content to load
        time.sleep(2)

        # Capture screenshot
        screenshot = main_window.capture_as_image()
        screenshot.save(str(output_path))

        return True, f"Screenshot saved to {output_path}"

    except Exception as e:
        return False, f"Screenshot capture failed: {e}"


def create_comparison_html(odb_img: Path, kicad_img: Path, output_dir: Path) -> Path:
    """Create an HTML file for side-by-side comparison"""
    html_path = output_dir / 'comparison.html'

    html_content = f"""<!DOCTYPE html>
<html>
<head>
    <title>ODB++ vs KiCad Comparison</title>
    <style>
        body {{ font-family: Arial, sans-serif; margin: 20px; background: #1a1a1a; color: white; }}
        h1 {{ text-align: center; }}
        .container {{ display: flex; justify-content: center; gap: 20px; flex-wrap: wrap; }}
        .panel {{ text-align: center; }}
        .panel img {{ max-width: 800px; border: 2px solid #444; }}
        .label {{ font-size: 1.2em; margin: 10px 0; }}
        .info {{ margin: 20px; padding: 10px; background: #333; border-radius: 5px; }}
    </style>
</head>
<body>
    <h1>Visual Comparison: ODB++ Viewer vs KiCad</h1>
    <div class="info">
        <p>Generated: {datetime.now().isoformat()}</p>
        <p>Compare these images to identify conversion differences.</p>
    </div>
    <div class="container">
        <div class="panel">
            <div class="label">ODB++ Viewer (Ground Truth)</div>
            <img src="{odb_img.name}" alt="ODB++ Viewer">
        </div>
        <div class="panel">
            <div class="label">KiCad (Converted)</div>
            <img src="{kicad_img.name}" alt="KiCad Output">
        </div>
    </div>
</body>
</html>
"""

    with open(html_path, 'w') as f:
        f.write(html_content)

    return html_path


def generate_comparison_images(odb_path: Path, kicad_path: Path,
                                output_dir: Path) -> dict:
    """
    Generate comparison images from ODB++ and KiCad files.

    Returns dict with paths and status.
    """
    output_dir.mkdir(parents=True, exist_ok=True)

    results = {
        'odb_image': None,
        'kicad_image': None,
        'html': None,
        'errors': []
    }

    # Generate KiCad render
    kicad_img = output_dir / 'kicad_render.png'
    print(f"Rendering KiCad PCB...")
    success, msg = render_kicad_pcb(kicad_path, kicad_img)
    if success:
        results['kicad_image'] = kicad_img
        print(f"  {msg}")
    else:
        results['errors'].append(f"KiCad render: {msg}")
        print(f"  Error: {msg}")

    # Capture ODB Viewer screenshot
    odb_img = output_dir / 'odb_screenshot.png'
    print(f"Capturing ODB++ Viewer screenshot...")
    success, msg = capture_odb_viewer_screenshot(odb_path, odb_img)
    if success:
        results['odb_image'] = odb_img
        print(f"  {msg}")
    else:
        results['errors'].append(f"ODB screenshot: {msg}")
        print(f"  Error: {msg}")

    # Create comparison HTML if both images exist
    if results['odb_image'] and results['kicad_image']:
        html_path = create_comparison_html(results['odb_image'],
                                           results['kicad_image'],
                                           output_dir)
        results['html'] = html_path
        print(f"Comparison HTML: {html_path}")

    return results


def main():
    parser = argparse.ArgumentParser(description='Visual comparison for odb2kicad')
    parser.add_argument('odb_path', help='Path to ODB++ directory')
    parser.add_argument('kicad_path', help='Path to .kicad_pcb file')
    parser.add_argument('--output-dir', '-o', default='./visual_output',
                       help='Output directory for images')
    parser.add_argument('--kicad-only', action='store_true',
                       help='Only render KiCad (skip ODB viewer)')

    args = parser.parse_args()

    odb_path = Path(args.odb_path)
    kicad_path = Path(args.kicad_path)
    output_dir = Path(args.output_dir)

    if not odb_path.exists():
        print(f"Error: ODB++ path not found: {odb_path}")
        sys.exit(1)

    if not kicad_path.exists():
        print(f"Error: KiCad file not found: {kicad_path}")
        sys.exit(1)

    print(f"ODB++ path: {odb_path}")
    print(f"KiCad file: {kicad_path}")
    print(f"Output dir: {output_dir}")
    print()

    if args.kicad_only:
        # Just render KiCad
        output_dir.mkdir(parents=True, exist_ok=True)
        kicad_img = output_dir / 'kicad_render.png'
        success, msg = render_kicad_pcb(kicad_path, kicad_img)
        print(msg)
        sys.exit(0 if success else 1)

    results = generate_comparison_images(odb_path, kicad_path, output_dir)

    if results['errors']:
        print("\nErrors:")
        for error in results['errors']:
            print(f"  - {error}")

    print("\nTo compare visually with Claude:")
    print("  1. Share the images from the output directory")
    print("  2. Ask Claude to identify differences between ODB++ and KiCad renders")

    if results['html']:
        print(f"\nOr open {results['html']} in a browser for side-by-side view")


if __name__ == '__main__':
    main()
