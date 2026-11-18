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
    odb_parser/             # modular parsers (matrix, features, components, eda, netlist, tools,
                            #   attrlist, font, ...) + odb_parser.cpp, the orchestrator (steps, units,
                            #   origin, text strokes, thickness, job checks)
    writer/
      kicad_writer.cpp/.h   # Emits .kicad_pcb s-expression output
      keyhole.cpp/.h        # holes folded into an outline (grid search; keyholeReference() = brute force)
    util/
      coord.h               # Coordinate types
      odb_file.cpp/.h       # openOdbFile(): a job file or its UNIX-compressed twin (<name>.z)
      string_utils.cpp/.h   # String helpers

wasm/                       # JS/npm wrapper around the WASM build
  package.json              # npm package: odb2kicad-wasm
  src/index.mjs             # JS API: convertOdb(), convertOdbArchive()
  src/index.d.ts            # TypeScript declarations
  bin/odb2kicad.mjs          # CLI entry point for `npx odb2kicad`
  scripts/build.mjs         # Copies WASM artifacts into dist/
  scripts/build-site.sh     # Assembles the demo site (page + dist + sample + npm .tgz) for GitHub Pages
  dist/                     # Built artifacts (committed)
  demo/index.html           # Browser demo page (deployed to GitHub Pages); runs dist/index.mjs, served beside it
  test/test-node.mjs        # Node.js integration tests (19 tests)

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
# --report: what the reader chose and assumed; --step <name>: one step of a multi-step job
# (a panel step, STEP-REPEAT, holds a copy of each placed step; --no-panels leaves them out)
# (with an output file, every other step is written beside it as <job>__<step>.kicad_pcb);
# --document-layers all|none|N: document layers drawn on free User.N layers (default N = 100000)
./odb2kicad/build/odb2kicad --report samples/odb-kitchen-sink /tmp/kitchen.kicad_pcb
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

The `wasm/dist/` directory is committed to the repo so the npm package can be installed directly from the repository (or the demo site) without building.

## Testing

### Unit tests (85 cases)

```bash
bash odb2kicad/test/run_unit_tests.sh [name-filter]   # plain g++, no cmake needed
```

### Native smoke tests (80+ assertions)

```bash
cd odb2kicad && bash test/test_simple.sh
```

Requires the native CLI to be built first (the CMake build defaults to Release) and `python3` (the
multilayer section makes its job with `test/synthetic_odb.py --variant multilayer`: 10 copper
layers, INCH by default, compressed `.z` files, text, drill tools, parts on both sides). Tests the
sample boards and made-up jobs.

### WASM integration tests (19 tests)

```bash
cd /root/odb2kicad/wasm && node test/test-node.mjs
```

