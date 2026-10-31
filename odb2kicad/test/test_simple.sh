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

# Stage D checks: silkscreen graphics at board level
grep -q 'gr_line.*F.SilkS' /tmp/test-out.kicad_pcb || { echo "FAIL: no silkscreen gr_lines"; exit 1; }

grep -q ' dnp)' /tmp/test-out.kicad_pcb && { echo "FAIL: dnp on a board without DNP footprints"; exit 1; }

echo "PASS: simple test"

# --- Full-circle arcs (start == end) ---
# ODB++ defines an arc record whose start and end points coincide as a 360 degree circle.
echo "--- Full-circle arc test ---"
FC_DIR=$(mktemp -d)
cp -r "${SAMPLES}/odb-output/." "$FC_DIR"
echo "A 95.0 -95.0 95.0 -95.0 95.0 -94.88 1 P 0 Y" >> "$FC_DIR/steps/pcb/layers/f.silkscreen/features"
echo "A 110.0 -105.0 110.0 -105.0 110.0 -104.0 1 P 0 N" >> "$FC_DIR/steps/pcb/layers/f.cu/features"
"${BUILD_DIR}/odb2kicad" "$FC_DIR" /tmp/test-fullcircle.kicad_pcb
rm -rf "$FC_DIR"

# graphic: one gr_circle, centre + a point on the radius
grep -qF '(gr_circle (center 95 94.88) (end 95 95) (stroke (width 0.15)' /tmp/test-fullcircle.kicad_pcb \
    || { echo "FAIL: full-circle silk arc not written as gr_circle"; exit 1; }
# copper: KiCad has no circular track, so two half arcs through the opposite point
grep -qF '(arc (start 110 105) (mid 109 104) (end 110 103) (width 0.25)' /tmp/test-fullcircle.kicad_pcb \
    || { echo "FAIL: full-circle copper arc: first half missing"; exit 1; }
grep -qF '(arc (start 110 103) (mid 111 104) (end 110 105) (width 0.25)' /tmp/test-fullcircle.kicad_pcb \
    || { echo "FAIL: full-circle copper arc: second half missing"; exit 1; }
# no zero-length arc left anywhere
if grep -E '\((gr_)?arc \(start ([^ ]+) ([^)]+)\) \(mid [^)]+\) \(end \2 \3\)' /tmp/test-fullcircle.kicad_pcb; then
    echo "FAIL: arc with start == end written"; exit 1
fi

echo "PASS: full-circle arc test"
# --- Non-KiCad stackup tests (issue #2), synthetic fixtures built from odb-output ---
echo "--- Stackup test ---"
STACK=$(mktemp -d)
trap 'rm -rf "$STACK"' EXIT

# Copper layer named TOP/BOTTOM (forces type-based mapping), with one bottom trace.
make_stackup() {
    local dir="$1"
    cp -r "${SAMPLES}/odb-output" "$dir"
    local L="$dir/steps/pcb/layers"
    mv "$L/f.cu" "$L/top"
    mv "$L/b.cu" "$L/bottom"
    sed -i 's/^    NAME=F\.CU$/    NAME=TOP/; s/^    NAME=B\.CU$/    NAME=BOTTOM/' "$dir/matrix/matrix"
    printf 'UNITS=MM\nF 1\n$0 r250.0\nL 100.0 -105.0 120.0 -105.0 0 P 0\n' > "$L/bottom/features"
}

add_signal_layer() {  # dir name row context
    printf '\nLAYER {\n    ROW=%s\n    CONTEXT=%s\n    TYPE=SIGNAL\n    NAME=%s\n    POLARITY=POSITIVE\n}\n' \
        "$3" "$4" "$2" >> "$1/matrix/matrix"
    mkdir -p "$1/steps/pcb/layers/$(echo "$2" | tr 'A-Z' 'a-z')"
    printf 'UNITS=MM\nF 1\n$0 r250.0\nL 95.0 -95.0 125.0 -95.0 0 P 0\n' \
        > "$1/steps/pcb/layers/$(echo "$2" | tr 'A-Z' 'a-z')/features"
}

copper_count() { grep -cE '^    \(([0-9]|[12][0-9]|3[01]) "[^"]*\.Cu" signal\)' "$1"; }

