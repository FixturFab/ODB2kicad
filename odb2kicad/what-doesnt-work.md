# What Doesn't Work Yet

## Stage D (not yet implemented)
- **Copper fills/zones**: Surface records on copper layers not emitted as KiCad zones
- **Silkscreen graphics**: Lines/pads on silkscreen layers not emitted as gr_line/fp_line
- **Fab layer graphics**: Lines on fab layers not emitted
- **Arcs**: Arc records parsed but not emitted as gr_arc (midpoint computation needed)
- **Board outline holes**: Profile holes (H contours) skipped, only outer boundary emitted
- **Bottom-side components**: comp_+_bot parsing exists but untested with real data
- **Courtyard graphics**: F.CrtYd/B.CrtYd features not emitted

## Known Limitations (by design)
- Text is rasterized as line strokes, not editable text objects
- Only F.Cu and B.Cu copper layers supported (no inner layers)
- Zone outlines are lost (ODB++ only has filled polygons)
- No archive extraction (must pre-extract ODB++ directory)
- Precision differences vs original KiCad (ODB++ rounding)
