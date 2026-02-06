# KiCad CLI to WebAssembly - Product Requirements Document

## Overview

**Project Goal**: Port KiCad's command-line interface (kicad-cli) functionality to WebAssembly, enabling DRC (Design Rule Check) and STEP export to run in browsers and Node.js.

**Priority Order**:
1. DRC (Design Rule Check) - Primary focus
2. STEP Export - Secondary, leverage opencascade.js

**Target Environments**: Browser and Node.js (universal module)

**Binary Size Constraint**: None - functionality over size

---

## Stage 1: Native Build Foundation

### Objective
Clone KiCad source and build kicad-cli natively to verify baseline functionality.

### Tasks

#### 1.1 Install Build Dependencies
```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential cmake ninja-build git \
    libwxgtk3.2-dev libwxgtk-webview3.2-dev \
    libboost-all-dev libglew-dev libglm-dev libcairo2-dev \
    libcurl4-openssl-dev libprotobuf-dev protobuf-compiler \
    libocct-modeling-algorithms-dev libocct-modeling-data-dev \
    libocct-data-exchange-dev libocct-visualization-dev \
    python3-dev swig libngspice0-dev
```

**Note**: If wxgtk3.2 packages are not available, try wxgtk3.0 variants or build wxWidgets from source.

#### 1.2 Clone KiCad Source
```bash
cd /root/kicad-test
git clone --depth 1 https://gitlab.com/kicad/code/kicad.git kicad-src
```

#### 1.3 Configure CMake
```bash
cd /root/kicad-test/kicad-src
mkdir -p build/release
cd build/release
cmake -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DKICAD_SCRIPTING_WXPYTHON=OFF \
    -DKICAD_BUILD_I18N=OFF \
    -DKICAD_BUILD_QA_TESTS=OFF \
    -DKICAD_INSTALL_DEMOS=OFF \
    ../..
```

#### 1.4 Build kicad-cli
```bash
cd /root/kicad-test/kicad-src/build/release
ninja kicad-cli
```

**Expected Output**: Successful compilation of kicad-cli binary

#### 1.5 Get Sample KiCad Files
Download or create sample .kicad_pcb files for testing:
```bash
mkdir -p /root/kicad-test/samples
# Download a sample PCB from KiCad demos or create a minimal one
```

#### 1.6 Verify DRC Functionality
```bash
cd /root/kicad-test/kicad-src/build/release
./kicad-cli version
./kicad-cli pcb drc --output /tmp/drc-report.json --format json /root/kicad-test/samples/test.kicad_pcb
```

**Success Criteria**:
- `kicad-cli version` prints version information
- DRC command produces a JSON report file

#### 1.7 Verify STEP Export
```bash
./kicad-cli pcb export step --output /tmp/output.step /root/kicad-test/samples/test.kicad_pcb
```

**Success Criteria**: STEP file is created (may be minimal without 3D models)

---

## Stage 2: Emscripten Toolchain Validation

### Objective
Install and verify Emscripten SDK works correctly for C++ to WASM compilation.

### Tasks

#### 2.1 Install Emscripten SDK
```bash
cd /root/kicad-test
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
```

#### 2.2 Create Test Directory
```bash
mkdir -p /root/kicad-test/emscripten-test
```

#### 2.3 Create Simple Test Program
Create `/root/kicad-test/emscripten-test/test.cpp`:
```cpp
#include <iostream>
#include <emscripten/emscripten.h>

extern "C" {
    EMSCRIPTEN_KEEPALIVE
    int add(int a, int b) {
        return a + b;
    }

    EMSCRIPTEN_KEEPALIVE
    const char* greet() {
        return "Hello from WebAssembly!";
    }
}

int main() {
    std::cout << "WASM module initialized" << std::endl;
    return 0;
}
```

#### 2.4 Compile with Emscripten
```bash
cd /root/kicad-test/emscripten-test
source /root/kicad-test/emsdk/emsdk_env.sh
em++ test.cpp -o test.js \
    -s EXPORTED_FUNCTIONS='["_add", "_greet", "_main"]' \
    -s EXPORTED_RUNTIME_METHODS='["ccall", "cwrap"]' \
    -s MODULARIZE=1 \
    -s EXPORT_ES6=1
```

