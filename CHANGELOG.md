# Changelog

## 0.3.1 (2026-11-06)

Every standard symbol of the ODB++ 8.1 spec (Appendix A) is now drawn, to the spec's own drawings, by both the converter and the round-up's reference.

### Symbols
- Thermals: `thr` copper ends are round; square and rectangle thermals take rounded / chamfered corners (`s_ths...xr|xc`, `rc_ths...xr|xc`); a rectangle thermal's 45 deg gap runs through the corner; open-corner thermals (`s_tho`, `rc_tho`) take the corner square out; line thermals (`s_thr`) are round-ended lines; oval (`o_ths`) and oblong (`oblong_ths`) thermals are drawn.
- `oval_h` is a half oval (square left end) instead of a full oval.
- `moire` (rings and crosshair) is drawn.
- Stencil symbols: `hplate`, `rhplate`, `fhplate`, `radhplate`, `dshape`, `cross`, `dogbone`, `dpack`, with their corner radii.
- Standard symbol names are matched without regard to case (`THR...`, `RECT...`).
- A symbol may now be several overlapping shapes less some cut shapes (`OdbSymbol::outlineAdd` / `outlineCut`).

### Tests
- Unit tests 51 -> 52 (`standard_symbols_spec_shapes`, areas worked by hand); round-up tests 14 -> 15.
- `test/synthetic_odb.py --variant symbols`: a made-up board with one flash of each (turned, mirrored, negative in a plane, on an inner layer); not in `all`, which the PC-lane recipe pins.
- `wasm/dist` rebuilt (emsdk 3.1.50); WASM output is byte-identical to native on every public and made-up board. `wasm/odb2kicad-wasm.tgz` is repacked at 0.3.1.

## 0.3.0 (2026-11-04)

Conversion fixes found by a layer-by-layer round-up against an independent reference drawn straight from the ODB++ files, plus clearer failures.

### Copper, zones and polarity
- Negative-polarity features cut out the surfaces drawn before them; a NEGATIVE matrix layer becomes a plane (the profile less its features) on the net its pins and vias carry.
- Zone holes stay holes; outline and zone contours start at their OB point.
- Overlapping copper surfaces get zone priorities in file order; joined custom pads are marked as net ties.
- Surface and profile arcs (OC records) are parsed.
- Vias come from the matrix drill layers with their START/END span (`blind` when not F.Cu-B.Cu).

### Pads and symbols
- Free copper pads (test points, fiducials, mounting pads) reach the board as board-only footprints.
- Mask and paste openings keep their own size; an opening of another shape becomes a mask- or paste-only pad.
- Standard symbols without a KiCad pad shape (thermals, octagons, chamfered rectangles, ellipses, ...) and user symbols (`symbols/<name>`) are drawn: custom pads on pins, filled polygons elsewhere.
- Mirrored flashes (orient 9) are read clockwise per the spec, counter-clockwise only for KiCad-made jobs.

### Layers and units
- USER.1-USER.9 map to KiCad's User.1-User.9.
- Units are read per file (`UNITS=`, else misc/info, else INCH); a symbol's `I`/`M` suffix overrides.

### WASM package
- A failed conversion returns `{ success: false, code, error }`: `code` is one of `bad-input`, `archive`, `not-odb`, `no-input`, `bad-matrix`, `no-step`, `no-layers`, `crash`, `internal`, and `error` says what to do about it.
- A WASM abort or out-of-memory resets the module (`resetWasmModule()` is exported), so the next conversion starts clean.
- Archives: the ODB++ root is found by its `matrix/matrix` file (files beside the job and `__MACOSX` entries no longer break it); tar long names (GNU `L`, pax `path=`) are read.
- `wasm/dist` rebuilt (emsdk 3.1.50; byte-identical to a fresh build of this tree) and `wasm/odb2kicad-wasm.tgz` repacked.

### Tests and docs
- Round-up harness (`test/roundup.py`, made-up boards in `test/synthetic_odb.py`, `test/test_roundup.py`).
- Unit tests 33 -> 51; WASM node test 13 cases (failure codes).
- `docs/rotation.md`: why the CMP rotation is clockwise, checked on public samples.
- `what-doesnt-work.md` updated, with a DRC triage of an imported board.

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