Requires both native CLI and WASM to be built. Tests include:
- Directory-based and archive-based conversion (.tgz, .zip)
- WASM vs native byte-identical parity (odb-output, and the synthetic multilayer job with its compressed files; needs `python3`)
- Multi-step jobs (`{ step }`), the `report` and `steps` fields
- Error handling (empty input, invalid archives)
- Sequential conversion cleanup (MEMFS isolation)
- A file the reader throws on comes back as an error, never an abort, and the module keeps working (tests 17-18); the Xpedition rigid-flex job matches native (test 19)

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
python test/run_oracle_tests.py                          # converts and compares every registered sample
python test/run_oracle_tests.py --sample multilayer      # one sample (converted first; this one is made up at run time)
python test/run_oracle_tests.py --sample simple --stored # compare samples/test.kicad_pcb instead
python3 -m unittest discover -s test -p 'test_compare_oracle.py'  # the checks fail on broken boards
```
Needs the native converter built. A sample that cannot be converted or compared is an ERROR,
and an ERROR fails the run (exit 1) like a FAIL does.

**Run structural comparison on a specific sample:**
```bash
python test/compare_oracle.py samples/odb-output samples/test.kicad_pcb --verbose
```

**Generate visual comparison (requires KiCad installed):**
```bash
python test/visual_compare.py samples/odb-output samples/test.kicad_pcb --output-dir test/output/visual
```

**What gets compared:**
- Component count (oracle vs KiCad footprints), positions and rotations
- Feature counts (pads, lines, arcs per layer; informational)
- Bounding box: the profile's extent against Edge.Cuts', edge by edge (arcs with their bulge)
- Pin 1 positions: each footprint's pin 1 pad turned by the footprint angle and placed, against
  the ODB++ TOP record (pin named `1`, else the part's first terminal)
- Trace/segment counts on copper layers (informational)

### ODB++ round-up (Linux / headless)

Per board and per layer: each converter's `.kicad_pcb` vs an independent reference drawn straight
from the ODB++ files. No KiCad, no Pillow, no viewer needed; pure Python 3.10+.

```bash
python3 -I test/roundup.py samples/odb-output samples/odb-kitchen-sink --out /tmp/roundup
python3 -I test/synthetic_odb.py /tmp/synth                      # made-up boards (basic / edge / mixed units)
python3 -I test/synthetic_odb.py /tmp/synth --variant symbols    # every standard symbol 0.3.0 missed (also: outline, negative)
python3 -I test/roundup.py /tmp/synth/* samples/designodb_rigidflex.tgz --out /tmp/roundup \
    --converter odb2kicad=native:odb2kicad/build/odb2kicad --converter other=pcb:/path/to/boards/
python3 -I test/synthetic_odb.py /tmp/synth --variant text       # also: origin, stackup, steps (the 0.4.0 boards)
python3 -I test/synthetic_odb.py /tmp/synth --variant glyphs     # mm font, POLYGON glyphs, V text, arcs without a direction
python3 -I test/synthetic_odb.py /tmp/synth --variant multilayer # 10 copper layers, INCH by default, .z files, both-side parts
python3 -m unittest discover -s test -p 'test_roundup.py'        # 25 tests, made-up boards only
python3 -I tools/render_odb_sheet.py /tmp/synth/synthetic-multilayer --layer procmap_bot --out /tmp/sheet   # the reference side alone, as PNGs
```

Writes `<out>/<job>/report.json` (per layer: `xor_ratio` = XOR / union of ink; `missing_ratio` /
`extra_ratio` = ink farther than `--tol-px` from the other side, over that side's ink; the outline
alignment; compare_oracle.py's structural diff), `<out>/<job>/sheet.png` (reference | converter |
diff, one row per non-empty layer; grey both, red missing, green extra), `<out>/index.html` (every
layer with a "better: ref / converter / same" pick and a comment; **Download verdicts.json**, no
server) and `<out>/metrics.json` (numbers only).

- `test/raster.py`: the one rasterizer both sides use (even-odd scanline, PNG writer, metrics).
- `test/odb_render.py`: ODB++ side. Records from `valor_oracle.parse_feature_geometry`; standard
  symbols per the ODB++ spec plus `symbols/<name>` user symbols; per-file `UNITS` and `$n name I|M`
  overrides; polarity in file order; rotation clockwise, orient 4-7/9 mirror X first (the spec,
  not kicad-cli's counter-clockwise orient 9, so KiCad-made boards show that convention as a diff).
  Layers from the matrix only: copper rows -> F.Cu / In<n>.Cu / B.Cu, mask/paste/silk by side,
  every DRILL row -> `Drill`, profile -> `Edge.Cuts` (both sides stroked 0.3 mm).
- `test/odb_render_text.py`: our own reference font parser + T / V text renderer, which
  `odb_render.py` draws text with; `tools/render_odb_sheet.py` is our reference renderer's CLI over
  the same code (any job / step / layer to PNG).
- `test/kicad_render.py`: KiCad side (pads, tracks, arcs, vias, zone fills, graphics, holes).
- Frame: odb2kicad's (x, -y); the Edge.Cuts bbox shifts it only when its size matches the profile.
- Siemens column: set `ODBVIEWER_HOME`, `DISPLAY` (Xvfb) and have `server.pl` on :56753 with the job
  open (`ODBVIEWER_JOB`, `ODBVIEWER_TMP`); LIMITS + SYMS_HIST per layer go into report.json. Absent
  viewer -> one "siemens: skipped (...)" line.
- External batch runs: `compare_job()` / `metrics_only()` are the library entry points for running the
  same comparison elsewhere; only `metrics_only()` numbers should leave that machine.
- Inputs are untrusted: archives are unpacked (regular files only, no `..`) under `--scratch`;
  keep customer files out of the repo (`samples/private/` is gitignored) and their outputs too.

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
- **Units**: per file. `normalizeUnits()` (called by `parseOdbDirectory`) scales every file to mm by its own `UNITS=` line, else misc/info's `UNITS`, else INCH (the spec's default); a symbol's `I`/`M` suffix overrides its file's units. The writer then works in mm.
- **Step origin**: `normalizeUnits()` also applies each layer's output transform (attrlist `.out_*`), strokes `T` text records from the job's font into the layer's lines, and moves the step by its stephdr `X_ORIGIN` / `Y_ORIGIN`.
- **Files**: every parser opens its file through `openOdbFile()` (util/odb_file.h), which falls back to the UNIX-compressed `<name>.z`; new parsers must too, and must be lenient about empty or missing files.
- **Report**: what the reader assumes or drops goes to `OdbDesign::report` as `warning: ...` / `info: ...` lines (`ConvertResult::report`, CLI `--report`, WASM `result.report`), not to stderr.
- Native and WASM outputs must be byte-identical (enforced by tests 3 and 15)
- `wasm/dist/` is committed — rebuild and commit after any C++ changes
- The WASM build has C++ exceptions on (`-fwasm-exceptions`): a throw reaches `convert()`'s catch and is reported, as natively. Without it every throw is an `abort()` ("Aborted()" in the demo). Still, do not use exceptions for ordinary control flow: read numbers with `parseInt()` (util/string_utils.h), not `std::stoi` in a try block

## CI/CD

GitHub Actions (`.github/workflows/pages.yml`) deploys the demo to GitHub Pages on every push to `main`, on `v*` tags, and on demand:
- Builds the WASM in `emscripten/emsdk:3.1.50`, warns if it differs from the committed `wasm/dist`, then `wasm/scripts/build-site.sh _site` copies `wasm/demo/index.html`, `wasm/dist/{index.mjs,odb2kicad_wasm.*}`, `samples/test-odb.tgz` and a fresh `npm pack` into the site
- Demo: https://fixturfab.github.io/ODB2kicad/
- npm install: `npm install -g https://fixturfab.github.io/ODB2kicad/odb2kicad-wasm.tgz`
- Preview locally: `bash wasm/scripts/build-site.sh _site && python3 -m http.server -d _site 8000`

## License

GPL-3.0 (builds upon KiCad source code)
