#!/bin/bash
set -e
BUILD_DIR="$(dirname "$0")/../build"
SAMPLES="$(dirname "$0")/../../samples"

echo "=== Running odb2kicad smoke tests ==="

"${BUILD_DIR}/odb2kicad" "${SAMPLES}/odb-output" /tmp/test-out.kicad_pcb

# Check file is valid s-expression (basic bracket matching)
OPEN=$(grep -o '(' /tmp/test-out.kicad_pcb | wc -l)
CLOSE=$(grep -o ')' /tmp/test-out.kicad_pcb | wc -l)
[ "$OPEN" -eq "$CLOSE" ] || { echo "FAIL: mismatched brackets ($OPEN open vs $CLOSE close)"; exit 1; }

# Check key content
grep -q 'kicad_pcb' /tmp/test-out.kicad_pcb || { echo "FAIL: no kicad_pcb header"; exit 1; }
grep -q '"GND"' /tmp/test-out.kicad_pcb || { echo "FAIL: no GND net"; exit 1; }
grep -q '"VCC"' /tmp/test-out.kicad_pcb || { echo "FAIL: no VCC net"; exit 1; }
grep -q '"SIG"' /tmp/test-out.kicad_pcb || { echo "FAIL: no SIG net"; exit 1; }
grep -q '"R1"' /tmp/test-out.kicad_pcb || { echo "FAIL: no R1 component"; exit 1; }
grep -q '"R2"' /tmp/test-out.kicad_pcb || { echo "FAIL: no R2 component"; exit 1; }
grep -q 'segment' /tmp/test-out.kicad_pcb || { echo "FAIL: no segments"; exit 1; }
grep -q 'Edge.Cuts' /tmp/test-out.kicad_pcb || { echo "FAIL: no Edge.Cuts"; exit 1; }
grep -q 'roundrect' /tmp/test-out.kicad_pcb || { echo "FAIL: no roundrect pads"; exit 1; }
grep -q 'roundrect_rratio' /tmp/test-out.kicad_pcb || { echo "FAIL: no roundrect_rratio"; exit 1; }
grep -q 'gr_rect' /tmp/test-out.kicad_pcb || { echo "FAIL: no gr_rect (board outline)"; exit 1; }
grep -q 'F.Cu' /tmp/test-out.kicad_pcb || { echo "FAIL: no F.Cu layer"; exit 1; }
grep -q 'B.Cu' /tmp/test-out.kicad_pcb || { echo "FAIL: no B.Cu layer"; exit 1; }

# Check specific values
grep -q '(at 100 100)' /tmp/test-out.kicad_pcb || { echo "FAIL: R1 not at (100,100)"; exit 1; }
grep -q '(at 120 100)' /tmp/test-out.kicad_pcb || { echo "FAIL: R2 not at (120,100)"; exit 1; }
grep -q '(size 1.025 1.4)' /tmp/test-out.kicad_pcb || { echo "FAIL: wrong pad size"; exit 1; }
grep -q '(start 90 90)' /tmp/test-out.kicad_pcb || { echo "FAIL: wrong board outline start"; exit 1; }
grep -q '(end 130 110)' /tmp/test-out.kicad_pcb || { echo "FAIL: wrong board outline end"; exit 1; }
grep -q '"10k"' /tmp/test-out.kicad_pcb || { echo "FAIL: no R1 value 10k"; exit 1; }
grep -q '"4.7k"' /tmp/test-out.kicad_pcb || { echo "FAIL: no R2 value 4.7k"; exit 1; }

echo "PASS: all smoke tests passed"
