#!/usr/bin/env python3
"""
Oracle Test Runner for odb2kicad Validation

Runs automated validation tests using the oracle comparison system.
Supports both structural comparison and visual comparison (screenshots).

Usage:
    python run_oracle_tests.py                    # Run all tests
    python run_oracle_tests.py --sample simple    # Run specific sample
    python run_oracle_tests.py --visual           # Include visual comparison
"""

import os
import sys
import json
import argparse
import subprocess
from pathlib import Path
from datetime import datetime
from typing import Dict, List, Optional, Tuple

# Add test directory to path
sys.path.insert(0, str(Path(__file__).parent))

from valor_oracle import DirectOdbOracle
from compare_oracle import KiCadPcbParser, OracleComparator, print_results, redact_message


class TestResult:
    def __init__(self, name: str):
        self.name = name
        self.passed = False
        self.oracle_data: Dict = {}
        self.kicad_data: Dict = {}
        self.comparison_results: List = []
        self.error: Optional[str] = None
        self.visual_diff_path: Optional[str] = None


class OracleTestRunner:
    """Runs oracle validation tests on ODB++ samples"""

    def __init__(self, project_root: Path):
        self.project_root = project_root
        self.samples_dir = project_root / 'samples'
        self.output_dir = project_root / 'test' / 'output'
        self.output_dir.mkdir(parents=True, exist_ok=True)

        # Sample configurations (built-in samples)
        self.samples = {
            'simple': {
                'odb_path': 'odb-output',
                'description': '2-resistor simple board',
                'expected_components': 2,
                'expected_nets': ['GND', 'VCC', 'SIG']
            },
            'kitchen-sink': {
                'odb_path': 'odb-kitchen-sink',
                'description': 'Complex board with vias, TH, zones, arcs',
                'expected_components': None,  # TBD
                'expected_nets': None
            }
        }

        # Auto-discover private samples in samples/private/
        private_dir = self.samples_dir / 'private'
        if private_dir.exists():
            for item in private_dir.iterdir():
                if item.is_dir() and (item / 'matrix').exists():
                    # Looks like an ODB++ directory
                    sample_name = f"private/{item.name}"
                    self.samples[sample_name] = {
                        'odb_path': f'private/{item.name}',
                        'description': f'Private sample: {item.name}',
                        'expected_components': None,
                        'expected_nets': None
                    }

    def find_converter(self) -> Optional[Path]:
        """Find the odb2kicad converter executable"""
        # Check various locations
        candidates = [
            self.project_root / 'odb2kicad' / 'build' / 'odb2kicad',
            self.project_root / 'odb2kicad' / 'build' / 'odb2kicad.exe',
            self.project_root / 'odb2kicad' / 'build' / 'Release' / 'odb2kicad.exe',
        ]

        for path in candidates:
            if path.exists():
                return path

        return None

    def convert_odb_to_kicad(self, odb_path: Path, output_path: Path) -> Tuple[bool, str]:
        """Run odb2kicad converter on ODB++ directory"""
        converter = self.find_converter()

        if not converter:
            # Try WASM version via node
            wasm_index = self.project_root / 'wasm' / 'dist' / 'index.mjs'
            if wasm_index.exists():
                # Would need to implement WASM conversion here
                return False, "WASM converter found but not implemented in test runner"
            return False, "No converter found. Build with: cmake -S odb2kicad -B odb2kicad/build && cmake --build odb2kicad/build"

        try:
            result = subprocess.run(
                [str(converter), str(odb_path), str(output_path)],
                capture_output=True,
                text=True,
                timeout=60
            )

            if result.returncode != 0:
                return False, f"Converter failed: {result.stderr}"

            return True, "Conversion successful"

        except subprocess.TimeoutExpired:
            return False, "Converter timed out"
        except Exception as e:
            return False, f"Converter error: {e}"

    def run_test(self, sample_name: str, use_existing_kicad: bool = True,
                  redacted: bool = False) -> TestResult:
        """Run oracle test on a sample"""
        result = TestResult(sample_name)

        if sample_name not in self.samples:
            result.error = f"Unknown sample: {sample_name}"
            return result

        config = self.samples[sample_name]
        odb_path = self.samples_dir / config['odb_path']

        if not odb_path.exists():
            result.error = f"ODB++ path not found: {odb_path}"
            return result

        print(f"\n{'='*60}")
        if redacted:
            print(f"Testing: {sample_name} [REDACTED]")
        else:
            print(f"Testing: {sample_name} - {config['description']}")
        print(f"{'='*60}")

        # Parse oracle data
        if redacted:
            print("Parsing ODB++ oracle: [REDACTED]")
        else:
            print(f"Parsing ODB++ oracle: {odb_path}")
        try:
            oracle = DirectOdbOracle(str(odb_path))
            result.oracle_data = oracle.parse()
        except Exception as e:
            result.error = f"Oracle parsing failed: {e}"
            return result

        # Find or create KiCad file
        kicad_path = self.output_dir / f"{sample_name}_converted.kicad_pcb"

        if use_existing_kicad:
            # Check for existing converted file
            existing = self.samples_dir / 'test.kicad_pcb'
            if existing.exists() and sample_name == 'simple':
                kicad_path = existing
        else:
            # Convert ODB++ to KiCad
            success, message = self.convert_odb_to_kicad(odb_path, kicad_path)
            if not success:
                result.error = message
                if not redacted:
                    print(f"  Warning: {message}")
                else:
                    print("  Warning: conversion failed")
                # Continue with existing file if available
                if not kicad_path.exists():
                    return result

        if not kicad_path.exists():
            result.error = f"KiCad file not found: {kicad_path}"
            return result

        # Parse KiCad output
        if redacted:
            print("Parsing KiCad output: [REDACTED]")
        else:
            print(f"Parsing KiCad output: {kicad_path}")
        try:
            kicad_parser = KiCadPcbParser(str(kicad_path))
            result.kicad_data = kicad_parser.parse()
        except Exception as e:
            result.error = f"KiCad parsing failed: {e}"
            return result

        # Run comparison
        print("Running comparison...")
        comparator = OracleComparator(tolerance=0.1)  # 0.1mm tolerance
        result.comparison_results = comparator.compare(result.oracle_data, result.kicad_data)

        # Determine pass/fail
        failures = [r for r in result.comparison_results if not r.passed]
        result.passed = len(failures) == 0

        # Print results
        print_results(result.comparison_results, verbose=True, redacted=redacted)

        return result

    def run_all_tests(self, redacted: bool = False) -> Dict[str, TestResult]:
        """Run all sample tests"""
        results = {}

        for sample_name in self.samples:
            results[sample_name] = self.run_test(sample_name, redacted=redacted)

        return results

    def generate_report(self, results: Dict[str, TestResult], redacted: bool = False) -> str:
        """Generate a summary report"""
        report = []
        report.append("\n" + "="*60)
        report.append("ORACLE TEST SUMMARY" + (" [REDACTED]" if redacted else ""))
        report.append("="*60)
        report.append(f"Timestamp: {datetime.now().isoformat()}")
        report.append("")

        passed = sum(1 for r in results.values() if r.passed)
        failed = sum(1 for r in results.values() if not r.passed and not r.error)
        errors = sum(1 for r in results.values() if r.error)

        report.append(f"Total: {len(results)} | Passed: {passed} | Failed: {failed} | Errors: {errors}")
        report.append("")

        for name, result in results.items():
            if result.error:
                status = "ERROR"
            elif result.passed:
                status = "PASS"
            else:
                status = "FAIL"

            report.append(f"  [{status}] {name}")
            if result.error:
                if redacted:
                    # Redact file paths and specific details
                    error_type = "file_not_found" if "not found" in result.error else "parse_error"
                    report.append(f"       Error type: {error_type}")
                else:
                    report.append(f"       Error: {result.error}")

            # For failures, show which checks failed (redacted-safe)
            if not result.passed and not result.error and result.comparison_results:
                failed_checks = [r.category for r in result.comparison_results if not r.passed]
                report.append(f"       Failed checks: {', '.join(failed_checks)}")

        report.append("="*60)

        return "\n".join(report)


