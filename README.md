# odb2kicad

Convert ODB++ PCB designs to KiCad `.kicad_pcb` format.

## Try It

**[Live Demo](https://henrybtroutman.gitlab.io/odb2kicad/)** — drag and drop an ODB++ archive and get a `.kicad_pcb` file instantly, right in your browser. No install, no upload — everything runs locally via WebAssembly.

## Install

Changes per release: [CHANGELOG.md](CHANGELOG.md).

```bash
npm install -g https://raw.githubusercontent.com/FixturFab/odb2kicad/main/wasm/odb2kicad-wasm.tgz
```

Or, from a clone of this repository:

```bash
npm install -g ./wasm/odb2kicad-wasm.tgz
```

Then convert from the command line:

```bash
odb2kicad input.tgz -o output.kicad_pcb
odb2kicad input.zip > output.kicad_pcb
```

Supports `.tgz`, `.tar.gz`, `.zip`, and `.tar` archives.

## Use as a Library

```bash
npm install odb2kicad-wasm
```

```js
import { convertOdbArchive } from 'odb2kicad-wasm';
import { readFileSync } from 'fs';

const archive = readFileSync('board.tgz');
const result = await convertOdbArchive(archive);

if (result.success) {
  console.log(result.kicadPcb);
} else {
  console.error(result.error);
}
```

The API also exposes `convertOdb(files)` for pre-extracted file arrays — see the [type definitions](wasm/dist/index.d.ts) for details.

## Features

- **Components & pads** — SMD and through-hole placement with correct positioning, rotation, and pad shapes
- **Net assignment** — pads, traces, and vias assigned to their correct nets
- **Copper traces** — segments on outer and inner copper layers with proper width and net
- **Vias** — detected from drill layers with correct drill and pad sizing
- **Copper fill zones** — imported from surface records, nets taken from the eda/data FID records
- **Custom-shape pads** — rebuilt from the pin's copper surface
- **DNP footprints** — written with the `dnp` attribute (see [DNP Footprints](#dnp-footprints))
- **Board outline** — rectangular and polygonal profiles, including cutout holes
- **Graphics** — silkscreen, mask, fab, courtyard and user-layer lines, arcs, flashes and filled shapes
- **Footprint courtyards** — courtyard strokes claimed by the eda/data package outline become the footprint's own F/B.CrtYd lines, arcs and circles
- **Layer mapping** — ODB++ layers mapped to KiCad canonical layer names

## DNP Footprints

A footprint is written as `(attr ... dnp)` when either of these holds:

1. **`.no_pop` on its CMP record.** This is how kicad-cli 9 exports a footprint's *Do not populate* flag: a boolean attribute (an index with no `=`, e.g. `CMP ... ;0,1=1` with `@0 .no_pop`). The components file only defines `.no_pop` when at least one footprint is DNP.
2. **A `Config` property** (`PRP Config '...'`) whose whole value, or one comma- or semicolon-separated token of it, is `dnp`, `do not populate`, `do_not_populate`, `no_pop`, `nopop` or `not populated` (case-insensitive). `Rev B; DNP` counts, `DNP-ready` does not. KiCad exports a user field named `Config` like any other field, so this only applies when a board keeps its DNP mark in that field. KiCad does not set the flag from the field.

There is no precedence between the two: either one makes the footprint DNP, and neither clears the other. A `.no_pop` footprint stays DNP whatever its `Config` says. No other property or attribute is read for DNP.

## Build from Source

For the native C++ CLI (no Node.js required):

```bash
cmake -S odb2kicad -B odb2kicad/build
cmake --build odb2kicad/build -j$(nproc)
./odb2kicad/build/odb2kicad /path/to/extracted-odb output.kicad_pcb
```

Requires CMake 3.10+ and a C++17 compiler. No external libraries.

The native CLI takes a pre-extracted ODB++ directory as input (use `tar xzf` or `unzip` first).

## Testing

```bash
cd odb2kicad
bash test/run_unit_tests.sh     # unit tests (plain g++, no cmake needed); or: ctest --test-dir build
bash test/test_simple.sh        # end-to-end checks on the sample boards
```

The unit tests build small designs in memory or as tiny ODB++ files, one or more cases per
conversion rule (pad typing from drill hits, inner layers, FID nets, NPTH, outline, rotation,
custom pads, non-copper graphics).

`test_simple.sh` runs 60+ assertions against four sample boards:
- **Simple** (`samples/odb-output`) — 2-resistor board: nets, components, pads, traces, board outline
- **Kitchen-sink** (`samples/odb-kitchen-sink`) — complex board: vias, through-hole, copper zones, arcs, polygonal outline with holes
- **DNP** (`samples/odb-dnp`) — kicad-cli 9.0.9 export: R1 has the DNP flag (`.no_pop`), R2 a `Config` field `Rev B; DNP`
- **Rotation** (`samples/odb-rotation`) — kicad-cli 9.0.9 export: SOT-23s and 1x03 headers at 0/30/45/90/270°, on top and flipped to the bottom

## Known Limitations

- **Text is rasterized** — ODB++ exports text as line strokes; imported as line segments, not editable KiCad text objects
- **Zone outlines are the fill** — ODB++ carries the filled copper, not the zone's drawn outline, priority or rules
- **Blind/buried vias** — every via is written through (F.Cu-B.Cu)
- **Precision** — ODB++ coordinates may have minor rounding differences vs. the original design

Details in [odb2kicad/what-doesnt-work.md](odb2kicad/what-doesnt-work.md).

## Architecture

```
ODB++ archive (.tgz / .zip)
        │
        ▼
┌─────────────────┐
│  ODB Parsers    │  10 modular parsers → OdbDesign model
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  Transformer    │  Coordinate transforms, layer/net remapping
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  KiCad Writer   │  Emit .kicad_pcb s-expression
└─────────────────┘
```

## License

This project is licensed under the [GNU General Public License v3.0](LICENSE).