#### 2.5 Create Node.js Test Script
Create `/root/kicad-test/emscripten-test/test-node.mjs`:
```javascript
import createModule from './test.js';

const Module = await createModule();
console.log('Add result:', Module._add(5, 3));
console.log('Greet:', Module.ccall('greet', 'string', [], []));
```

#### 2.6 Test in Node.js
```bash
cd /root/kicad-test/emscripten-test
node test-node.mjs
```

**Success Criteria**:
- WASM compiles without errors
- Node.js can load and call exported functions
- Output shows correct addition result and greeting

---

## Stage 3: Analyze KiCad Dependencies

### Objective
Map the dependency tree for DRC and STEP export to understand what needs to be isolated.

### Tasks

#### 3.1 Analyze kicad-cli Entry Point
Examine `/root/kicad-test/kicad-src/kicad/kicad_cli.cpp`:
- Document the APP_KICAD_CLI class
- List all #includes
- Identify wxWidgets dependencies

#### 3.2 Analyze DRC Command
Examine `/root/kicad-test/kicad-src/kicad/cli/command_pcb_drc.cpp`:
- Document how DRC is invoked
- List dependencies on BOARD, DRC_ENGINE
- Identify what settings/config are needed

#### 3.3 Map DRC Engine Dependencies
Examine `/root/kicad-test/kicad-src/pcbnew/drc/`:
- List all .cpp and .h files
- Document class hierarchy (DRC_ENGINE, DRC_TEST_PROVIDER, etc.)
- Identify external library dependencies

#### 3.4 Analyze BOARD Data Structure
Examine `/root/kicad-test/kicad-src/pcbnew/board.cpp`:
- Document the BOARD class
- List contained objects (FOOTPRINT, PCB_TRACK, ZONE, etc.)
- Identify serialization/deserialization code

#### 3.5 Analyze S-Expression Parser
Examine `/root/kicad-test/kicad-src/common/`:
- `richio.cpp` - File I/O
- `string_utf8_map.cpp` - String handling
- PCB file parser location

#### 3.6 Create Dependency Report
Create `/root/kicad-test/DEPENDENCY_ANALYSIS.md` documenting:
- Full include tree for DRC
- wxWidgets touch points
- Boost dependencies
- OpenCascade dependencies (for STEP)
- Candidates for stubbing

---

## Stage 4: Create Isolated DRC Library

### Objective
Extract DRC functionality into a standalone library that can be compiled without wxWidgets.

### Tasks

#### 4.1 Create Project Structure
```bash
mkdir -p /root/kicad-test/kicad-drc-wasm/{src,include,stubs,test}
```

#### 4.2 Create wxWidgets Stubs
Create minimal stub implementations for required wxWidgets types:
- wxString (or use std::string)
- wxPoint, wxSize (geometric types)
- wxArrayString
- Logging macros

#### 4.3 Copy Required Source Files
Copy and adapt necessary files from KiCad:
- DRC engine files
- BOARD data model
- S-expression parser
- Common utilities

#### 4.4 Create CMakeLists.txt
Create `/root/kicad-test/kicad-drc-wasm/CMakeLists.txt` for both native and Emscripten builds.

#### 4.5 Create API Header
Create `/root/kicad-test/kicad-drc-wasm/include/kicad_drc_api.h`:
```cpp
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Load a KiCad PCB file from memory
int kicad_load_pcb(const char* pcb_content, size_t length);

// Run DRC checks on loaded PCB
int kicad_run_drc(void);

// Get DRC results as JSON string
const char* kicad_get_drc_results(void);

// Free resources
void kicad_cleanup(void);

#ifdef __cplusplus
}
#endif
```

#### 4.6 Implement API
Implement the C API wrapping the DRC engine.

#### 4.7 Test Native Build
```bash
cd /root/kicad-test/kicad-drc-wasm
mkdir build && cd build
cmake ..
make
./test_drc /root/kicad-test/samples/test.kicad_pcb
```

**Success Criteria**: Isolated DRC library compiles and runs natively

---

## Stage 5a: Configure WASM Build System

### Objective
Ensure the unified CMakeLists.txt works for both native and WASM builds via `if(EMSCRIPTEN)`.

### Tasks

