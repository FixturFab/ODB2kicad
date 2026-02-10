# odb2kicad

Convert ODB++ PCB designs to KiCad `.kicad_pcb` format.

A standalone C++17 command-line tool that reads a pre-extracted ODB++ directory tree and writes a KiCad s-expression PCB file. Zero external dependencies — pure C++17 standard library only.

## Quick Start

```bash
# Build
cmake -S odb2kicad -B odb2kicad/build
cmake --build odb2kicad/build -j$(nproc)

# Convert (to file)
./odb2kicad/build/odb2kicad /path/to/extracted-odb output.kicad_pcb

# Convert (to stdout)
./odb2kicad/build/odb2kicad /path/to/extracted-odb > output.kicad_pcb
```

The input must be a pre-extracted ODB++ directory (use `tar xzf` or `unzip` on the archive first).

## Features

- **Components & pads** — SMD and through-hole placement with correct positioning, rotation, and pad shapes (round, rect, roundrect, oval)
- **Net assignment** — pads, traces, and vias are assigned to their correct nets
- **Copper traces** — segments on F.Cu and B.Cu with proper width and net
- **Vias** — detected from drill layers with correct drill and pad sizing
- **Copper fill zones** — imported as filled polygons from surface records
- **Board outline** — rectangular and polygonal profiles, including cutout holes
- **Graphics** — silkscreen, fab, and courtyard lines and arcs on all layers
- **Layer mapping** — ODB++ layers mapped to KiCad canonical layer names (F.Cu, B.Cu, F.SilkS, F.Mask, Edge.Cuts, etc.)

## Building

### Prerequisites

- CMake 3.10+
- C++17 compiler (GCC or Clang)

No external libraries required.

```bash
cmake -S odb2kicad -B odb2kicad/build
cmake --build odb2kicad/build -j$(nproc)
```

## Architecture

```
ODB++ directory tree
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

### Source Layout

```
odb2kicad/
├── CMakeLists.txt
├── src/
│   ├── main.cpp                    # CLI entry point
│   ├── odb_parser/                 # Modular ODB++ parsers
│   │   ├── odb_parser.cpp/h        #   Top-level orchestrator
│   │   ├── info_parser.cpp/h       #   misc/info (units, job name)
│   │   ├── matrix_parser.cpp/h     #   matrix/matrix (layer stack)
│   │   ├── stephdr_parser.cpp/h    #   steps/pcb/stephdr (origins)
│   │   ├── profile_parser.cpp/h    #   steps/pcb/profile (board outline)
│   │   ├── features_parser.cpp/h   #   Layer features (pads, lines, arcs, surfaces)
│   │   ├── components_parser.cpp/h #   Component placement
│   │   ├── eda_parser.cpp/h        #   EDA data (packages, nets, pins)
│   │   ├── netlist_parser.cpp/h    #   Netlist definitions
│   │   └── symbols.cpp/h           #   Symbol name → pad geometry decoder
│   ├── model/
│   │   ├── odb_design.h            #   In-memory ODB++ design model
│   │   └── kicad_pcb.h             #   In-memory KiCad PCB model
│   ├── writer/
│   │   └── kicad_writer.cpp/h      #   Transform + s-expression output
│   └── util/
│       ├── string_utils.cpp/h      #   String helpers
│       └── coord.h                 #   Coordinate transforms
└── test/
    └── test_simple.sh              #   Smoke tests (60+ assertions)
```

## Testing

```bash
cd odb2kicad && bash test/test_simple.sh
```

Runs against two sample boards:
- **Simple** (`samples/odb-output`) — 2-resistor board: validates nets, components, pads, traces, board outline
- **Kitchen-sink** (`samples/odb-kitchen-sink`) — complex board: validates vias, through-hole components, copper zones, arcs, polygonal outline with holes

## Known Limitations

- **Text is rasterized** — ODB++ exports text as line strokes; imported as line segments, not editable KiCad text objects
- **Zone outlines simplified** — copper fills are imported as filled polygons without thermal relief or clearance rules
- **Two copper layers only** — F.Cu and B.Cu are supported; inner copper layers are parsed but not yet mapped
- **No archive extraction** — input must be a pre-extracted ODB++ directory
- **Precision** — ODB++ coordinates may have minor rounding differences vs. the original design

## License

This project builds upon KiCad source code, which is licensed under the [GNU General Public License v3.0](https://www.gnu.org/licenses/gpl-3.0.html). See the [KiCad project](https://www.kicad.org/) for details.
