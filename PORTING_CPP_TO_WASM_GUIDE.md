# Porting Large C++ Codebases to WebAssembly: A Practical Guide

Lessons learned from porting KiCad's Design Rule Check (DRC) engine — 250+ source files, 7.3MB optimized WASM — to run in browsers and Node.js.

---

## Table of Contents

1. [Overview & Strategy](#1-overview--strategy)
2. [Toolchain Setup (Emscripten)](#2-toolchain-setup-emscripten)
3. [Dependency Analysis](#3-dependency-analysis)
4. [The Stubbing Strategy](#4-the-stubbing-strategy)
5. [Build System (CMake + Emscripten)](#5-build-system-cmake--emscripten)
6. [The C API Bridge](#6-the-c-api-bridge)
7. [JavaScript Integration](#7-javascript-integration)
8. [Threading (pthreads in WASM)](#8-threading-pthreads-in-wasm)
9. [Release Optimization](#9-release-optimization)
10. [Pitfalls & Hard-Won Lessons](#10-pitfalls--hard-won-lessons)
11. [Checklist](#11-checklist)

---

## 1. Overview & Strategy

### The Problem

You have a large C++ application (100k+ lines) and want to run a specific subset of its functionality in the browser. The codebase has deep dependencies on system libraries (GUI toolkits, native I/O, system APIs) that don't exist in WebAssembly.

### The Approach

Rather than rewriting, you **isolate** the functionality you need and **stub** everything else:

```
┌─────────────────────────────────────────────┐
│  Original C++ Application                   │
│  ┌───────────────────┐  ┌────────────────┐  │
│  │  Core Logic       │  │  GUI / System  │  │
│  │  (what you want)  │  │  (don't need)  │  │
│  └───────────────────┘  └────────────────┘  │
└─────────────────────────────────────────────┘
                    ▼
┌─────────────────────────────────────────────┐
│  WASM Module                                │
│  ┌───────────────────┐  ┌────────────────┐  │
│  │  Core Logic       │  │  Stub Headers  │  │
│  │  (real code)      │  │  + Linker Stubs│  │
│  └───────────────────┘  └────────────────┘  │
│  ┌───────────────────┐                      │
│  │  C API Bridge     │ ← JS calls this      │
│  └───────────────────┘                      │
└─────────────────────────────────────────────┘
```

### Key Principle

**You don't modify the original source files.** Instead you:
1. Create stub headers that satisfy `#include` chains
2. Create linker stubs that provide symbols for unreachable code paths
3. Write a thin C API that exposes your functionality to JavaScript
4. Selectively compile only the source files you need

---

## 2. Toolchain Setup (Emscripten)

### Installation

```bash
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest    # e.g., 5.0.0
./emsdk activate latest
source emsdk_env.sh       # MUST source before every build session
```

### Building with CMake

```bash
# Configure (replaces cmake with emcmake wrapper)
emcmake cmake -G "Unix Makefiles" /path/to/your/project

# Build
emmake make -j$(nproc)
```

The `emcmake` wrapper sets `CMAKE_TOOLCHAIN_FILE` to Emscripten's toolchain, which replaces `gcc`/`clang` with `emcc`/`em++`.

### Critical Emscripten Gotcha

**Never add `/usr/include` to the Emscripten include path.** Native libc headers conflict with Emscripten's sysroot headers. If you need headers from system packages (e.g., Boost), symlink only the specific directories:

```bash
# WRONG: -I/usr/include
# RIGHT: symlink specific headers into your stubs directory
ln -s /usr/include/boost /path/to/your/stubs/boost
```

---

## 3. Dependency Analysis

Before writing any code, map the dependency tree of the functionality you want to port.

### What to Map

1. **Entry point**: How is the feature invoked in the original code?
2. **Core files**: Which `.cpp` files contain the actual logic?
3. **Data model**: What classes represent the data being processed?
4. **I/O**: How does data get in and out?
5. **System dependencies**: What system libraries are pulled in transitively?

### Categorize Every Dependency

| Category | Action | Example |
|----------|--------|---------|
| **Must compile** | Include the real source file | Math library, data model, core algorithm |
| **Must stub (header)** | Create minimal header stubs | GUI toolkit classes referenced in headers |
| **Must stub (linker)** | Provide empty function bodies | Serialization methods you never call |
| **Can exclude** | Don't compile at all | Test files, plugins, optional features |

### The Transitive Dependency Problem

The hardest part isn't your core logic — it's the **transitive includes**. A data model header might include a GUI header which includes a platform header, etc. You need stubs for the entire include chain, not just the direct dependencies.

In our case (KiCad):
- DRC engine itself: clean, no GUI deps
- Board data model: includes wxString (GUI toolkit) in every header
- Result: needed 300+ wx stub headers just to satisfy includes

---

## 4. The Stubbing Strategy

### Two Kinds of Stubs

**Header stubs** satisfy the compiler (`#include` resolution, type definitions):

```cpp
// stubs/wx/string.h - Minimal wxString that wraps std::string
#pragma once
#include <string>
#include <cstdarg>

class wxString {
    std::string m_str;
public:
    wxString() = default;
    wxString(const char* s) : m_str(s ? s : "") {}
    wxString(const std::string& s) : m_str(s) {}

    const char* c_str() const { return m_str.c_str(); }
    size_t length() const { return m_str.length(); }
    bool empty() const { return m_str.empty(); }
    // ... minimum methods needed by included headers
};
```

**Linker stubs** satisfy the linker (unresolved symbols from code that compiles but is never called at runtime):

```cpp
// linker_stubs.cpp
// Functions referenced by linked object code but never actually called

void SomeGUIFunction(wxWindow* parent, const wxString& msg) {
    // Empty — this code path is never reached in WASM
}

SomeClass* SomeFactory::Create(int type) {
    return nullptr;  // Never called, but linker needs the symbol
}
```

### Header Stub Rules

1. **Match the original API surface** — if code does `obj.GetName()`, your stub must have `GetName()`
2. **Return types must compile** — return defaults, empty strings, zero values
3. **Class hierarchies must match** — if `Derived : Base` is used, stub both
4. **Enums must match values** — especially if used in `switch` or serialization
5. **Don't stub what you can exclude** — if a file isn't compiled, its headers don't need stubs

### Stub Organization

```
stubs/
├── wx/                    # GUI toolkit stubs
│   ├── string.h           # Most critical — used everywhere
│   ├── filename.h         # File path manipulation
│   ├── defs.h             # Type definitions
│   └── ...                # 300+ headers matching wx API
├── freetype2/             # Font library stubs
│   ├── ft2build.h
│   └── freetype.h
├── google/protobuf/       # Serialization library stubs
│   └── message.h
└── curl.h, fontconfig.h   # Other system library stubs
```

### Force-Include for Pervasive Types

If a type (like `wxString`) is assumed available in nearly every file via precompiled headers:

```cmake
# Force-include the stub so every translation unit sees it
set(FORCE_INCLUDE_FLAGS -include ${STUB_DIR}/wx/string.h)

# Apply to all targets that compile the original source
target_compile_options(my_lib PRIVATE ${FORCE_INCLUDE_FLAGS})
```

---

## 5. Build System (CMake + Emscripten)

### Project Structure

```cmake
cmake_minimum_required(VERSION 3.20)
project(my-wasm-lib LANGUAGES CXX C)
set(CMAKE_CXX_STANDARD 20)

set(ORIGINAL_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../original-project")

if(EMSCRIPTEN)
    # ═══ WASM BUILD ═══

    # Stubs MUST come first in include path to override system headers
    set(STUB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/stubs")

    set(INCLUDE_DIRS
        ${STUB_DIR}                              # 1. Your stubs (highest priority)
        ${CMAKE_CURRENT_SOURCE_DIR}/include      # 2. Your API headers
        ${ORIGINAL_SRC}/include                  # 3. Original project headers
        ${ORIGINAL_SRC}/libs/math/include        # 4. Original project libs
        # ... etc
    )

    # Compile definitions MUST match original build
    add_compile_definitions(
        NDEBUG
        MY_APP_FEATURE_FLAG          # Must match if it affects class layout
        MY_WASM=1                    # Your own marker for WASM-specific paths
    )

    add_compile_options(
        -O3 -flto                    # Optimization (critical for size)
        -fexceptions                 # If the code uses C++ exceptions
        -pthread                     # If the code uses threading
        -Wno-non-pod-varargs         # Common warning with stub types
    )
else()
    # ═══ NATIVE BUILD (for testing) ═══
    # Link against actual system libraries
endif()
```

### Compile Definitions Must Match

If the original project has compile definitions that affect **class layout** (e.g., conditional member variables), your WASM build **must use the same definitions**:

```cpp
// In original code:
class Board {
    int m_width;
#ifdef FEATURE_3D
    float m_height;    // Changes sizeof(Board)!
#endif
    int m_layers;
};
```

If you compile some files with `FEATURE_3D` and others without, you get memory corruption.

### Selective Source File Compilation

Don't glob entire directories. Explicitly list the files you need:

```cmake
set(MY_CORE_SOURCES
    ${ORIGINAL_SRC}/src/engine.cpp       # Core algorithm
    ${ORIGINAL_SRC}/src/data_model.cpp   # Data structures
    ${ORIGINAL_SRC}/src/parser.cpp       # File parser
    # Deliberately excluded:
    # ${ORIGINAL_SRC}/src/gui_dialog.cpp  # GUI - not needed
    # ${ORIGINAL_SRC}/src/cloud_sync.cpp  # Network - not needed
)

add_library(core_wasm STATIC ${MY_CORE_SOURCES})
target_include_directories(core_wasm PUBLIC ${INCLUDE_DIRS})
target_compile_options(core_wasm PRIVATE ${FORCE_INCLUDE_FLAGS})
```

### Self-Registering Components Need `--whole-archive`

If the original code uses the static registration pattern (common for plugins/providers):

```cpp
// In each provider file:
static REGISTER_PROVIDER<MyProvider> s_register;
// Constructor runs at static init time, adds to global registry
```

The linker normally strips these because nothing explicitly references them. Force inclusion:

```cmake
target_link_libraries(my_wasm_executable PRIVATE
    -Wl,--whole-archive
    provider_library          # Contains self-registering components
    -Wl,--no-whole-archive
    other_library             # Normal libraries
)
```

### Final Executable Link Flags

```cmake
set_target_properties(my_wasm_module PROPERTIES
    SUFFIX ".mjs"                    # Required for ES6 modules in Node.js
    LINK_FLAGS "\
        -sEXPORT_ES6=1 \
        -sMODULARIZE=1 \
        -sEXPORT_NAME=createMyModule \
        -sEXPORTED_FUNCTIONS=[\"_my_init\",\"_my_process\",\"_my_get_result\",\"_my_cleanup\",\"_malloc\",\"_free\"] \
        -sEXPORTED_RUNTIME_METHODS=[\"ccall\",\"cwrap\",\"UTF8ToString\",\"stringToUTF8\",\"lengthBytesUTF8\"] \
        -O3 -flto \
        -sALLOW_MEMORY_GROWTH=1 \
        -sINITIAL_MEMORY=67108864 \
        -sSTACK_SIZE=1048576 \
        -sFILESYSTEM=1 \
        -sENVIRONMENT=node,web,worker \
        -sNO_DISABLE_EXCEPTION_CATCHING \
        -sASSERTIONS=0 \
        -sSTACK_OVERFLOW_CHECK=0 \
        -pthread \
        -sPTHREAD_POOL_SIZE=4 \
    "
)
```

**Key flags explained:**

| Flag | Purpose |
|------|---------|
| `-sEXPORT_ES6=1` | Generate ES6 module (requires `.mjs` suffix) |
| `-sMODULARIZE=1` | Wrap in factory function (not global) |
| `-sEXPORT_NAME=createMyModule` | Name of the factory function |
| `-sEXPORTED_FUNCTIONS` | C functions callable from JS (prefix with `_`) |
| `-sEXPORTED_RUNTIME_METHODS` | Emscripten helpers exposed to JS |
| `-sALLOW_MEMORY_GROWTH=1` | Heap can grow beyond INITIAL_MEMORY |
| `-sINITIAL_MEMORY=67108864` | 64MB initial heap |
| `-sFILESYSTEM=1` | Enable virtual filesystem (if code uses fopen/etc.) |
| `-sENVIRONMENT=node,web,worker` | Target environments |
| `-sNO_DISABLE_EXCEPTION_CATCHING` | Preserve C++ exceptions |
| `-sPTHREAD_POOL_SIZE=4` | Pre-spawn N web workers for threads |

---

## 6. The C API Bridge

### Design Pattern

Expose your C++ functionality through a flat C API. This is the **only interface** between JavaScript and your code:

```c
// include/my_api.h
#pragma once
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int   my_load_data(const char* content, size_t length);
int   my_process(void);
const char* my_get_result(void);
void  my_cleanup(void);

#ifdef __cplusplus
}
#endif
```

### Implementation Pattern

```cpp
// src/api.cpp
#include "my_api.h"
#include <memory>
#include <string>

// Original project headers
#include <data_model.h>
#include <engine.h>
#include <parser.h>

// Global state (lives for the lifetime of the WASM module)
static std::unique_ptr<DataModel> g_model;
static std::string                g_result;

extern "C" {

int my_load_data(const char* content, size_t length)
{
    g_model.reset();
    g_result.clear();

    if (!content) return -1;

    try {
        std::string data(content, length ? length : strlen(content));
        g_model = Parser::Parse(data);
        return 0;
    } catch (const std::exception& e) {
        fprintf(stderr, "Parse error: %s\n", e.what());
        return -1;
    }
}

int my_process(void)
{
    if (!g_model) return -1;

    Engine engine(g_model.get());
    engine.Run();

    // Serialize result to JSON string
    g_result = engine.GetResultAsJSON();
    return engine.GetIssueCount();
}

const char* my_get_result(void)
{
    return g_result.empty() ? nullptr : g_result.c_str();
}

void my_cleanup(void)
{
    g_result.clear();
    g_model.reset();
}

} // extern "C"
```

### Key Design Decisions

- **String ownership**: Return `const char*` pointing to a `static std::string`. Document that the pointer is valid until the next call.
- **Memory**: Let C++ manage its own memory. JS only manages the input buffer.
- **Error handling**: Return error codes. Use `fprintf(stderr, ...)` for diagnostics (visible in browser console).
- **Initialization**: Lazy-init singletons on first use, not at module load time.

---

## 7. JavaScript Integration

### Calling WASM from JavaScript

```javascript
import createMyModule from './my_module.mjs';

// Initialize the WASM module (async — downloads + compiles WASM)
const Module = await createMyModule();

// Prepare input data
const inputData = "...your data...";
const len = Module.lengthBytesUTF8(inputData) + 1;  // +1 for null terminator
const ptr = Module._malloc(len);
Module.stringToUTF8(inputData, ptr, len);

// Call C functions
const loadResult = Module._my_load_data(ptr, 0);
Module._free(ptr);  // Free input buffer immediately after use

if (loadResult !== 0) {
    throw new Error(`Load failed: ${loadResult}`);
}

// Process
const count = Module._my_process();
console.log(`Found ${count} issues`);

// Get results (string lives in WASM heap — read it immediately)
const resultPtr = Module._my_get_result();
const jsonStr = Module.UTF8ToString(resultPtr);
const result = JSON.parse(jsonStr);

// Cleanup
Module._my_cleanup();
```

### Memory Management Rules

1. **Input strings**: JS allocates with `_malloc`, copies with `stringToUTF8`, frees with `_free`
2. **Output strings**: C++ owns the memory, JS reads with `UTF8ToString` (copies to JS heap)
3. **Never hold C pointers across calls** that might invalidate them

### Browser Requirements for Threading

If your WASM uses pthreads, the browser requires these HTTP headers:

```
Cross-Origin-Opener-Policy: same-origin
Cross-Origin-Embedder-Policy: require-corp
```

Without these, `SharedArrayBuffer` is disabled and threads will deadlock.

---

## 8. Threading (pthreads in WASM)

### How It Works

Emscripten implements pthreads using Web Workers + SharedArrayBuffer:
- Each pthread becomes a Web Worker
- All workers share the same WASM memory (via SharedArrayBuffer)
- `PTHREAD_POOL_SIZE=N` pre-spawns N workers at module load time

### When You Need It

- Original code uses `std::thread`, `std::async`, or a thread pool
- Algorithms use parallel execution (e.g., connectivity analysis)
- Libraries expect threading support (e.g., Intel TBB)

### Configuration

```cmake
# Compile flags (all translation units)
add_compile_options(-pthread)

# Link flags
-pthread
-sPTHREAD_POOL_SIZE=4    # Pre-spawn 4 workers
```

### Critical: Pre-spawn vs. Lazy-spawn

- `PTHREAD_POOL_SIZE=0`: Threads created on demand (may deadlock if main thread waits synchronously for worker to start)
- `PTHREAD_POOL_SIZE=4`: 4 workers pre-spawned at load time (safer, avoids deadlocks)

**Always use a non-zero pool size** if your code synchronously waits for thread results.

---

## 9. Release Optimization

### Size Reduction Techniques

Our results: **34MB → 7.3MB** (78% reduction)

| Technique | Impact |
|-----------|--------|
| `-O3` | Aggressive optimization, removes dead code |
| `-flto` (Link-Time Optimization) | Analyzes across translation units, eliminates unreachable functions |
| `-sASSERTIONS=0` | Removes Emscripten runtime checks |
| `-sSTACK_OVERFLOW_CHECK=0` | Removes stack guard code |
| No `-g` flag | Strips debug info |
| No `--emit-symbol-map` | Omits symbol mapping file |

### LTO Gotcha: Symbol Conflicts

When LTO is enabled, the linker sees all symbols across all translation units. This can cause conflicts between your stubs and standard library implementations:

```cpp
// Your stub:
void __libcpp_atomic_wait(...) noexcept {}

// libc++ (included via LTO) also defines this!
// Result: duplicate symbol error
```

**Fix: Use weak linkage on stubs:**

```cpp
__attribute__((weak))
void __libcpp_atomic_wait(void const volatile*, __cxx_contention_t) noexcept {}
```

The `weak` attribute lets the real implementation win if present.

### Memory Configuration

```cmake
-sINITIAL_MEMORY=67108864    # 64MB initial (tune to your needs)
-sALLOW_MEMORY_GROWTH=1      # Grow if needed (slight performance cost)
-sSTACK_SIZE=1048576          # 1MB stack (increase if deep recursion)
```

---

## 10. Pitfalls & Hard-Won Lessons

### 1. C-style Varargs Can't Handle C++ Objects

```cpp
// This CRASHES in WASM (and is technically UB everywhere):
wxString name = "test";
wxString result = wxString::Format("Name: %s", name);  // name is non-POD!
```

C-style `va_arg` with non-POD types is undefined behavior. It happens to work on x86 but crashes in WASM.

**Fix: Use C++17 template parameter packs:**

```cpp
template<typename... Args>
static wxString Format(const wxString& fmt, Args&&... args) {
    // Convert non-POD args to POD (e.g., wxString → const char*)
    char buf[4096];
    snprintf(buf, sizeof(buf), fmt.c_str(), toPOD(std::forward<Args>(args))...);
    return wxString(buf);
}
```

### 2. Static Initialization Order Fiasco

Global constructors run before `main()`. If a global constructor calls code that depends on another global not yet initialized → crash.

```cpp
// This crashes if wxString::Format isn't safe during static init:
static wxString VERSION = wxString::Format("v%d.%d", MAJOR, MINOR);
```

**Fix: Exclude the offending file and stub the symbols it provides.**

### 3. Struct Layout Must Match Exactly

If you use a stub header that defines a struct differently from the real one, and both stubs and real code access the same object, you get memory corruption.

```cpp
// Real header:
struct FT_FaceRec { long num_glyphs; long style_flags; ... };

// BAD stub:
typedef void* FT_Face;  // Code doing face->style_flags won't compile

// GOOD stub:
struct FT_FaceRec { long num_glyphs; long style_flags; ... };
typedef FT_FaceRec* FT_Face;
```

### 4. Include Order Matters Enormously

Your stubs directory must be **first** in the include path so it overrides system headers:

```cmake
set(INCLUDE_DIRS
    ${STUB_DIR}          # FIRST - overrides system headers
    ${PROJECT_INCLUDES}  # Second - your code
    ${ORIGINAL_SRC}      # Third - original project
)
```

### 5. `--whole-archive` Is Required for Self-Registration

If the original code uses static constructors for plugin/provider registration, those objects will be stripped by the linker unless forced:

```cmake
target_link_libraries(exe PRIVATE
    -Wl,--whole-archive
    providers_lib          # Self-registering components
    -Wl,--no-whole-archive
    everything_else
)
```

### 6. ES6 Module Requires `.mjs` Extension

When using `-sEXPORT_ES6=1`, the output must have a `.mjs` extension for Node.js to treat it as an ES module:

```cmake
set_target_properties(my_module PROPERTIES SUFFIX ".mjs")
```

### 7. Virtual Filesystem for File I/O

If the original code uses `fopen`/`fwrite`/`std::ifstream`, enable Emscripten's virtual filesystem:

```cmake
-sFILESYSTEM=1
```

This creates an in-memory filesystem. Temp files work, but there's no persistence.

### 8. Build Natively First

Always maintain a native build path. It's 10x easier to debug crashes natively than in WASM:

```cmake
if(EMSCRIPTEN)
    # WASM build with stubs
else()
    # Native build linking against real system libraries
    # Use this for debugging
endif()
```

---

## 11. Checklist

### Before You Start

- [ ] Identify the exact functionality you want to port
- [ ] Map the complete dependency tree (files, classes, libraries)
- [ ] Categorize each dependency: compile / stub-header / stub-linker / exclude
- [ ] Install Emscripten SDK

### Build Setup

- [ ] Create project structure: `src/`, `include/`, `stubs/`, `platform/`, `test/`
- [ ] Write CMakeLists.txt with `if(EMSCRIPTEN)` / `else()` dual build
- [ ] Stub headers: start with the most pervasive type (usually string class)
- [ ] Verify compile definitions match the original build
- [ ] Set up include path with stubs directory first

### Incremental Compilation

- [ ] Compile thirdparty libraries first (smallest dependency surface)
- [ ] Compile core/math libraries next
- [ ] Compile platform stubs
- [ ] Compile the main application libraries (largest effort)
- [ ] Create linker stubs for remaining unresolved symbols
- [ ] Link final WASM executable

### API & Testing

- [ ] Design C API (4-6 functions: init, load, process, get_result, cleanup)
- [ ] Write JavaScript test harness
- [ ] Test in Node.js: `node --experimental-wasm-threads test.mjs`
- [ ] Verify output matches native build

### Release

- [ ] Enable `-O3 -flto` for size optimization
- [ ] Set `-sASSERTIONS=0 -sSTACK_OVERFLOW_CHECK=0`
- [ ] Remove `-g` debug flags
- [ ] Use `__attribute__((weak))` on stubs that conflict with LTO libc++
- [ ] Tune `INITIAL_MEMORY` and `STACK_SIZE`
- [ ] Test in browser with COOP/COEP headers (if using threads)

---

## Appendix: File Layout Reference

```
my-wasm-project/
├── CMakeLists.txt              # Dual WASM/native build
├── include/
│   └── my_api.h                # C API header (extern "C")
├── src/
│   ├── api.cpp                 # C API implementation
│   ├── linker_stubs.cpp        # Empty function bodies for unresolved symbols
│   └── io_manager_wasm.cpp     # Simplified I/O (subset of plugins)
├── platform/
│   ├── platform_wasm.cpp       # Platform abstraction stubs
│   └── app_base_wasm.cpp       # Application singleton stub
├── stubs/
│   ├── wx/                     # GUI toolkit header stubs
│   │   ├── string.h            # String class (most critical)
│   │   ├── filename.h          # File path class
│   │   └── ...
│   ├── freetype2/              # Font library stubs
│   ├── google/protobuf/        # Serialization stubs
│   └── curl.h, etc.            # Other library stubs
├── test/
│   └── test-wasm.mjs           # Node.js test harness
├── build-wasm/                 # WASM build output directory
│   ├── my_module.wasm          # The compiled WebAssembly binary
│   └── my_module.mjs           # ES6 JavaScript glue code
└── build/                      # Native build (for debugging)
```
