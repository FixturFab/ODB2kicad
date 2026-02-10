# odb2kicad WASM — Product Requirements Document

## Overview

**Goal**: Compile the `odb2kicad` C++17 converter to WebAssembly so it can run in browsers and Node.js, enabling client-side ODB++ to KiCad conversion with zero server dependency.

**Why this is tractable**: The project has **zero external dependencies** — pure C++17 standard library only. The only POSIX-specific code is `<dirent.h>` / `<sys/stat.h>` for directory listing in a single file (`odb_parser.cpp`). Emscripten's virtual filesystem handles these POSIX calls natively.

**Target environments**: Browser (ES module) and Node.js (CommonJS/ESM)

**Input/Output**:
- **Input**: ODB++ directory tree loaded into Emscripten's virtual filesystem (MEMFS)
- **Output**: `.kicad_pcb` s-expression string returned to JavaScript

---

## Current State

- Working native C++17 CLI tool: `odb2kicad <odb-dir> [output.kicad_pcb]`
- 12 source files, ~2500 lines of C++
- Build system: CMake 3.10+
- POSIX calls limited to one file (`odb_parser.cpp`): `opendir`, `readdir`, `closedir`, `stat`
- Uses `std::regex` (supported by Emscripten)
- Uses `std::ifstream` / `std::ofstream` (supported via Emscripten MEMFS)
- Existing `emscripten-test/` with a hello-world WASM proof-of-concept already in repo
- Existing `kicad-drc-wasm/` with prior Emscripten CMake patterns to reference

---

## Stage 1: Refactor Core into a Library

### Objective
Separate the conversion logic from the CLI `main()` so WASM and CLI share the same library code.

### Tasks

#### 1.1 Create `src/odb2kicad_lib.h` — public C++ API header
```cpp
#pragma once
#include <string>

namespace odb2kicad {

struct ConvertResult {
    bool success;
    std::string kicad_pcb;   // output s-expression on success
    std::string error;       // error message on failure
};

// Convert from a filesystem path (used by CLI and WASM-with-MEMFS)
ConvertResult convert(const std::string& odbDirPath);

// Convert from in-memory file map: path -> contents
// (alternative API that avoids filesystem entirely)
ConvertResult convertFromMemory(
    const std::vector<std::pair<std::string, std::string>>& files
);

}
```

#### 1.2 Create `src/odb2kicad_lib.cpp` — implement the API
Extract the parse→transform→write pipeline from `main.cpp` into `convert()`. Use `std::ostringstream` to capture output as a string instead of writing to a file.

#### 1.3 Slim down `main.cpp`
Rewrite `main.cpp` to call `odb2kicad::convert()` and write the result. Should be ~15 lines.

#### 1.4 Update `CMakeLists.txt`
- Add a `odb2kicad_core` static library target containing all source files except `main.cpp`
- Link `odb2kicad` executable against `odb2kicad_core`
- This library target will be reused by the WASM build

#### 1.5 Verify native build still works
```bash
cmake -S odb2kicad -B odb2kicad/build
cmake --build odb2kicad/build -j$(nproc)
cd odb2kicad && bash test/test_simple.sh
```

**Success criteria**: All 60+ existing tests pass with the refactored code.

---

## Stage 2: Emscripten Build Configuration

### Objective
Add Emscripten WASM build support to CMakeLists.txt.

### Tasks

#### 2.1 Install Emscripten SDK (if not present)
```bash
cd /root
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk && ./emsdk install latest && ./emsdk activate latest
```

#### 2.2 Add WASM-specific CMake configuration
Extend `odb2kicad/CMakeLists.txt` with an `if(EMSCRIPTEN)` block:

```cmake
if(EMSCRIPTEN)
    # Build the WASM module instead of CLI executable
    add_executable(odb2kicad_wasm src/wasm_bindings.cpp)
    target_link_libraries(odb2kicad_wasm PRIVATE odb2kicad_core)

    set_target_properties(odb2kicad_wasm PROPERTIES
        SUFFIX ".mjs"
        LINK_FLAGS "\
            -s MODULARIZE=1 \
            -s EXPORT_ES6=1 \
            -s EXPORTED_RUNTIME_METHODS=['FS','callMain','ccall','cwrap'] \
            -s EXPORTED_FUNCTIONS=['_convert_odb','_malloc','_free'] \
            -s ALLOW_MEMORY_GROWTH=1 \
            -s ENVIRONMENT='web,node' \
            -s FORCE_FILESYSTEM=1 \
            -lembind"
    )
else()
    add_executable(odb2kicad src/main.cpp)
    target_link_libraries(odb2kicad PRIVATE odb2kicad_core)
endif()
```

#### 2.3 Verify Emscripten CMake configures
```bash
source /root/emsdk/emsdk_env.sh
emcmake cmake -S odb2kicad -B odb2kicad/build-wasm -DCMAKE_BUILD_TYPE=Release
```

**Success criteria**: CMake configures without errors.

---

## Stage 3: WASM Bindings (C → JS bridge)

### Objective
Create the C/JS bridge that exposes the converter to JavaScript.

