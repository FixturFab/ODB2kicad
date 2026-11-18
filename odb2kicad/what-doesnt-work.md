# What Doesn't Work / Known Limitations

Measured by round-tripping KiCad boards (KiCad board -> kicad-cli 9 ODB++ export -> odb2kicad ->
KiCad) and diffing the result against the original, layer by layer.

## Import Quality
- **Text is strokes**: KiCad exports text as individual line strokes (or filled glyph surfaces); other tools write `T` records, which are stroked from the job's vector font (`fonts/<name>`). Either way the board gets gr_line / gr_poly, not editable text objects. Footprint Reference/Value fields are written hidden; their original position is not in the ODB++.
- **T text placement follows the format's glyph convention**: glyph coordinates are used as the font writes them, the glyph's own zero on the record's location (the standard font draws its 0.2 x 0.3 characters from (-0.1, -0.1) to (0.1, 0.2), so a text stands a third of a character under and half a character left of where a lower-left-corner reading puts it); characters advance one xsize, `V` records one ysize along +y; a stroke is the record's width factor times the font's line width (12 mil); the font's `OFFSET` is kept and not applied (0 in every font seen). Two points where a simpler reading of the format goes wrong: the record's last field is its version (0 / 1), not a mirror flag (read as one, `'FS' 1` is drawn with every letter flipped), and a mirrored text (orient 4-7 / 9) is mirrored as a whole about its location, not letter by letter. Neither has been checked in a commercial viewer. Square-ended font lines (`S`) are drawn with KiCad's round-ended `gr_line`; filled `POLYGON` glyphs are `gr_poly` (a zone on copper). A text whose font the job lacks, characters the font lacks, and barcode (`B`) records are reported and not drawn; `$$JOB`, `$$STEP` and `$$LAYER` are filled in, other `$$` variables (dates, counters) are drawn as written.
- **Arcs without a direction**: an `A` record's trailing `Y|N` is honoured when written (every producer seen writes it); without it the arc runs counter-clockwise from start to end (the centre on the left of travel), and the report counts such arcs. No real job without the token has been seen.
- **Zone outlines are the fill**: ODB++ only exports filled copper polygons, not the zone's drawn outline, its priority or its rules. Each imported zone's outline is its fill, priority 0. A KiCad zone with several islands comes back as one zone per island (the eda/data SNT PLN record groups them, but a KiCad zone file holds a single outline).
- **Zone net assignment**: nets come from the eda/data FID records. Without eda/data, a zone takes the net of a pad that lies in its fill (not in a clearance hole), else net 0.
- **Free pads are footprints**: a copper flash that is neither a component pin nor a via (test point, fiducial, mounting pad) becomes a board-only footprint (`odb2kicad:FreePad`, reference FID/TP/PAD/H<n>) with one pad. KiCad has no SMD pads on inner layers, so an inner-layer free flash without a hole is a filled copper graphic with no net.
- **Mask / paste openings**: a pad's opening that is its own shape grown evenly is a `solder_mask_margin` / `solder_paste_margin`; any other opening (another shape, aspect ratio or corner radius, a custom symbol) is a mask- or paste-only pad of its own shape on the same pin; further flashes at the pin and a via's opening (untented via) are board graphics on the mask layer.
- **Symbols without a KiCad pad shape** (thermals, octagons, chamfered or part-rounded rectangles, ellipses, diamonds, hexagons, triangles, butterflies, half ovals, square / oval donuts, moires, the stencil symbols and user symbols): a pin or free flash of one is a KiCad custom pad (0.01 mm anchor, the outline as primitives); elsewhere it is a filled polygon. Every ODB++ 8.1 standard symbol is drawn. Read from the spec's drawings where its text is silent, not yet checked on a real board: `oblong_ths`'s `r|s` is taken as round / straight copper ends; `cross` / `dogbone` cross points are percent of the width / height; `ra` / `ro` on the stencil symbols round corners of up to / over 90 deg; `radhplate`'s and `dshape`'s `ra` rounds the four plain corners only (not where an arc meets a side), and `dogbone`'s `ra` only its inside corners. A user symbol is its positive features less its negative ones, in no particular order.
- **Custom pad shapes**: rebuilt from the pin's copper surface (SNT TOP); the anchor shape is not exported, so it is a 0.01 mm circle inside the primitives.
- **Negative polarity**: a negative feature is cut out of the surfaces drawn before it on its layer (zones, copper and other polygons), and a NEGATIVE matrix layer becomes a plane: the profile less its features, one zone per island, on the net most through-hole pins and vias in its copper carry. Not done: a negative feature over a trace, pad or line drawn before it leaves that copper whole (KiCad has no way to clip a track), a negative feature on a NEGATIVE layer (copper again) is not drawn, and the clearances live only in the zone fill: KiCad's refill does not keep a void it has no rule for.
- **Footprint courtyards need courtyard strokes**: a footprint's courtyard is rebuilt from the F/B.Courtyard strokes its eda/data package outline runs along (90% of the outline's length). A package outline with no strokes under it draws nothing: kicad-cli writes a footprint's bounding hull there when it has no courtyard, Allegro the place-bound, Altium the body, and those are not courtyards. Allegro and Altium package outlines are not imported.