# MISC-context SIGNAL layer after the bottom copper must not steal B.Cu.
make_stackup "$STACK/misc"
add_signal_layer "$STACK/misc" DRILL_MAP 99 MISC
"${BUILD_DIR}/odb2kicad" "$STACK/misc" "$STACK/misc.kicad_pcb"
N=$(copper_count "$STACK/misc.kicad_pcb")
[ "$N" -eq 2 ] || { echo "FAIL: MISC signal layer counted as copper ($N copper layers)"; exit 1; }
grep -q 'In1.Cu' "$STACK/misc.kicad_pcb" && { echo "FAIL: MISC stackup emitted In1.Cu"; exit 1; }
grep -q 'segment (start 100 105) (end 120 105).*(layer "B.Cu")' "$STACK/misc.kicad_pcb" \
    || { echo "FAIL: bottom trace not on B.Cu"; exit 1; }

# Three real copper layers must be padded to an even count KiCad accepts.
make_stackup "$STACK/odd"
add_signal_layer "$STACK/odd" MID 6 BOARD
"${BUILD_DIR}/odb2kicad" "$STACK/odd" "$STACK/odd.kicad_pcb"
N=$(copper_count "$STACK/odd.kicad_pcb")
[ "$N" -eq 4 ] || { echo "FAIL: odd stackup emitted $N copper layers, KiCad needs an even count"; exit 1; }
grep -q 'segment (start 95 95) (end 125 95).*(layer "In1.Cu")' "$STACK/odd.kicad_pcb" \
    || { echo "FAIL: inner trace not on In1.Cu"; exit 1; }
grep -q 'segment (start 100 105) (end 120 105).*(layer "B.Cu")' "$STACK/odd.kicad_pcb" \
    || { echo "FAIL: odd stackup bottom trace not on B.Cu"; exit 1; }

echo "PASS: stackup test"

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
# CMP rotation is clockwise (ODB++), KiCad's counter-clockwise: CMP 90 -> (at ... 270). With 270 the
# bornier2 pads land at their library positions (pad 1 rect at -2.54 0); kicad-cli 9.0.9 exports a
# 90 deg footprint as CMP 270.
grep -q '(at 88.3 68.9 270)' /tmp/test-kitchen.kicad_pcb || { echo "FAIL: kitchen-sink P2 position/angle wrong"; exit 1; }
grep -q '(pad "1" thru_hole rect (at -2.54 0 270)' /tmp/test-kitchen.kicad_pcb || { echo "FAIL: kitchen-sink P2 pad 1 not at its footprint-local position"; exit 1; }

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
# mask openings come from the mask layers, so both sides are listed explicitly
grep -qE '"\*\.Cu" ("\*\.Mask"|"F\.Mask" "B\.Mask")' /tmp/test-kitchen.kicad_pcb || { echo "FAIL: kitchen-sink no *.Cu + F/B mask layers"; exit 1; }

# Check board outline (polygon, not rect)
grep -q 'gr_line' /tmp/test-kitchen.kicad_pcb || { echo "FAIL: kitchen-sink no gr_line (board outline)"; exit 1; }

# Stage D: Check copper fill zones on B.Cu
grep -q 'zone.*B.Cu' /tmp/test-kitchen.kicad_pcb || { echo "FAIL: kitchen-sink no zones on B.Cu"; exit 1; }
grep -q 'filled_polygon' /tmp/test-kitchen.kicad_pcb || { echo "FAIL: kitchen-sink no filled polygons in zones"; exit 1; }

# Stage D: Check arcs and circles (the two donut_r Edge.Cuts cutouts are gr_circles, B.Fab has an arc)
grep -q 'gr_arc' /tmp/test-kitchen.kicad_pcb || { echo "FAIL: kitchen-sink no gr_arcs"; exit 1; }
CUTOUTS=$(grep -c 'gr_circle.*Edge.Cuts' /tmp/test-kitchen.kicad_pcb || true)
[ "$CUTOUTS" -eq 2 ] || { echo "FAIL: kitchen-sink expected 2 Edge.Cuts circle cutouts, got $CUTOUTS"; exit 1; }
# kicad-cli 9 writes a circle (R, w) as donut_r(2R + w)x(2R - w/2): 8234.13x8159.13 is R 4.092065, w 0.05
grep -q '(gr_circle (center 224.6 97.7) (end 228.692065 97.7) (stroke (width 0.05)' /tmp/test-kitchen.kicad_pcb || { echo "FAIL: kitchen-sink cutout is not the circle KiCad exported"; exit 1; }