def main():
    parser = argparse.ArgumentParser(description='Oracle Test Runner for odb2kicad')
    parser.add_argument('--sample', '-s', help='Run specific sample (simple, kitchen-sink, private/name)')
    parser.add_argument('--odb', help='Direct path to ODB++ directory (alternative to --sample)')
    parser.add_argument('--kicad', help='Path to KiCad file to compare against (use with --odb)')
    parser.add_argument('--convert', '-c', action='store_true',
                       help='Run converter (requires build)')
    parser.add_argument('--visual', '-v', action='store_true',
                       help='Generate visual comparison (requires viewer)')
    parser.add_argument('--output', '-o', help='Output report file')
    parser.add_argument('--json', action='store_true', help='Output as JSON')
    parser.add_argument('--redacted', '-r', action='store_true',
                       help='Redact sensitive design data for sharing with AI/others')
    parser.add_argument('--list', '-l', action='store_true',
                       help='List available samples and exit')

    args = parser.parse_args()

    # Find project root
    script_dir = Path(__file__).parent
    project_root = script_dir.parent

    runner = OracleTestRunner(project_root)

    # List samples and exit
    if args.list:
        print("Available samples:")
        for name, config in runner.samples.items():
            print(f"  {name}: {config['description']}")
        print("\nUsage:")
        print("  python run_oracle_tests.py --sample simple")
        print("  python run_oracle_tests.py --odb /path/to/odb --kicad /path/to/file.kicad_pcb")
        sys.exit(0)

    if args.redacted:
        print("=" * 60)
        print("REDACTED MODE: Design data (coordinates, names) will be hidden")
        print("=" * 60)

    # Direct path mode
    if args.odb:
        odb_path = Path(args.odb)
        if not odb_path.exists():
            print(f"Error: ODB++ path not found: {odb_path}")
            sys.exit(1)

        # Determine KiCad path
        if args.kicad:
            kicad_path = Path(args.kicad)
        else:
            # Auto-generate output path
            kicad_path = runner.output_dir / f"{odb_path.name}_converted.kicad_pcb"
            if not kicad_path.exists() and not args.convert:
                print(f"Error: No KiCad file specified and none found at {kicad_path}")
                print("Use --kicad to specify a file, or --convert to generate one")
                sys.exit(1)

        # Run direct comparison
        from compare_oracle import OracleComparator, KiCadPcbParser

        print(f"\n{'='*60}")
        if args.redacted:
            print("Testing: [DIRECT PATH - REDACTED]")
        else:
            print(f"Testing: {odb_path.name}")
        print(f"{'='*60}")

        if args.redacted:
            print("Parsing ODB++ oracle: [REDACTED]")
        else:
            print(f"Parsing ODB++ oracle: {odb_path}")
        oracle = DirectOdbOracle(str(odb_path))
        oracle_data = oracle.parse()

        if args.convert:
            success, msg = runner.convert_odb_to_kicad(odb_path, kicad_path)
            if not success:
                print(f"Conversion failed: {msg if not args.redacted else 'see local output'}")
                sys.exit(1)

        if args.redacted:
            print("Parsing KiCad output: [REDACTED]")
        else:
            print(f"Parsing KiCad output: {kicad_path}")
        kicad_parser = KiCadPcbParser(str(kicad_path))
        kicad_data = kicad_parser.parse()

        print("Running comparison...")
        comparator = OracleComparator(tolerance=0.1)
        comparison_results = comparator.compare(oracle_data, kicad_data)

        print_results(comparison_results, verbose=True, redacted=args.redacted)

        failed = sum(1 for r in comparison_results if not r.passed)
        sys.exit(0 if failed == 0 else 1)

    if args.sample:
        results = {args.sample: runner.run_test(
            args.sample,
            use_existing_kicad=not args.convert,
            redacted=args.redacted
        )}
    else:
        results = runner.run_all_tests(redacted=args.redacted)

    # Generate report
    report = runner.generate_report(results, redacted=args.redacted)
    print(report)

    # Output to file if requested
    if args.output:
        if args.json:
            # Convert to JSON-serializable format
            json_results = {}
            for name, result in results.items():
                json_results[name] = {
                    'passed': result.passed,
                    'error': result.error,
                    'comparison_results': [
                        {
                            'passed': r.passed,
                            'category': r.category,
                            'message': r.message
                        }
                        for r in result.comparison_results
                    ]
                }
            with open(args.output, 'w') as f:
                json.dump(json_results, f, indent=2)
        else:
            with open(args.output, 'w') as f:
                f.write(report)
        print(f"\nReport written to: {args.output}")

    # Exit with appropriate code
    all_passed = all(r.passed or r.error for r in results.values())
    sys.exit(0 if all_passed else 1)


if __name__ == '__main__':
    main()