#### 5a.1 Verify cmake configures successfully with emcmake
```bash
cd /root/kicad-test/kicad-drc-wasm
source /root/kicad-test/emsdk/emsdk_env.sh
emcmake cmake -B build-wasm -DCMAKE_BUILD_TYPE=Release
```

**Success Criteria**: `cmake` completes without errors

---

## Stage 5b: Compile Thirdparty + Core Libraries

### Objective
Build the pure C++ libraries that have minimal/no wx dependencies.

### Tasks

#### 5b.1 Build thirdparty and core targets
Build targets: `clipper2`, `fmt_lib`, `delaunator`, `core`, `kimath`, `sexpr`, `json_schema_validator`

```bash
cd /root/kicad-test/kicad-drc-wasm/build-wasm
emmake make clipper2 fmt_lib delaunator core kimath sexpr json_schema_validator
```

#### 5b.2 Fix any compilation errors in these targets

**Success Criteria**: All 7 targets produce `.a` files

---

## Stage 5c: Compile kiplatform_stubs + kicommon_wasm

### Objective
Build the platform stubs and the large kicommon_wasm library (selected subset of libkicommon.so).

### Tasks

#### 5c.1 Build kiplatform_stubs
```bash
emmake make kiplatform_stubs
```

#### 5c.2 Build kicommon_wasm
```bash
emmake make kicommon_wasm
```

#### 5c.3 Fix wx stub compilation errors iteratively
This is the hardest target (~140 source files with heavy wx usage). Add missing methods/classes to stubs in `stubs/wx/` as needed.

**Success Criteria**: `kicommon_wasm` produces a `.a` file

---

## Stage 5d: Compile gal_wasm

### Objective
Build the GAL (Graphics Abstraction Layer) subset needed by DRC providers.

### Tasks

#### 5d.1 Build gal_wasm
```bash
emmake make gal_wasm
```

#### 5d.2 Fix any OpenGL stub issues or eliminate GL-specific code

**Success Criteria**: `gal_wasm` produces a `.a` file

---

## Stage 5e: Compile pcbcommon_wasm + drc_providers

### Objective
Build the PCB data model, DRC engine, and test providers.

### Tasks

#### 5e.1 Build pcbcommon_wasm
```bash
emmake make pcbcommon_wasm
```

#### 5e.2 Build drc_providers
```bash
emmake make drc_providers
```

#### 5e.3 Fix any compilation errors

**Success Criteria**: Both targets produce `.a` files

---

## Stage 5f: Link Final WASM Executable

### Objective
Link all compiled targets into a single WASM module.

### Tasks

#### 5f.1 Build final executable
```bash
emmake make kicad_drc
```

#### 5f.2 Fix any linker errors (undefined symbols, missing implementations)

**Success Criteria**: `kicad_drc.mjs` + `kicad_drc.wasm` files are produced

---

## Stage 5g: Test WASM Module in Node.js

### Objective
Verify the WASM module works end-to-end.

### Tasks

#### 5g.1 Create test script
Create `test/test-wasm.mjs` that loads the WASM module.

#### 5g.2 Load sample PCB and run DRC
```bash
node test/test-wasm.mjs /root/kicad-test/samples/test.kicad_pcb
```

#### 5g.3 Verify JSON output matches native DRC results

**Success Criteria**: DRC runs in Node.js via WASM and produces violations

---

## Stage 6: Add STEP Export

### Objective
Add STEP export functionality using opencascade.js.

### Tasks

#### 6.1 Install opencascade.js
```bash
cd /root/kicad-test/kicad-drc-wasm
npm init -y
npm install opencascade.js
```

#### 6.2 Analyze KiCad STEP Export
Study `/root/kicad-test/kicad-src/pcbnew/exporters/step_pcb_model.cpp`:
- How board outline is generated
- How footprints are positioned
- How 3D models are loaded

#### 6.3 Create Geometry Extraction
Extract PCB geometry from BOARD:
- Board outline as polygon
- Pad positions and shapes
- Via positions
- Component placement

#### 6.4 Implement STEP Generation
Use opencascade.js to:
- Create board shape from outline
- Add component models (simplified boxes if needed)
- Export as STEP

#### 6.5 Create API
Add to `/root/kicad-test/kicad-drc-wasm/include/kicad_drc_api.h`:
```cpp
// Export loaded PCB as STEP file
int kicad_export_step(char** output, size_t* length);
```

