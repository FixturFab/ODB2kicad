# odb2kicad

ODB++ to KiCad `.kicad_pcb` converter. Pure C++17, no external dependencies. Also builds to WebAssembly via Emscripten.

## Project Structure

```
odb2kicad/                  # C++ converter
  CMakeLists.txt            # Builds native CLI + WASM (if Emscripten)
  src/
    main.cpp                # CLI entry point
    odb2kicad_lib.h/.cpp    # Public library API: ConvertResult convert(path)
    wasm_bindings.cpp       # Emscripten embind bindings
    model/
      odb_design.h          # ODB++ data model structs
      kicad_pcb.h           # KiCad PCB data model structs
    odb_parser/             # 10 modular parsers (matrix, features, components, etc.)
    writer/
      kicad_writer.cpp/.h   # Emits .kicad_pcb s-expression output
    util/
      coord.h               # Coordinate types
      string_utils.cpp/.h   # String helpers

wasm/                       # JS/npm wrapper around the WASM build
  package.json              # npm package: odb2kicad-wasm
  src/index.mjs             # JS API: convertOdb(), convertOdbArchive()
  src/index.d.ts            # TypeScript declarations
  bin/odb2kicad.mjs          # CLI entry point for `npx odb2kicad`
  scripts/build.mjs         # Copies WASM artifacts into dist/
  dist/                     # Built artifacts (committed)
  demo/index.html           # Browser demo page (deployed to GitLab Pages)
  test/test-node.mjs        # Node.js integration tests (11 tests)

samples/
  odb-output/               # Simple 2-resistor test board
  odb-kitchen-sink/         # Complex board (vias, TH, zones, arcs, polygons)
  test-odb.tgz              # Archive of odb-output for archive tests
```

## Building

### Native CLI

```bash
cmake -S odb2kicad -B odb2kicad/build
cmake --build odb2kicad/build -j$(nproc)
```

Produces `odb2kicad/build/odb2kicad`. Requires CMake 3.10+ and a C++17 compiler. No external libraries.

```bash
./odb2kicad/build/odb2kicad samples/odb-output output.kicad_pcb
# Or write to stdout:
./odb2kicad/build/odb2kicad samples/odb-output
```

### WASM Build

Requires Emscripten. The SDK env script is at `/root/emsdk/emsdk_env.sh`. All commands must run from the repo root (`/root/odb2kicad`).

```bash
source /root/emsdk/emsdk_env.sh
cd /root/odb2kicad
emcmake cmake -S odb2kicad -B odb2kicad/build-wasm -DCMAKE_BUILD_TYPE=Release
emmake make -C odb2kicad/build-wasm -j$(nproc)
```

Produces `odb2kicad/build-wasm/odb2kicad_wasm.{mjs,wasm}`.

Then copy artifacts into the npm package:

```bash
cd wasm && node scripts/build.mjs
```

### npm Package

```bash
cd wasm && npm pack
```

The `wasm/dist/` directory is committed to the repo so the npm package can be installed directly from GitLab Pages without building.

## Testing

### Native smoke tests (60+ assertions)

```bash
cd odb2kicad && bash test/test_simple.sh
```

Requires the native CLI to be built first. Tests both sample boards.

### WASM integration tests (11 tests)

```bash
cd /root/odb2kicad/wasm && node test/test-node.mjs
```

Requires both native CLI and WASM to be built. Tests include:
- Directory-based and archive-based conversion (.tgz, .zip)
- WASM vs native byte-identical parity
- Error handling (empty input, invalid archives)
- Sequential conversion cleanup (MEMFS isolation)

## Architecture

1. **ODB Parsers** — read ODB++ directory structure into `OdbDesign` model
2. **Transformer** (`odb2kicad_lib.cpp`) — coordinate transforms, layer/net remapping
3. **KiCad Writer** — emits `.kicad_pcb` s-expression from `KicadPcb` model

WASM bindings use Emscripten embind. The JS wrapper (`wasm/src/index.mjs`) handles MEMFS file staging and archive extraction (using `fflate` for gzip/zip).

## Key Conventions

- C++17, no external dependencies, POSIX filesystem calls only (opendir/readdir/stat)
- Emscripten MEMFS handles filesystem calls transparently in WASM
- Coordinates: ODB++ uses mils, converted to mm for KiCad
- Native and WASM outputs must be byte-identical (enforced by test 3)
- `wasm/dist/` is committed — rebuild and commit after any C++ changes

## CI/CD

GitLab CI (`.gitlab-ci.yml`) deploys to GitLab Pages:
- Copies `wasm/demo/index.html`, WASM artifacts, and npm `.tgz` to `public/`
- Demo: https://henrybtroutman.gitlab.io/odb2kicad/
- npm install: `npm install -g https://henrybtroutman.gitlab.io/odb2kicad/odb2kicad-wasm.tgz`

## License

GPL-3.0 (builds upon KiCad source code)
