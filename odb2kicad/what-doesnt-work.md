# What Doesn't Work / Known Limitations

Measured by round-tripping KiCad boards (KiCad board -> kicad-cli 9 ODB++ export -> odb2kicad ->
KiCad) and diffing the result against the original, layer by layer.

## Import Quality
- **Text is rasterized**: ODB++ exports text as individual line strokes (or filled glyph surfaces). Imported as gr_line / gr_poly — visually correct but not editable as text objects in KiCad. Footprint Reference/Value fields are written hidden; their original position is not in the ODB++.
- **Zone outlines are the fill**: ODB++ only exports filled copper polygons, not the zone's drawn outline, its priority or its rules. Each imported zone's outline is its fill, priority 0. A KiCad zone with several islands comes back as one zone per island (the eda/data SNT PLN record groups them, but a KiCad zone file holds a single outline).
- **Zone net assignment**: nets come from the eda/data FID records. Without eda/data, a zone takes the net of a pad that lies in its fill (not in a clearance hole), else net 0.
- **Custom pad shapes**: rebuilt from the pin's copper surface (SNT TOP); the anchor shape is not exported, so it is a 0.01 mm circle inside the primitives.
- **Negative-polarity features** on any layer are not drawn.

## Layer Support
- **User layers 1-9**: ODB++ USER.1 through USER.9 don't have standard KiCad layer mappings and are skipped.
- **Tenting/filling/capping/plugging layers**: These ODB++ manufacturing layers have no KiCad equivalent and are skipped.
- **Blind/buried vias**: every via is written F.Cu-B.Cu; the drill layer's START/END span is not used.

## Conventions taken from KiCad's exporter
- **Mirrored flashes (orient 9)**: the angle is read counter-clockwise (KiCad's ODB++ export writes a bottom pad at 30 deg as `9 30.0`); other exporters are untested.
- **Missing board outline**: KiCad exports a bounding-box profile for a board without Edge.Cuts; it is imported as the outline.

## DNP
- **DNP footprints**: read from the CMP `.no_pop` attribute (kicad-cli 9's export of the DNP flag) or a DNP token in a `Config` property; see the README. Other producers' DNP conventions (other attribute or property names, assembly variants) are not read, and a footprint without either mark is written populated.

## Precision
- **Coordinate rounding**: ODB++ coordinates may have rounding differences vs the original KiCad design (e.g., 100.91 vs 100.9125). The converter uses ODB++ values as-is.

## Not Implemented
- **Archive extraction**: Input must be a pre-extracted directory.
- **Netlist-driven DRC**: The converter trusts ODB++ data without verifying connectivity.
- **Round-tripping**: KiCad -> ODB++ -> KiCad won't produce identical files (the sections above list what still differs).
