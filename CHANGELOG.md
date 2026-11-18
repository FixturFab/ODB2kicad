# Changelog

## 0.4.0 (2026-11-16)

Closes the gaps between the converter and the ODB++ reader spec (v1 sections 4.1-4.8 and v2), checked on the ODB++ Viewer's own demo job, now a fixture. Highlights:
- Reads what it used to miss: compressed `.z` files, the step origin, every step of a job, STEP-REPEAT panels (placed as one board), every component side, the full matrix, text drawn from the job's font, pin outlines, drill tool sizes, layer attributes and the stackup's board thickness, netlist via points.
- Writes per-layer via and through-hole pad sizes as KiCad 9 padstacks; a board that needs them is written as KiCad 9, every other board stays KiCad 8, and `--kicad 8|9` forces one.
- Converts large boards about 4x faster (demo-design: 17 s to 4.5 s native, 26 s to 7.4 s in WASM).
- The oracle's outline and pin 1 checks now test the board: all 8 checks pass on all three samples.

Gates on the composed branch: 83/83 unit tests, `test_simple.sh` passes, oracle 3/3 samples (8/8 checks each), round-up 15 jobs with 0 failed (25 harness tests), oracle self-tests 7/7, WASM node test 16/16, and WASM output is byte-identical to native on all 15 boards.

### Oracle suite: outline and pin 1 checks
- `bounding_box` compares the step's profile extent with the extent of Edge.Cuts (board and footprint items; arcs and circles with their bulge), edge by edge at the run's tolerance, instead of every layer's features against footprint origins and track ends. The stephdr origin is applied to the oracle's profile and components, as the converter does.
- `pin1_positions` places each footprint's pin 1 pad by the footprint's angle and position and compares it with the ODB++ TOP record; pin 1 is the terminal named `1`, else the part's first terminal (demo-design names pins `C10-1`, so the old check compared none of its parts while reporting all 851). A misplaced pin 1 reports how far off and by how many degrees turned.
- `test/run_oracle_tests.py` converts every registered sample by default (`--stored` compares `samples/test.kicad_pcb` for `simple`), and an ERROR (not converted, not compared) now fails the exit code. `test/test_compare_oracle.py` breaks boards on purpose to show both checks fail.

### Faster keyhole cuts (demo-design converts in about 4 s, was 17 s)
- Folding a copper area's holes into its outline (KiCad's `gr_poly` has no holes) tried every outline point against every hole point, the outline growing with each hole spliced in: on demo-design one plane island has 3666 holes (a 98726-point outline) and the fold took 11 of the 15-17 s. It now finds each hole's closest pair through a grid of the outline's points, with the same tie rule (earliest outline point, then earliest hole point) and the same winding test, so every board is byte-identical: all six samples, `--kicad 8` and `9`, native and WASM. Native demo-design: 17.1 s to 4.5 s (keyhole 11 s to 0.07 s); WASM (Node): 26.1 s to 7.4 s. Small islands keep the pair-by-pair search, which stays as `keyholeReference()` for the unit test that compares the two.