### Tasks

#### 3.1 Create `src/wasm_bindings.cpp`
Two API approaches (implement both):

**Approach A — Filesystem-based** (simple, uses Emscripten MEMFS):
```cpp
#include <emscripten/bind.h>
#include "odb2kicad_lib.h"

std::string convert_odb(const std::string& odbDirPath) {
    auto result = odb2kicad::convert(odbDirPath);
    if (!result.success) {
        return "ERROR: " + result.error;
    }
    return result.kicad_pcb;
}

EMSCRIPTEN_BINDINGS(odb2kicad) {
    emscripten::function("convertOdb", &convert_odb);
}
```

JavaScript caller writes files to MEMFS first, then calls `convertOdb("/odb")`.

**Approach B — Pure memory** (no filesystem, for browser simplicity):
```cpp
std::string convert_odb_from_json(const std::string& filesJson) {
    // Parse JSON map of {path: contents}, call convertFromMemory()
}
```

#### 3.2 Compile WASM module
```bash
source /root/emsdk/emsdk_env.sh
emmake cmake --build odb2kicad/build-wasm -j$(nproc)
```

**Success criteria**: `odb2kicad_wasm.mjs` + `odb2kicad_wasm.wasm` files produced.

---

## Stage 4: JavaScript/TypeScript Wrapper

### Objective
Create a clean JS/TS API that hides WASM initialization and MEMFS plumbing.

### Tasks

#### 4.1 Create `wasm/package.json`
```json
{
    "name": "odb2kicad-wasm",
    "version": "0.1.0",
    "type": "module",
    "main": "dist/index.mjs",
    "types": "dist/index.d.ts",
    "files": ["dist/"]
}
```

#### 4.2 Create `wasm/src/index.ts` — high-level API
```typescript
export interface ConvertResult {
    success: boolean;
    kicadPcb?: string;
    error?: string;
}

export interface OdbFile {
    path: string;      // e.g. "matrix/matrix"
    content: string;   // file content
}

/**
 * Convert ODB++ files to KiCad .kicad_pcb format.
 * Accepts an array of {path, content} file entries representing
 * the ODB++ directory tree.
 */
export async function convertOdb(files: OdbFile[]): Promise<ConvertResult>;

/**
 * Convert from a .tgz or .zip archive buffer.
 * Extracts in-memory, then converts.
 */
export async function convertOdbArchive(buffer: ArrayBuffer): Promise<ConvertResult>;
```

#### 4.3 Implement MEMFS file loading
The wrapper:
1. Initializes the WASM module (lazy, cached)
2. Writes input files to Emscripten MEMFS under `/odb/`
3. Calls `convertOdb("/odb")`
4. Reads and returns the result string
5. Cleans up MEMFS

#### 4.4 Create TypeScript type declarations
Full `.d.ts` for the public API.

---

## Stage 5: Testing

### Objective
Verify the WASM module produces identical output to the native CLI.

### Tasks

#### 5.1 Create `wasm/test/test-node.mjs` — Node.js integration test
```javascript
import { convertOdb } from '../dist/index.mjs';
import { readdir, readFile } from 'fs/promises';
import { join } from 'path';

// Recursively read ODB++ directory into file array
async function loadOdbDir(dir, base = '') { ... }

// Test against samples/odb-output
const files = await loadOdbDir('../samples/odb-output');
const result = await convertOdb(files);
assert(result.success);
assert(result.kicadPcb.includes('kicad_pcb'));
assert(result.kicadPcb.includes('net "GND"') || result.kicadPcb.includes('(net "GND")'));
```

#### 5.2 Compare WASM output to native CLI output
```bash
# Native
./odb2kicad/build/odb2kicad samples/odb-output > /tmp/native.kicad_pcb

# WASM (Node.js)
node wasm/test/test-node.mjs samples/odb-output > /tmp/wasm.kicad_pcb

# Diff
diff /tmp/native.kicad_pcb /tmp/wasm.kicad_pcb
```

**Success criteria**: Output is byte-identical between native and WASM builds.

#### 5.3 Run existing smoke tests against WASM output
Adapt `test/test_simple.sh` assertions to run against the WASM-produced output.

#### 5.4 Browser smoke test
Create a minimal `wasm/test/browser.html`:
- File input for ODB++ .tgz upload
- "Convert" button
- Textarea showing `.kicad_pcb` output
- Download link for result

---

## Stage 6: Archive Extraction Support

### Objective
Accept `.tgz` and `.zip` ODB++ archives directly, not just pre-extracted directories.

### Tasks

#### 6.1 Evaluate archive extraction options
Options (pick one):
- **pako + untar** (JS-side, lightweight, .tgz only)
- **fflate** (JS-side, ~29KB, supports .tgz and .zip)
- **libarchive compiled to WASM** (C-side, heavier but comprehensive)

Recommended: **fflate** — small, fast, handles both formats in pure JS.

#### 6.2 Add archive extraction to JS wrapper
```typescript
import { unzipSync, gunzipSync } from 'fflate';

export async function convertOdbArchive(buffer: ArrayBuffer): Promise<ConvertResult> {
    const files = extractArchive(buffer);  // → OdbFile[]
    return convertOdb(files);
}
```