# Stage D: Check silkscreen graphics at board level
grep -q 'gr_line.*F.SilkS' /tmp/test-kitchen.kicad_pcb || { echo "FAIL: kitchen-sink no silkscreen gr_lines"; exit 1; }

# Stage D: Check fab layer graphics
grep -q 'gr_line.*F.Fab' /tmp/test-kitchen.kicad_pcb || { echo "FAIL: kitchen-sink no F.Fab gr_lines"; exit 1; }
grep -q 'gr_line.*B.Fab' /tmp/test-kitchen.kicad_pcb || { echo "FAIL: kitchen-sink no B.Fab gr_lines"; exit 1; }
grep -q 'gr_arc.*B.Fab' /tmp/test-kitchen.kicad_pcb || { echo "FAIL: kitchen-sink no B.Fab gr_arc"; exit 1; }

# Stage D: Check courtyard graphics
grep -q 'F.CrtYd' /tmp/test-kitchen.kicad_pcb || { echo "FAIL: kitchen-sink no F.CrtYd layer"; exit 1; }

# Stage D: Check TH pad drill sizes (D1 = 1.143mm, P2 = 1.524mm)
grep -q 'drill 1.143' /tmp/test-kitchen.kicad_pcb || { echo "FAIL: kitchen-sink no D1 drill size 1.143"; exit 1; }
grep -q 'drill 1.524' /tmp/test-kitchen.kicad_pcb || { echo "FAIL: kitchen-sink no P2 drill size 1.524"; exit 1; }

# Stage D: Board outline should have profile holes (polygon outline, not just rect)
OUTLINE_LINES=$(grep -c 'gr_line.*Edge.Cuts' /tmp/test-kitchen.kicad_pcb || true)
[ "$OUTLINE_LINES" -ge 10 ] || { echo "FAIL: kitchen-sink too few Edge.Cuts lines ($OUTLINE_LINES), expected profile with holes"; exit 1; }

echo "PASS: kitchen-sink test"

# --- DNP test (odb-dnp: KiCad 9 export, R1 has the dnp flag, R2 a 'Config' field 'Rev B; DNP') ---
echo "--- DNP test ---"
"${BUILD_DIR}/odb2kicad" "${SAMPLES}/odb-dnp" /tmp/test-dnp.kicad_pcb
DNP=$(grep -c '(attr smd dnp)' /tmp/test-dnp.kicad_pcb || true)
[ "$DNP" -eq 2 ] || { echo "FAIL: expected 2 dnp footprints (.no_pop and Config), got $DNP"; exit 1; }

echo "PASS: DNP test"

# --- Rotation test (odb-rotation: KiCad 9 export, SOT-23s and 1x03 headers at 0/30/45/90/270 deg,
# top and flipped to the bottom). Footprint and pad angles are KiCad's (CCW); pads stay at their
# library positions in the footprint frame, mirrored in y on the bottom as KiCad stores them. ---
echo "--- Rotation test ---"
"${BUILD_DIR}/odb2kicad" "${SAMPLES}/odb-rotation" /tmp/test-rotation.kicad_pcb
ROT=/tmp/test-rotation.kicad_pcb
grep -q '(at 40 20 90)' $ROT || { echo "FAIL: rotation U3 (top, 90) footprint angle wrong"; exit 1; }
grep -q '(pad "3" smd roundrect (at 0.94 0 90)' $ROT || { echo "FAIL: rotation U3 pad 3 not at its footprint-local position"; exit 1; }
grep -q '(at 30 35 30)' $ROT || { echo "FAIL: rotation U5 (bottom, 30) footprint angle wrong"; exit 1; }
grep -q '(at 40 35 270)' $ROT || { echo "FAIL: rotation U6 (bottom, 270) footprint angle wrong"; exit 1; }
grep -q '(pad "1" smd roundrect (at -0.94 0.95 270)' $ROT || { echo "FAIL: rotation U6 pad 1 not at its footprint-local position"; exit 1; }
grep -q '(at 55 35 45)' $ROT || { echo "FAIL: rotation J2 (bottom, 45) footprint angle wrong"; exit 1; }

echo "PASS: rotation test"

echo ""
echo "=== All smoke tests passed ==="