## Layer Support
- **User layers**: USER.1 through USER.9 (kicad-cli's names) map to KiCad's User.1-User.9. Other producers' DOCUMENT layers (assembly drawings, process maps, notes, board outline copies) have no KiCad layer by name: they are drawn on the User.1-User.9 layers the job leaves free, in matrix row order, under their own name. Not drawn: a tenth such layer, a layer holding over 100000 features (`--document-layers all` draws it; the rigid-flex sample's top assembly drawing is 370000 features and 50 MB of board file), and ROUT / MASK / CONDUCTIVE_PASTE rows. `--report` lists every matrix row with where it went.
- **Tenting/filling/capping/plugging layers**: These ODB++ manufacturing layers have no KiCad equivalent and are skipped.
- **Blind/buried vias**: a via spans its drill layer's START/END_NAME and is written `blind` when that is not F.Cu-B.Cu (KiCad's token for blind and buried); micro vias are written as blind. Via and plated through-hole pad sizes are per copper layer (KiCad 9 padstacks); the file is KiCad 9 only when a board needs it. Left: a through-hole pad's far outer side keeps the component side's shape (its mask margin is fitted to that shape), a layer flash KiCad has no pad shape for (thermal, user symbol) or turned by other than a quarter keeps the pad's own shape, and `--kicad 8` still writes one size (the largest copper flash). A negative plane's flash is a clearance, so there the via / pad is the drill's size: KiCad's DRC reports a zero annular ring on those layers.