#### 6.6 Test STEP Export
Verify generated STEP files can be opened in 3D viewers.

**Success Criteria**:
- STEP files generate from PCB data
- Files open in external 3D viewers
- Board shape is accurate

---

## Stage 7: Package and Document

### Objective
Create a distributable npm package.

### Tasks

#### 7.1 Create Package Structure
```
kicad-wasm/
├── package.json
├── README.md
├── dist/
│   ├── kicad-wasm.js
│   ├── kicad-wasm.wasm
│   └── kicad-wasm.d.ts
├── examples/
│   ├── browser/
│   └── node/
└── src/
```

#### 7.2 Create TypeScript Types
Full type definitions for all exported functions.

#### 7.3 Create Examples
- Browser example with file upload
- Node.js CLI example
- Integration test suite

#### 7.4 Write Documentation
- API reference
- Usage examples
- Build instructions

---

## Key Technical Challenges

### Challenge 1: wxWidgets Dependency
**Problem**: kicad-cli uses wxAppConsole which requires wxWidgets
**Solution**:
- Extract core functionality without wx wrapper
- Create stub types for wx primitives (wxString → std::string, etc.)
- Replace wx logging with console output

### Challenge 2: Settings/Configuration
**Problem**: KiCad uses complex settings management
**Solution**:
- Hardcode sensible defaults for DRC rules
- Allow rule configuration via JSON input
- Skip project-specific settings

### Challenge 3: File I/O
**Problem**: Native filesystem access
**Solution**:
- Use Emscripten virtual filesystem
- Accept PCB content as string input
- Return results as JSON/strings

### Challenge 4: Large Codebase
**Problem**: KiCad is a large monolithic codebase
**Solution**:
- Incrementally extract needed code
- Start with minimal subset
- Add features as needed

---

## File Structure After Completion

```
/root/kicad-test/
├── PRD.md                          # This document
├── DEPENDENCY_ANALYSIS.md          # Dependency analysis
├── kicad-src/                      # Original KiCad source
│   └── ...
├── emsdk/                          # Emscripten SDK
│   └── ...
├── emscripten-test/                # Simple Emscripten tests
│   ├── test.cpp
│   ├── test.js
│   └── test-node.mjs
├── samples/                        # Sample KiCad files
│   └── test.kicad_pcb
└── kicad-drc-wasm/                 # The WASM library
    ├── CMakeLists.txt
    ├── package.json
    ├── include/
    │   └── kicad_drc_api.h
    ├── src/
    │   ├── api.cpp
    │   ├── drc/
    │   ├── board/
    │   └── parser/
    ├── stubs/
    │   └── wx_stubs.h
    ├── wrapper/
    │   └── kicad-drc.ts
    ├── test/
    │   ├── test-native.cpp
    │   └── test-wasm.mjs
    ├── build/                      # Native build
    └── build-wasm/                 # WASM build
```

---

## Checkpoints

After each stage, verify:

- [x] **Stage 1**: `kicad-cli version` works, DRC produces output
- [x] **Stage 2**: Simple WASM runs in Node.js and browser
- [x] **Stage 3**: DEPENDENCY_ANALYSIS.md complete with wxWidgets touch points
- [x] **Stage 4**: Isolated DRC compiles and runs natively
- [x] **Stage 5a**: WASM cmake configures without errors
- [x] **Stage 5b**: Thirdparty + core libraries compile (7 `.a` files)
- [ ] **Stage 5c**: kiplatform_stubs + kicommon_wasm compile
- [ ] **Stage 5d**: gal_wasm compiles
- [ ] **Stage 5e**: pcbcommon_wasm + drc_providers compile
- [ ] **Stage 5f**: WASM executable links successfully
- [ ] **Stage 5g**: DRC runs in Node.js via WASM
- [ ] **Stage 6**: STEP export works via opencascade.js
- [ ] **Stage 7**: npm package ready for distribution

---

## Resources

- KiCad GitLab: https://gitlab.com/kicad/code/kicad
- KiCad Dev Docs: https://dev-docs.kicad.org/en/build/
- Emscripten: https://emscripten.org/docs/
- opencascade.js: https://github.com/donalffons/opencascade.js
- KiCanvas (reference): https://github.com/theacodes/kicanvas