#### 6.3 Test with real ODB++ archives from EDA tools

**Success criteria**: Can load `.tgz` archives produced by Altium, KiCad, and other EDA tools.

---

## Stage 7: Optimize and Package

### Objective
Minimize WASM binary size and publish.

### Tasks

#### 7.1 Optimize WASM binary
- Compile with `-O3` or `-Oz` (size-optimized)
- Use `wasm-opt` from Binaryen for further size reduction
- Enable `-flto` (link-time optimization)
- Measure binary size target: aim for <500KB `.wasm`

#### 7.2 Bundle and publish
```bash
cd wasm && npm pack  # creates odb2kicad-wasm-0.1.0.tgz
npm publish          # when ready
```

#### 7.3 Create README with usage examples
- Node.js usage
- Browser usage with file upload
- CDN usage via unpkg/esm.sh

---

## Technical Notes

### Why this should be straightforward

| Factor | Status |
|--------|--------|
| External C/C++ dependencies | **None** — pure C++17 stdlib |
| POSIX filesystem calls | **Minimal** — `opendir`/`readdir`/`stat` in one file; Emscripten MEMFS handles these |
| `std::regex` | **Supported** by Emscripten |
| `std::ifstream` / `std::ofstream` | **Supported** via Emscripten MEMFS |
| `std::cout` / `std::cerr` | **Supported** — maps to `console.log` |
| Network calls | **None** |
| Threading | **None** — single-threaded |
| Dynamic linking | **None** — static build |
| Code size | **~2500 lines** — small WASM binary expected |

### POSIX calls that Emscripten handles

All POSIX calls are in `odb_parser.cpp` (lines 13-39):
```cpp
#include <dirent.h>      // opendir, readdir, closedir
#include <sys/stat.h>    // stat, S_ISDIR, S_ISREG
```
Emscripten provides full POSIX filesystem emulation via MEMFS. These calls will work as-is when `FORCE_FILESYSTEM=1` is set. **No code changes needed** for these.

### `std::regex` consideration

`symbols.cpp` uses `std::regex` extensively. Emscripten supports `<regex>` but it adds ~100-200KB to binary size. If size becomes a concern, these could be replaced with manual string parsing in a future optimization pass. Not blocking.

### Memory model

Emscripten defaults to 16MB initial memory. ODB++ files for typical boards are <10MB. `ALLOW_MEMORY_GROWTH=1` ensures large boards don't OOM. No special memory management needed.

---

## Risks & Mitigations

| Risk | Likelihood | Mitigation |
|------|-----------|------------|
| Emscripten `std::regex` bloat | Medium | Replace with manual parsing if >500KB |
| Large ODB++ boards OOM in browser | Low | `ALLOW_MEMORY_GROWTH=1`, warn on >100MB input |
| MEMFS performance for many small files | Low | ODB++ typically has <50 files; MEMFS is fine |
| Browser `Worker` needed for large boards | Medium | Document that long conversions should use a Worker |

---

## Checkpoints

- [ ] **Stage 1**: Core refactored into library; `test_simple.sh` passes
- [ ] **Stage 2**: `emcmake cmake` configures without errors
- [ ] **Stage 3**: `.mjs` + `.wasm` files produced; basic call works in Node.js
- [ ] **Stage 4**: TypeScript wrapper with clean `convertOdb()` API
- [ ] **Stage 5**: WASM output byte-identical to native; browser demo works
- [ ] **Stage 6**: Archive extraction (.tgz/.zip) works end-to-end
- [ ] **Stage 7**: Optimized binary <500KB; npm package published

---

## File Structure After Completion

```
odb2kicad/
├── odb2kicad/
│   ├── CMakeLists.txt              # Updated: library target + if(EMSCRIPTEN) block
│   ├── src/
│   │   ├── main.cpp                # Slimmed CLI entry point
│   │   ├── odb2kicad_lib.h         # NEW: public C++ API
│   │   ├── odb2kicad_lib.cpp       # NEW: convert() implementation
│   │   ├── wasm_bindings.cpp       # NEW: Emscripten embind bridge
│   │   ├── odb_parser/             # Unchanged
│   │   ├── model/                  # Unchanged
│   │   ├── writer/                 # Unchanged
│   │   └── util/                   # Unchanged
│   ├── build/                      # Native build
│   └── build-wasm/                 # WASM build output
│       ├── odb2kicad_wasm.mjs
│       └── odb2kicad_wasm.wasm
├── wasm/                           # NEW: JS/TS package
│   ├── package.json
│   ├── src/
│   │   └── index.ts                # High-level JS API
│   ├── dist/
│   │   ├── index.mjs
│   │   ├── index.d.ts
│   │   ├── odb2kicad_wasm.mjs
│   │   └── odb2kicad_wasm.wasm
│   └── test/
│       ├── test-node.mjs
│       └── browser.html
├── samples/                        # Unchanged
└── PRD-wasm.md                     # This document
```
