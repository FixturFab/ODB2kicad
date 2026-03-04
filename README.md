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
- **Copper traces** — segments on F.Cu and B.Cu with proper width and net
- **Vias** — detected from drill layers with correct drill and pad sizing
- **Copper fill zones** — imported as filled polygons from surface records
- **Board outline** — rectangular and polygonal profiles, including cutout holes
- **Graphics** — silkscreen, fab, and courtyard lines and arcs on all layers
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
cd odb2kicad && bash test/test_simple.sh
```

Runs 60+ assertions against two sample boards:
- **Simple** (`samples/odb-output`) — 2-resistor board: nets, components, pads, traces, board outline
- **Kitchen-sink** (`samples/odb-kitchen-sink`) — complex board: vias, through-hole, copper zones, arcs, polygonal outline with holes

## Known Limitations

- **Text is rasterized** — ODB++ exports text as line strokes; imported as line segments, not editable KiCad text objects
- **Zone outlines simplified** — copper fills are imported as filled polygons without thermal relief or clearance rules
- **Two copper layers only** — F.Cu and B.Cu supported; inner copper layers are parsed but not yet mapped
- **Precision** — ODB++ coordinates may have minor rounding differences vs. the original design

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
