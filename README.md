# KiCad DRC WASM

Run KiCad design rule checks in the browser or Node.js via WebAssembly.

This project compiles KiCad's DRC (Design Rule Check) engine to WebAssembly, enabling PCB design rule validation without a native KiCad installation.

## Features

- **26 DRC test providers** — clearance, track width, hole size, annular width, silk/mask, courtyard, connectivity, and more
- **JSON output** following the [`schemas.kicad.org/drc.v1.json`](https://schemas.kicad.org/drc.v1.json) schema
- **Multi-threaded** via WebAssembly pthreads (SharedArrayBuffer)
- **7.3 MB** optimized WASM binary (-O3 -flto)
- Runs in **Node.js** and **browser** environments

## Quick Start

```js
import { readFileSync } from 'fs';
import createKicadDRC from './build-wasm/kicad_drc.mjs';

// Initialize the WASM module
const Module = await createKicadDRC();

// Read a .kicad_pcb file
const pcb = readFileSync('board.kicad_pcb', 'utf-8');

// Allocate string in WASM memory
const len = Module.lengthBytesUTF8(pcb) + 1;
const ptr = Module._malloc(len);
Module.stringToUTF8(pcb, ptr, len);

// Load PCB (returns 0 on success)
const rc = Module._kicad_load_pcb(ptr, 0);
Module._free(ptr);

// Run DRC
const violationCount = Module._kicad_run_drc();

// Get JSON results
const jsonPtr = Module._kicad_get_drc_results();
const results = JSON.parse(Module.UTF8ToString(jsonPtr));

console.log(results.violations);

// Clean up
Module._kicad_cleanup();
```

Run with threading support:

```bash
node --experimental-wasm-threads test/test-wasm.mjs board.kicad_pcb
```

## API Reference

The module exports four C functions, callable via `Module._<name>()`:

### `kicad_load_pcb(pcb_content, length)`

Load a KiCad PCB from in-memory content (s-expression format).

- **pcb_content** (`const char*`) — null-terminated `.kicad_pcb` file content
- **length** (`size_t`) — byte length, or 0 to use strlen
- **Returns** `0` on success, non-zero error code on failure

### `kicad_run_drc()`

Run all DRC checks on the loaded PCB. Must call `kicad_load_pcb()` first.

- **Returns** number of violations found, or `-1` on error

### `kicad_get_drc_results()`

Get DRC results as a JSON string. The returned pointer is valid until the next call to `kicad_run_drc()` or `kicad_cleanup()`.

- **Returns** JSON string following the `schemas.kicad.org/drc.v1.json` schema, or `NULL` on error

### `kicad_cleanup()`

Free all resources (board, DRC engine, results).

## Output Format

Results follow the [KiCad DRC JSON schema](https://schemas.kicad.org/drc.v1.json):

```json
{
  "source": "board.kicad_pcb",
  "coordinate_units": "mm",
  "violations": [
    {
      "type": "silk_overlap",
      "severity": "warning",
      "description": "Silkscreen overlap ...",
      "items": [
        {
          "description": "Text \"REF**\" on F.Silkscreen",
          "pos": { "x": 100.0, "y": 50.0 }
        }
      ]
    }
  ],
  "unconnected_items": [],
  "schematic_parity": []
}
```

## Building from Source

### Prerequisites

- **Emscripten SDK** (emsdk 5.0.0+)
- **CMake** 3.20+
- **KiCad source tree** with a completed native release build (provides generated headers)

### Steps

```bash
# 1. Clone KiCad source (if not already present)
git clone --depth 1 --branch 9.0 https://gitlab.com/kicad/code/kicad.git kicad-src

# 2. Build KiCad natively first (needed for generated headers)
cmake -S kicad-src -B kicad-src/build/release -DCMAKE_BUILD_TYPE=Release
cmake --build kicad-src/build/release -j$(nproc)

# 3. Install and activate Emscripten
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk && ./emsdk install 5.0.0 && ./emsdk activate 5.0.0
source emsdk_env.sh && cd ..

# 4. Configure WASM build
emcmake cmake -S kicad-drc-wasm -B kicad-drc-wasm/build-wasm

# 5. Build
cmake --build kicad-drc-wasm/build-wasm -j$(nproc)
```

Output files:
- `kicad-drc-wasm/build-wasm/kicad_drc.wasm` (7.3 MB)
- `kicad-drc-wasm/build-wasm/kicad_drc.mjs` (103 KB loader)
- `kicad-drc-wasm/build-wasm/kicad_drc.worker.mjs` (pthread worker)

## Known Limitations

- **Violation count differs from kicad-cli** — the WASM build reports 4 violations on the sample PCB vs. 6 from `kicad-cli`. The 2 missing are library parity warnings that require footprint library access and schematic data, which are not available in standalone mode.
- **No schematic parity checks** — schematic data (`testFootprints=false`) is not loaded, so footprint-vs-schematic checks are skipped.
- **No library parity provider** — `drc_test_provider_library_parity` is excluded (crashes in WASM static initialization due to a global `UNITS_PROVIDER`).
- **wxWidgets stubbed** — wxString and other wx types are minimal stubs; no GUI functionality is available.
- **Threading requires SharedArrayBuffer** — the browser must serve pages with `Cross-Origin-Opener-Policy: same-origin` and `Cross-Origin-Embedder-Policy: require-corp` headers.

## License

This project builds upon KiCad source code, which is licensed under the [GNU General Public License v3.0](https://www.gnu.org/licenses/gpl-3.0.html). See the [KiCad project](https://www.kicad.org/) for details.
