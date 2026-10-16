# odb2kicad

Convert ODB++ PCB designs to KiCad `.kicad_pcb` format.

## Try It

**[Live Demo](https://henrybtroutman.gitlab.io/odb2kicad/)** — drag and drop an ODB++ archive and get a `.kicad_pcb` file instantly, right in your browser. No install, no upload — everything runs locally via WebAssembly.

## Install

```bash
npm install -g https://fixurfab.github.io/ODB2kicad/odb2kicad-wasm.tgz
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
- **Board outline** — rectangular and polygonal profiles, including cutout holes
- **Graphics** — silkscreen, mask, fab, courtyard and user-layer lines, arcs, flashes and filled shapes
- **Layer mapping** — ODB++ layers mapped to KiCad canonical layer names

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

`test_simple.sh` runs 60+ assertions against two sample boards:
- **Simple** (`samples/odb-output`) — 2-resistor board: nets, components, pads, traces, board outline
- **Kitchen-sink** (`samples/odb-kitchen-sink`) — complex board: vias, through-hole, copper zones, arcs, polygonal outline with holes

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
