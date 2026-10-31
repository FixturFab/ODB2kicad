# Changelog

## 0.2.0 (2026-10-31)

Conversion fixes, checked by converting KiCad 9 ODB++ exports back to KiCad and comparing the result with the original board.

### Pads and holes
- Through-hole pads are typed from their drill hits instead of `.comp_mount_type`.
- NPTH hits become `np_thru_hole` pads sized to the hole.
- Slotted holes keep their oval drill; custom-pad anchors find their openings.
- Custom-shape pads are rebuilt from the pin's copper surface.

### Footprints
- Footprint and pad rotation come from the CMP/TOP and P records (ODB++ rotation is clockwise; KiCad's is counter-clockwise).
- DNP footprints are written as `(attr ... dnp)`, from the CMP `.no_pop` attribute or a DNP token in a `Config` property (see the README).
- Footprints get their courtyard back from the eda/data package outline, so KiCad's courtyard DRC sees it.
- Reference and value fields are written hidden instead of adding extra `fp_text`.

### Copper and nets
- Inner copper layers of KiCad-named stacks (`IN<n>.CU`) are mapped.
- Copper feature nets come from the eda/data FID records.
- Net-less copper surfaces are written as `gr_poly`, not zones.
- `MISC`-context layers typed SIGNAL are no longer taken for copper, and an odd copper stackup gets one empty inner layer so KiCad loads it (#2).

### Outline and graphics
- The board outline is written once.
- `donut_rc` symbols are drawn as the rectangle they describe; Edge.Cuts `donut_r` circles on their stroke centreline.
- KiCad 9's `donut_r` size skew for exported circles is undone (gated on the exporter version in `misc/info`).
- Arcs whose start equals their end are written as circles (copper: two half arcs).
- Flashes and surfaces on non-copper layers (mask, silk, fab, user) are converted.

### Tests, docs, packaging
- Unit tests (`odb2kicad/test/run_unit_tests.sh`, plain g++, 33 cases).
- New samples `samples/odb-dnp` and `samples/odb-rotation` (kicad-cli 9.0.9 exports) with smoke checks.
- The WASM node test builds its zip fixtures with fflate; the WASM smoke test matches the native one.
- README: install from the tarball in the repo (#3), DNP section, current limitations.
- `wasm/dist` rebuilt and `wasm/odb2kicad-wasm.tgz` repacked (emsdk 3.1.50).

## 0.1.0

First release: ODB++ (directory, .tgz, .zip, .tar) to `.kicad_pcb`, native CLI and WebAssembly package.
