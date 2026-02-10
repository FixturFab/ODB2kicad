#!/bin/bash
set -e
BUILD_DIR="$(dirname "$0")/../build"
SAMPLES="$(dirname "$0")/../../samples"

echo "=== Running odb2kicad smoke tests ==="

# --- Simple test (odb-output) ---
echo "--- Simple test ---"
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

# Check segment net assignment
grep -q 'net 2' /tmp/test-out.kicad_pcb || { echo "FAIL: segment net should be SIG (net 2)"; exit 1; }

echo "PASS: simple test"

# --- Kitchen-sink test (odb-kitchen-sink) ---
echo "--- Kitchen-sink test ---"
"${BUILD_DIR}/odb2kicad" "${SAMPLES}/odb-kitchen-sink" /tmp/test-kitchen.kicad_pcb

# Check valid s-expression
OPEN=$(grep -o '(' /tmp/test-kitchen.kicad_pcb | wc -l)
CLOSE=$(grep -o ')' /tmp/test-kitchen.kicad_pcb | wc -l)
[ "$OPEN" -eq "$CLOSE" ] || { echo "FAIL: kitchen-sink mismatched brackets ($OPEN open vs $CLOSE close)"; exit 1; }

# Check key content
grep -q 'kicad_pcb' /tmp/test-kitchen.kicad_pcb || { echo "FAIL: kitchen-sink no kicad_pcb header"; exit 1; }
grep -q '"A"' /tmp/test-kitchen.kicad_pcb || { echo "FAIL: kitchen-sink no net A"; exit 1; }

# Check components
grep -q '"R1"' /tmp/test-kitchen.kicad_pcb || { echo "FAIL: kitchen-sink no R1"; exit 1; }
grep -q '"D1"' /tmp/test-kitchen.kicad_pcb || { echo "FAIL: kitchen-sink no D1"; exit 1; }
grep -q '"P2"' /tmp/test-kitchen.kicad_pcb || { echo "FAIL: kitchen-sink no P2"; exit 1; }

# Check component positions
grep -q '(at 125.2 90.9)' /tmp/test-kitchen.kicad_pcb || { echo "FAIL: kitchen-sink R1 position wrong"; exit 1; }
grep -q '(at 105.85 65.35)' /tmp/test-kitchen.kicad_pcb || { echo "FAIL: kitchen-sink D1 position wrong"; exit 1; }
grep -q '(at 88.3 68.9 90)' /tmp/test-kitchen.kicad_pcb || { echo "FAIL: kitchen-sink P2 position/angle wrong"; exit 1; }

# Check vias
VIA_COUNT=$(grep -c '^  (via' /tmp/test-kitchen.kicad_pcb || true)
[ "$VIA_COUNT" -eq 3 ] || { echo "FAIL: kitchen-sink expected 3 vias, got $VIA_COUNT"; exit 1; }
grep -q '(via (at 120 102)' /tmp/test-kitchen.kicad_pcb || { echo "FAIL: kitchen-sink via at (120,102) missing"; exit 1; }
grep -q '(via (at 118 102)' /tmp/test-kitchen.kicad_pcb || { echo "FAIL: kitchen-sink via at (118,102) missing"; exit 1; }
grep -q '(via (at 120 103.5)' /tmp/test-kitchen.kicad_pcb || { echo "FAIL: kitchen-sink via at (120,103.5) missing"; exit 1; }
grep -q '(drill 0.3)' /tmp/test-kitchen.kicad_pcb || { echo "FAIL: kitchen-sink via drill size wrong"; exit 1; }

# Check traces on B.Cu
grep -q '(layer "B.Cu")' /tmp/test-kitchen.kicad_pcb || { echo "FAIL: kitchen-sink no B.Cu traces"; exit 1; }

# Check trace net assignment (should be net 1 "A")
SEGMENT_LINES=$(grep 'segment' /tmp/test-kitchen.kicad_pcb)
if echo "$SEGMENT_LINES" | grep -q 'net 0'; then
    echo "FAIL: kitchen-sink has traces with net 0 (unassigned)"
    exit 1
fi

# Check through-hole pads
grep -q 'thru_hole' /tmp/test-kitchen.kicad_pcb || { echo "FAIL: kitchen-sink no through-hole pads"; exit 1; }
grep -qF '"*.Cu" "*.Mask"' /tmp/test-kitchen.kicad_pcb || { echo "FAIL: kitchen-sink no *.Cu *.Mask layers"; exit 1; }

# Check board outline (polygon, not rect)
grep -q 'gr_line' /tmp/test-kitchen.kicad_pcb || { echo "FAIL: kitchen-sink no gr_line (board outline)"; exit 1; }

echo "PASS: kitchen-sink test"

echo ""
echo "=== All smoke tests passed ==="
