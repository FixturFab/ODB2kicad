# What Doesn't Work / Known Limitations

## Import Quality
- **Text is rasterized**: ODB++ exports text as individual line strokes. Imported as gr_line segments — visually correct but not editable as text objects in KiCad.
- **Zone outlines are simplified**: ODB++ only exports filled copper polygons, not original zone outlines with rules. Zones are imported as filled polygons without thermal relief or clearance rules.
- **Zone net assignment**: Copper fill zones are imported with net 0 (unconnected). The ODB++ format doesn't directly associate surface records with nets at the feature level.
- **Custom pad shapes**: Non-standard pad shapes beyond round, rect, roundrect, oval are approximated or may not render correctly.

## Layer Support
- **Inner copper layers**: Only F.Cu and B.Cu are mapped. Inner copper layers (In1.Cu, In2.Cu, etc.) are parsed but not mapped to KiCad inner layer IDs.
- **User layers 1-9**: ODB++ USER.1 through USER.9 don't have standard KiCad layer mappings and are skipped.
- **Tenting/filling/capping/plugging layers**: These ODB++ manufacturing layers have no KiCad equivalent and are skipped.

## Precision
- **Coordinate rounding**: ODB++ coordinates may have rounding differences vs the original KiCad design (e.g., 100.91 vs 100.9125). The converter uses ODB++ values as-is.
- **Donut symbol dimensions**: Donut symbols on Edge.Cuts are approximated as two half-arcs. The inner/outer ring distinction is lost.

## Not Implemented
- **Archive extraction**: Input must be a pre-extracted directory.
- **Netlist-driven DRC**: The converter trusts ODB++ data without verifying connectivity.
- **Round-tripping**: KiCad -> ODB++ -> KiCad won't produce identical files.
