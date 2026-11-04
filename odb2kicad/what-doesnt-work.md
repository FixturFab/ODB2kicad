# What Doesn't Work / Known Limitations

Measured by round-tripping KiCad boards (KiCad board -> kicad-cli 9 ODB++ export -> odb2kicad ->
KiCad) and diffing the result against the original, layer by layer.

## Import Quality
- **Text is rasterized**: ODB++ exports text as individual line strokes (or filled glyph surfaces). Imported as gr_line / gr_poly — visually correct but not editable as text objects in KiCad. Footprint Reference/Value fields are written hidden; their original position is not in the ODB++.
- **Zone outlines are the fill**: ODB++ only exports filled copper polygons, not the zone's drawn outline, its priority or its rules. Each imported zone's outline is its fill, priority 0. A KiCad zone with several islands comes back as one zone per island (the eda/data SNT PLN record groups them, but a KiCad zone file holds a single outline).
- **Zone net assignment**: nets come from the eda/data FID records. Without eda/data, a zone takes the net of a pad that lies in its fill (not in a clearance hole), else net 0.
- **Free pads are footprints**: a copper flash that is neither a component pin nor a via (test point, fiducial, mounting pad) becomes a board-only footprint (`odb2kicad:FreePad`, reference FID/TP/PAD/H<n>) with one pad. KiCad has no SMD pads on inner layers, so an inner-layer free flash without a hole is a filled copper graphic with no net.
- **Mask / paste openings**: a pad's opening that is its own shape grown evenly is a `solder_mask_margin` / `solder_paste_margin`; any other opening (another shape, aspect ratio or corner radius, a custom symbol) is a mask- or paste-only pad of its own shape on the same pin; further flashes at the pin and a via's opening (untented via) are board graphics on the mask layer.
- **Symbols without a KiCad pad shape** (thermals, octagons, chamfered or part-rounded rectangles, ellipses, diamonds, hexagons, triangles, butterflies, square / oval donuts, and user symbols): a pin or free flash of one is a KiCad custom pad (0.01 mm anchor, the outline as primitives); elsewhere it is a filled polygon. Thermal gaps are straight bars (no rounded ends), `oval_h` is drawn as a full oval, and `moire` is not drawn. A user symbol is its positive features less its negative ones, in no particular order.
- **Custom pad shapes**: rebuilt from the pin's copper surface (SNT TOP); the anchor shape is not exported, so it is a 0.01 mm circle inside the primitives.
- **Negative polarity**: a negative feature is cut out of the surfaces drawn before it on its layer (zones, copper and other polygons), and a NEGATIVE matrix layer becomes a plane: the profile less its features, one zone per island, on the net most through-hole pins and vias in its copper carry. Not done: a negative feature over a trace, pad or line drawn before it leaves that copper whole (KiCad has no way to clip a track), a negative feature on a NEGATIVE layer (copper again) is not drawn, and the clearances live only in the zone fill: KiCad's refill does not keep a void it has no rule for.
- **Footprint courtyards need courtyard strokes**: a footprint's courtyard is rebuilt from the F/B.Courtyard strokes its eda/data package outline runs along (90% of the outline's length). A package outline with no strokes under it draws nothing: kicad-cli writes a footprint's bounding hull there when it has no courtyard, Allegro the place-bound, Altium the body, and those are not courtyards. Allegro and Altium package outlines are not imported.

## Layer Support
- **User layers**: USER.1 through USER.9 (kicad-cli's names) map to KiCad's User.1-User.9. Other producers' DOCUMENT layers (assembly, notes, board outline copies) have no KiCad layer by name and are skipped.
- **Tenting/filling/capping/plugging layers**: These ODB++ manufacturing layers have no KiCad equivalent and are skipped.
- **Blind/buried vias**: a via spans its drill layer's START/END_NAME and is written `blind` when that is not F.Cu-B.Cu (KiCad's token for blind and buried); micro vias are written as blind. A KiCad 8 via has one size: the largest copper flash along its span.

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