## Steps, stackup and drill tables
- **Step origin**: every coordinate of a step is moved by its stephdr `X_ORIGIN` / `Y_ORIGIN` (x' = x + X_ORIGIN). Every job seen so far has a zero origin, so the sign is checked on made-up boards only. `X_DATUM` / `Y_DATUM` are not applied.
- **Multi-step jobs**: each step is converted on its own (CLI: the first to the output file, the others beside it as `<job>__<step>.kicad_pcb`). A step with no layer data and no STEP-REPEAT blocks is skipped and reported. Matrix columns beyond the step list (COL > 1 panel matrices) are not interpreted.
- **Panels (STEP-REPEAT)**: a panel step is one board holding a copy of each placed step, nets `Board_<n>-<net>`, references unchanged (so a panel has duplicate references, as KiKit panels do). Not done: copies with `MIRROR` or `FLIP` (a bottom-side-up step needs every footprint flipped and its layers swapped; they are left out with a warning); routing tabs, mouse bites and v-scores (ODB++ draws them on rout / document layers, which convert as drawings, not as KiCad panel features); the panel's own `TOP_ACTIVE` / `BOTTOM_ACTIVE` areas and step-level attributes. Checked on made-up panels only: no real panel job has been converted yet.
- **Components on other sides**: COMPONENT rows other than comp_+_top / comp_+_bot (comp_+_left / comp_+_right on rigid-flex jobs) are placed on the top side when their row is above the first copper row, else on the bottom. KiCad has two sides only; eda/data's SNT TOP records name sides T and B, so custom pads from pin surfaces are rebuilt for those two only. No real job with such rows has been seen.
- **Board thickness**: an explicit `.board_thickness` (the step's attrlist, then `misc/attrlist`, then a layer's; in that file's units), else the stackup the layer attrlists describe (dielectric rows' `.layer_dielectric`, copper by `.copper_weight` at 34.8 um per oz or the row's own `.layer_dielectric`, solder mask rows), else a drill table's `THICKNESS`, else 1.6 mm. The rigid-flex sample says `.board_thickness=0.0547402` in (1.3904 mm, its drill tables' 1.390). Counting the dielectric rows alone gives 1.035 mm on the rigid-flex sample, so copper and mask are added (1.395 mm). Coverlay, stiffener and other MASK rows are not counted, and no KiCad stackup section is written.
- **Layer output transforms**: `.out_x_scale` / `.out_y_scale` / `.out_mirror` / `.out_angle` of a layer's attrlist are applied when they are not the identity: scale about the step's zero, mirror in X, turn clockwise. Every job seen carries the identity, so order and turning sense are from the spec's wording only; an uneven scale moves features and leaves symbol sizes alone. `.out_polarity` and `.out_comp` are not applied.
- **Drill tools**: a hole's size is its tool's `DRILL_SIZE` (the tool, not `FINISH_SIZE`: KiCad's drill is the finished hole on a plated pad, so a plated hole reads larger than it will end up by the plating). `BIT` (kept as written, sign and all), the tolerances and `TYPE2` are read and not used; a `COMPANY` / `CCE` tool's hits are plated holes unless the hit says otherwise; a slot keeps its oval symbol.
- **Pin outlines**: used only for a pin with no copper flash and no copper surface of its own. A rounded land comes out as a custom pad with flattened corners, not a roundrect; a pin outline is never checked against a flash that exists.
- **Compressed files**: `<name>.z` (UNIX compress) is read for every record file. `.sum` checksum files are not verified. ODB++ XML (ODX) is refused.

## Conventions taken from KiCad's exporter
- **Mirrored flashes (orient 9)**: read clockwise, as the spec says, except in archives whose misc/info names KiCad (ODB_SOURCE / SAVE_APP), whose exporter writes the angle counter-clockwise (a bottom pad at 30 deg is `9 30.0`). Checked on made-up boards and KiCad exports only: no real non-KiCad board with an angled mirrored flash has been seen yet.
- **Missing board outline**: KiCad exports a bounding-box profile for a board without Edge.Cuts; it is imported as the outline.
- **Bottom packages are already flipped**: kicad-cli writes a bottom part's eda/data package flipped, so its M flag must not mirror it again (the spec says it does). The package pins decide per part; a part without pins is placed kicad-cli's way.

## DNP
- **DNP footprints**: read from the CMP `.no_pop` attribute (kicad-cli 9's export of the DNP flag) or a DNP token in a `Config` property; see the README. Other producers' DNP conventions (other attribute or property names, assembly variants) are not read, and a footprint without either mark is written populated.

## Units
- **Per file**: each file is read in its own `UNITS` (else misc/info's, else INCH). A user symbol (`symbols/<name>`) is read in its own units.

## Precision
- **Coordinate rounding**: ODB++ coordinates may have rounding differences vs the original KiCad design (e.g., 100.91 vs 100.9125). The converter uses ODB++ values as-is.

## The oracle suite
- `test/run_oracle_tests.py` converts and compares every registered sample (`simple`, `kitchen-sink`, and `multilayer`, a 10-copper-layer job with compressed files that `test/synthetic_odb.py` makes at run time); all three pass. `bounding_box` is the profile's extent against Edge.Cuts' and `pin1_positions` places each pin 1 pad by its footprint's angle. Neither checks the shape of the outline (only its extent) or any pin but the first; the round-up's per-layer XOR covers those. Samples under `samples/` that are not registered (`odb-rotation`, `odb-dnp`, `rigidflex-odb`) run with `--odb <dir> --convert`.

## Reference-render comparison
Each public sample drawn from its ODB++ files by our reference renderer (`test/odb_render.py` with `test/odb_render_text.py` for text) against the converted board (`test/kicad_render.py`), per layer, 1 px tolerance: `python3 -I test/roundup.py samples/<job> --out /tmp/r --no-siemens`. Layers over 2% XOR, 2026-11-15:
- **Process-map text** (outside the board outline, so outside the round-up's view): `python3 -I tools/render_odb_sheet.py <job> --layer procmap_bot --kicad <board>` grows the view to the layer: on the synthetic multilayer job (`test/synthetic_odb.py --variant multilayer`) 0 px differ between the converter's text and the reference. With the simpler text reading (`--shipped-text-grammar`) every letter is drawn flipped and 26% of the layer differs (see "T text placement").
- **rigid-flex** (18 layers, 17 under 2%): `F.Mask` 3.0% (2.6% missing).
- **odb-rotation** (14 layers, 11 under 2%): `B.Cu` / `B.Mask` 11%, `B.Paste` 29%: kicad-cli's counter-clockwise orient 9, which the reference reads the spec's way on purpose (see "Mirrored flashes").
- **odb-kitchen-sink** (13 layers, 12 under 2%): `B.Paste` 13% of 110 px (one small opening).
- **odb-output**, **odb-dnp**: every layer under 2%.

## Not Implemented
- **Archive extraction**: Input must be a pre-extracted directory.
- **Netlist-driven DRC**: The converter trusts ODB++ data without verifying connectivity.
- **Round-tripping**: KiCad -> ODB++ -> KiCad won't produce identical files (the sections above list what still differs).

## DRC on an imported board
The board carries no design rules (KiCad keeps them in the .kicad_pro, ODB++ has none), so KiCad's DRC
checks it against its defaults. On the public rigid-flex sample (KiCad 9 kicad-cli, 2402 violations,
37 unconnected), every remaining type is one of these, not a geometry error:
- **Default rules vs an HDI design**: via_diameter, annular_width, drill_out_of_range, hole_to_hole,
  hole_clearance, clearance (microvias and 0.1 mm spacing under KiCad's 0.2-0.3 mm defaults).
- **Design facts**: solder_mask_bridge (mask openings spanning vias of other nets, untented via-in-pad),
  starved_thermal (pads the source fill joins with one spoke), isolated_copper (flex-layer islands),
  via_dangling (blind vias with no copper on one end layer), track_dangling / copper_edge_clearance on
  the flex layer (In4.Cu), present before the round-up fixes.
- **No library**: lib_footprint_issues (footprints name a library that is not configured).
KiCad stops reporting a type at about 200, so totals undercount the capped types (counts of 199 / 200).
Overlapping copper surfaces get zone priorities in file order and joined custom pads (net ties) are
marked, so zones_intersect and shorting_items do not appear.
