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
  test.kicad_pcb            # Reference KiCad file for oracle comparison

test/                       # Python oracle validation tests
  valor_oracle.py           # ODB++ parser + Valor TCP client
  compare_oracle.py         # Structural comparison tool
  visual_compare.py         # Visual screenshot comparison
  run_oracle_tests.py       # Unified test runner
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

### Docker Build (Cross-Platform)

For Windows or when native build tools aren't available, use Docker:

```bash
# Build the Docker image (includes native CLI)
docker build -t odb2kicad -f Dockerfile .

# Convert an ODB++ directory (output to stdout)
docker run --rm -v "$(pwd)/samples:/app/samples" odb2kicad odb2kicad samples/odb-output

# Convert and save to file (use redirect since volume mounts can be tricky)
docker run --rm -v "$(pwd)/samples:/app/samples" odb2kicad odb2kicad samples/odb-output > output.kicad_pcb
```

The Docker image uses Ubuntu 22.04 with GCC 11, providing a consistent build environment.

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

### Docker WASM Build (Cross-Platform)

For building WASM without a local Emscripten installation:

```bash
# Build the WASM Docker image
docker build -t odb2kicad-wasm -f Dockerfile.wasm .

# Extract WASM artifacts to wasm/dist/
docker create --name wasm-extract odb2kicad-wasm
docker cp wasm-extract:/app/odb2kicad/build-wasm/odb2kicad_wasm.wasm wasm/dist/
docker cp wasm-extract:/app/odb2kicad/build-wasm/odb2kicad_wasm.mjs wasm/dist/
docker rm wasm-extract
```

The Docker image uses `emscripten/emsdk:3.1.50` for consistent WASM builds.

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

### Oracle Validation Tests (Python)

Uses an independent ODB++ parser as an "oracle" to validate converter output. This catches parser bugs that would otherwise go undetected when comparing converter input/output.

```
test/
├── valor_oracle.py      # ODB++ oracle parser + Siemens Valor TCP client
├── compare_oracle.py    # Structural comparison (components, features, bbox)
├── visual_compare.py    # Visual comparison (KiCad render + ODB viewer screenshots)
└── run_oracle_tests.py  # Unified test runner
```

**Run all oracle tests:**
```bash
python test/run_oracle_tests.py
```

**Run structural comparison on a specific sample:**
```bash
python test/compare_oracle.py samples/odb-output samples/test.kicad_pcb --verbose
```

**Generate visual comparison (requires KiCad installed):**
```bash
python test/visual_compare.py samples/odb-output samples/test.kicad_pcb --output-dir test/output/visual
```

**What gets compared:**
- Component count (oracle vs KiCad footprints)
- Feature counts (pads, lines, arcs per layer)
- Bounding box dimensions
- Trace/segment counts on copper layers

**Siemens ODB++ Viewer Integration:**

The Valor TCP client (`valor_oracle.py`) can connect to a running Siemens ODB++ Viewer instance via TCP port 56753 for live structured data extraction. This requires:
1. Launch ODB++ Viewer with your design
2. Run `server.pl` within the viewer environment
3. Connect via `ValorClient(host='localhost', port=56753)`

Available INFO queries: `entity_type`, `data_type=LIMITS`, `data_type=SYMS_HIST`, etc.

## Architecture

1. **ODB Parsers** — read ODB++ directory structure into `OdbDesign` model
2. **Transformer** (`odb2kicad_lib.cpp`) — coordinate transforms, layer/net remapping
3. **KiCad Writer** — emits `.kicad_pcb` s-expression from `KicadPcb` model

WASM bindings use Emscripten embind. The JS wrapper (`wasm/src/index.mjs`) handles MEMFS file staging and archive extraction (using `fflate` for gzip/zip).

## Key Conventions

- C++17, no external dependencies, POSIX filesystem calls only (opendir/readdir/stat)
- Emscripten MEMFS handles filesystem calls transparently in WASM
- **Units**: ODB++ defaults to INCH when `UNITS=` line is not present (per ODB++ spec). The converter auto-detects units from component/feature files and scales to mm for KiCad (INCH × 25.4 = mm). Files with `UNITS=MM` are passed through without scaling.
- Native and WASM outputs must be byte-identical (enforced by test 3)
- `wasm/dist/` is committed — rebuild and commit after any C++ changes

## CI/CD

GitLab CI (`.gitlab-ci.yml`) deploys to GitLab Pages:
- Copies `wasm/demo/index.html`, WASM artifacts, and npm `.tgz` to `public/`
- Demo: https://henrybtroutman.gitlab.io/odb2kicad/
- npm install: `npm install -g https://henrybtroutman.gitlab.io/odb2kicad/odb2kicad-wasm.tgz`

## License

GPL-3.0 (builds upon KiCad source code)