### Per-layer via and through-hole pad sizes (KiCad 9 padstacks)
- A via's pad is sized per copper layer from that layer's flash, and a plated through-hole pad's inner layers likewise (component pins and free holes), written as KiCad 9 padstacks (`front_inner_back` when the inner layers agree, else `custom`). A board that needs one is written as KiCad 9 (`version 20241229`, KiCad 9 layer ids, default plot layer selection); every other board stays KiCad 8 as before. `--kicad 8|9` (library: `ConvertOptions::kicadFormat`) forces one.
- A negative plane's flash is the plane's clearance or thermal, not copper: it no longer sizes the via (demo-design's 43 mil "outer" via size was its PG5 / PG6 anti-pad; its vias are 23 mil, 33 on one outer side for some). On such a plane, and where the job has no flash, the via / pad is the drill's size (no ring). Also in KiCad 8 output, the via is now the largest *copper* flash.
- A via with no pad of its own on some layer (the CAD removed it: KiCad's "remove unused layers", a negative plane's clearance) is written with KiCad 9 `remove_unused_layers`, `keep_end_layers` when both ends have a ring, and `zone_layer_connections` for the inner layers that do, so KiCad flashes it only where the job has copper. KiCad's annular-width check reads the padstack size even on a layer it does not flash, so such a layer takes the via's smallest real ring instead of a 0 mm one (kicad-demo-vme-wren had ~4300 vias with 0 mm rings, DRC 1366 -> 1563 errors; now 1365). The round-up renderer flashes such a via the same way.
- The round-up's KiCad renderer (`test/kicad_render.py`) reads padstacks and orders copper layers by name (KiCad 9 numbers B.Cu 2).

Gap closure against the ODB++ reader spec (reference behaviour: ODB++ Viewer 2510), checked on the new `samples/demo-design` fixture (the viewer's own demo job).

### Reader spec v2
The second version of the reader spec changed these rules; each is applied on top of the items below.
- Arcs: the direction token of an `A` record is optional (the reference reader does not read one). It is honoured when written; an arc without it runs counter-clockwise, its centre on the left of the way from start to end, and the report counts such arcs. The polarity and dcode may be missing too.
- Fonts: a font's own `UNITS` line (`UNITS=MM`, default INCH, whatever the layer files use), square-ended lines (`S`), and filled `POLYGON` glyphs (drawn as polygons on the text's layer) are read. A font line that cannot be read is skipped and reported the reference's way ("error while reading font file - fonts/<name>, line - <n>").
- Text placement is the spec's glyph convention, as its reference renderer draws it: glyph coordinates as the font writes them with the glyph's own zero on the record's location, one xsize on per character, `V` text one ysize on along +y (it ran downwards), stroke width = `weight` x the font's line width. Texts move by a fraction of a character against the first reading (demo-design's `FS` / `SMT` / `IR`: 3.36 mm left and down).
- Board thickness: an explicit `.board_thickness` (step attrlist, then `misc/attrlist`, then a layer's) is the authority, ahead of the stackup sum, the drill table and the default. The rigid-flex sample now reads 1.3904 mm (it says 0.0547402 in; its drill tables say 1.390) instead of the 1.395 summed from its rows. `steps/<step>/attrlist` is read.
- Drill tools: `TYPE` `COMPANY` / `CCE` and `TYPE2` `STANDARD` / `SMOOTH` / `ROUGH` are kept as written, `BIT` with its sign. `COLOR` stays informational: nothing reads it.
- Reference rendering (spec section 5.5): the spec's `tools/odb_render_text.py` is ported as `test/odb_render_text.py` and is what the round-up's reference draws text with; `tools/render_odb_sheet.py` renders any job, step or layer to PNG from the same code, and with `--kicad <board>` compares a converted board with it on a view grown to the layer's content (the round-up's view is the board outline, which leaves demo-design's process-map text out): the converter's `FS` / `SMT` / `IR` match the reference to the pixel. The per-sample results are in `what-doesnt-work.md`. Two things in the shipped tool are not followed (see `what-doesnt-work.md`): it reads a text record's version field as a mirror flag, and mirrors letter by letter.

### Compressed files
- Files stored with UNIX compress are read: ODB++ lets any file of a job be kept as `<name>.z` (`features.z`, `components.z`, `eda/data.z`, ...), and the converter used to treat them as missing. demo-design ships its `eda/data`, both `components` files and seven layers that way, so it used to convert with no components (1621 anonymous pads), no eda nets and no bottom copper, mask, paste or silkscreen; it now has its 853 components on both sides and every layer. The plain file wins when both exist. In the WASM package such files must be passed as bytes (`content: Uint8Array`); `convertOdbArchive` does so itself.

### Steps
- Jobs with several steps: every `steps/*` folder is read as a design of its own (the matrix's STEP records by `COL`, matched to their folders without regard to case, then folders the matrix does not name). The CLI writes the first step to the output file and every other step beside it as `<job>__<step>.kicad_pcb`; `--step <name>` converts one step. To stdout it writes the first step and names the others. A step with neither layer data nor STEP-REPEAT blocks is skipped with a warning. One-step jobs convert as before, byte for byte.
- Panels: a step whose `stephdr` places other steps with `STEP-REPEAT` blocks (`NAME`, `X`, `Y`, `DX`, `DY`, `NX`, `NY`, `ANGLE`, `MIRROR`, `FLIP`) is converted as one KiCad board holding the panel's own features plus a copy of the named step at each of the NX x NY positions: the step's datum goes to (`X` + i·`DX`, `Y` + j·`DY`), and the copy is turned `ANGLE` degrees clockwise about it. Each copy's nets are its own, named `Board_<n>-<net>` (n counts the copies across the panel's blocks, 1 up, as KiKit names a panel's nets); references stay as they are. When the panel has a profile it is the board edge and each copy's outline is drawn on `Dwgs.User` (its rout line); a panel without a profile is the copies, each with its own outline on `Edge.Cuts`. A panel with no layer data of its own (only placements) is converted, not skipped. A panel placed in a panel places its copies again (`Board_1-Board_4-<net>`). Copies with `MIRROR` or `FLIP` set, a step that places itself and a `NAME` the job has not got are left out with a warning; the placed step's own warnings are repeated once, tagged `[placed step <name>]`. `--no-panels` (library: `ConvertOptions::placeSteps = false`) converts a panel without its placed steps, as before. Jobs without STEP-REPEAT convert byte for byte as before.
- When the matrix names several steps the first one is now the lowest `COL` (it was the last STEP record in the file).
- Library: `convert(path, ConvertOptions{step, allSteps})`, `ConvertResult::stepNames` / `steps`, `parseOdbJob`, `parseOdbStep`, `listOdbSteps`. WASM package: `convertOdb(files, { step })`, and results carry `step`, `steps` and `report`; the npm CLI takes `--step`.
- Components are read from every COMPONENT row of the matrix (`comp_+_left`, `comp_+_right`, ... on rigid-flex jobs), not only `comp_+_top` / `comp_+_bot`: a row above the first copper row mounts on the top side, one below it on the bottom. Layer folders are matched to matrix names without regard to case.
- The step header's origin is applied: every coordinate of a step is relative to `X_ORIGIN` / `Y_ORIGIN` of its `stephdr` (in the stephdr's own units), so the profile, layer features, component placements and netlist points move by it. The datum is a reference point and moves nothing. Jobs with a zero origin (every sample so far) convert as before.

### Matrix, version and the report
- The matrix is captured in full: `COL` and every STEP record, `OLD_NAME`, `COLOR` and `ID` per layer, and rows keep their numbers (a skipped row is legal and is not closed up). A layer `TYPE` outside the standard thirteen is drawn as a DOCUMENT layer and a drill span that names no copper row is drilled through, each with a warning.
- `misc/info`: `ODB_VERSION_MAJOR` / `MINOR`, `CREATION_DATE` and `SAVE_DATE` are kept, with every other key; a job without `JOB_NAME` takes it from `misc/job_name`. Every board says what made it and from what: `(generator "odb2kicad/0.4.0 odb++:v8.1")` (no version suffix when the job gives none; no dates, so two runs are byte-identical). A job older than version 8 is read as 8, with a warning.
- The report (`--report`, `ConvertResult::report`, WASM `report`) lists the ODB++ version, the matrix with its row gaps, and every matrix row with the KiCad layer it is drawn on or why it is not.
- An ODB++ XML (ODX) matrix is refused with `bad-matrix` instead of being mis-read.

### Text and document layers
- Text records (`T`, and `V` drawn upwards) are converted; they used to be counted and dropped. Each character is stroked from the job's vector font (`fonts/<name>`: `XSIZE` / `YSIZE` / `OFFSET`, `CHAR ... LINE ... ECHAR`), scaled by xsize / XSIZE and ysize / YSIZE, one xsize apart from the record's location, mirrored and turned clockwise as the record says, `weight` font line widths wide (12 mil each in the standard font). The strokes join the layer's line work, so they are `gr_line`s on graphic layers, and a negative text clears the copper drawn before it. `$$JOB`, `$$STEP` and `$$LAYER` are filled in. A text whose font the job lacks, characters the font lacks and barcode records are reported, not drawn.
- DOCUMENT layers that no KiCad layer answers to by name (assembly drawings, process maps, notes) are drawn on the `User.1` .. `User.9` layers the job leaves free, in matrix row order, under their own name (`(50 "User.1" user "PROCMAP_BOT")`). A layer holding over 100000 features is left out with a warning (`--document-layers all|none|N`; WASM `{ documentLayers }`). KiCad-made jobs are not affected: their other DOCUMENT rows are kicad-cli's tenting / plugging / filling layers. demo-design's process maps and their `FS` / `SMT` / `IR` labels now reach the board; the rigid-flex sample gains its bottom assembly drawing, DRC notes and outline copy (17.6 -> 23.6 MB), its 370000-feature top assembly drawing stays out by default.

### Pads
- Pin outlines are read from `eda/data` (the `CT ... CE`, `RC`, `CR` and `SQ` records after a `PIN`; they were skipped). A pin with no copper flash and no copper surface of its own takes its pad shape from its outline only when that outline is copper: a surface-mount pin on a net, with no paste or mask opening on either side, whose outline is covered by copper of its own net on its side (the tracks, arcs or surfaces a CAD drew the land with). Then a rectangle centred on the pin is a rect pad, anything else a custom pad. A pin with a flash keeps the flash. Paste-only and mask-only pads (KiCad lists them as pins with an outline and no copper), pins on no net, and pins only partly over copper (the rigid-flex sample's key domes are copper rings under a square outline) keep the 0.1 mm placeholder and their opening stays a paste- or mask-only pad on its own side, as in 0.3.1 (the first rule turned those openings into copper pads on 21 of 36 public KiCad boards). On the 44-board shootout no board's output changes from 0.3.1 through this rule.

### Board thickness and layer attributes
- Layer `attrlist` files are read (`key = value`, every key kept; empty and missing files are fine), and `misc/attrlist`.
- The board thickness is no longer a fixed 1.6 mm. It is the stackup the attrlists describe when a DIELECTRIC row carries `.layer_dielectric`: those rows, plus the copper rows (`.copper_weight` in oz at 34.8 um, else the row's own `.layer_dielectric`) and the solder mask rows, to the micron. Without that it is the largest `THICKNESS` of a drill `tools` file, and only then 1.6. KiCad exports still read 1.6; the rigid-flex sample reads 1.395 mm (its drill tables say 1.390; its dielectric rows alone are 1.035) and demo-design 1.5748 mm (62 mil, from its drill table).
- A layer's output transform (`.out_x_scale`, `.out_y_scale`, `.out_mirror`, `.out_angle`) is applied to that layer's features when it is not the identity: scale about the step's zero, mirror in X, turn clockwise. An uneven scale moves features and leaves symbol sizes alone.
- `odb2kicad --report` prints what the reader chose and assumed (which thickness path, transformed layers); `ConvertResult::report` carries the same lines.

### Drills
- Drill sizes come from the drill layer's `tools` file (`layers/<drill>/tools`): a hit's tool is found by its dcode (the tool `NUM`), its `.bit` string, a `TOOL<n>` string or its symbol's size, and the hole is the tool's `DRILL_SIZE` (thousandths of the file's unit, the file's own `UNITS` line first). The flash symbol is used only when the table names no tool for the hit, a slot keeps its oval symbol, and a tool more than 10% off the symbol is ignored. demo-design's 3759 vias are 0.4572 mm (`DRILL_SIZE=18`); the rigid-flex sample's holes take the table's values (0.900001 for 0.899998).
- A hit with no `.drill` / `.pad_usage` attribute takes plated / non-plated / via from its tool's `TYPE`.

### Nets
- Netlist points are read field by field (`<net> <radius> <x> <y> <side> [<w> <h>] <epoint> <exp> ... [v]`). The letters after the side used to be read as numbers, which dropped the rest of the line and the trailing `v`, so no point was ever marked a via. A via without an FID record now takes the net of the `v` point at its place. The radius is scaled with the file's units.
- The netlist is `netlists/cadnet`, else the first other `netlists/<source>`.

### Build
- The native CMake build defaults to `Release`: demo-design converts in about 15 s instead of two minutes. The output is the same at every optimization level.

### Tests and docs
- Unit tests 52 -> 83, round-up tests 15 -> 25, WASM node test 13 -> 16 cases (multi-step, demo-design parity from a folder and a .tgz), a demo-design section in `test_simple.sh`.
- The oracle's rotation check compares angles instead of crashing on a turned part, its component count leaves out the converter's board-only footprints, and `--sample demo-design --convert` is a registered sample (all 8 checks pass since the outline and pin 1 checks were rewritten, above).
- The round-up harness is 0.2.0: its reference applies the step origin and layer output transforms, strokes text from the font, maps document layers, reads `.z` files and picks the first step by COL.
- `wasm/dist` rebuilt (emsdk 3.1.50) with every C++ change; WASM output is byte-identical to native on every public and made-up board. `wasm/odb2kicad-wasm.tgz` is repacked at 0.4.0.
- `samples/demo-design`: the 12-layer INCH demo job shipped with the ODB++ Viewer 2510 documentation.
- `test/synthetic_odb.py --variant origin`, `--variant stackup` and `--variant specv2` (a millimetre font with filled glyphs, V text, arcs without a direction); the round-up's reference applies the step origin and the layer output transforms, strokes text from the font and maps document layers the same way.

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
