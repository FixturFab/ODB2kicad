# KiCad DRC/STEP WASM - Dependency Analysis

## Overview

This document maps the full dependency tree for KiCad's DRC (Design Rule Check) and STEP export functionality, identifying what needs to be isolated, stubbed, or replaced for WebAssembly compilation.

**Analysis date**: 2026-02-06
**KiCad version**: 9.99.0 (development)
**Source**: `/root/kicad-test/kicad-src`

---

## 1. kicad-cli Entry Point Architecture

### File: `kicad/kicad_cli.cpp`

**Class hierarchy:**
```
wxAppConsole
  └── APP_KICAD_CLI    (wxApp entry point)
        └── PGM_KICAD  (program logic, inherits PGM_BASE)
              └── KIWAY (module loader, dispatches to KIFACE modules)
```

**Initialization flow:**
1. `IMPLEMENT_APP_CONSOLE(APP_KICAD_CLI)` - wxWidgets app macro creates `main()`
2. `APP_KICAD_CLI::OnInit()` → `PGM_KICAD::OnPgmInit()` - initializes settings, paths, library tables
3. `APP_KICAD_CLI::OnRun()` → `PGM_KICAD::OnPgmRun()` - parses CLI args, dispatches commands
4. Commands call `KIWAY::ProcessJob(FACE_PCB, job)` which loads `_pcbnew.kiface` shared library

**Command dispatch:**
- Uses `argparse` library for CLI argument parsing
- Commands registered as static objects in a `commandStack` tree
- `CLI::COMMAND::Perform(KIWAY&)` is the entry to each command
- `CLI::PCB_DRC_COMMAND::doPerform(KIWAY&)` creates `JOB_PCB_DRC` and calls `KIWAY::ProcessJob()`

**Key includes from kicad_cli.cpp:**
```cpp
// wxWidgets
#include <wx/filename.h>      // File path handling
#include <wx/log.h>           // Logging
#include <wx/stdpaths.h>      // Standard paths
#include <wx/wxcrtvararg.h>   // wxPrintf
#include <wx/app.h>           // wxAppConsole

// KiCad core
#include <kiway.h>            // Module loading/dispatch
#include <settings/settings_manager.h>
#include <settings/kicad_settings.h>
#include <kiface_base.h>      // KIFACE interface
#include <build_version.h>
#include <kiplatform/app.h>
#include <kiplatform/environment.h>
#include <locale_io.h>
```

### KIWAY Module Loading

The `KIWAY` class loads `.kiface` shared libraries (DLLs) dynamically:
- `KIWAY::ProcessJob(FACE_T aFace, JOB* aJob)` loads the kiface for a given face type
- `FACE_PCB` loads `_pcbnew.kiface` which contains all PCB functionality
- The kiface is loaded via `dlopen()`/`LoadLibrary()` and calls a `KIFACE_GETTER` function
- **WASM impact**: Dynamic library loading won't work; must statically link pcbnew code

---

## 2. DRC Command Flow

### File: `kicad/cli/command_pcb_drc.cpp`

**DRC invocation chain:**
```
CLI::PCB_DRC_COMMAND::doPerform(KIWAY& aKiway)
  → creates JOB_PCB_DRC with settings (format, severity, units, etc.)
  → aKiway.ProcessJob(KIWAY::FACE_PCB, drcJob)
    → loads _pcbnew.kiface
    → PCBNEW_KIFACE::ProcessJob(JOB_PCB_DRC)
      → loads BOARD from .kicad_pcb file
      → creates DRC_ENGINE
      → DRC_ENGINE::InitEngine()  (loads rules)
      → DRC_ENGINE::RunTests()    (runs all test providers)
      → DRC_REPORT::WriteJsonReport() or WriteTextReport()
```

**JOB_PCB_DRC fields:**
- `m_filename` - input PCB file path
- `m_format` - REPORT or JSON
- `m_units` - MM, INCH, or MILS
- `m_severity` - error/warning/exclusion flags
- `m_reportAllTrackErrors` - report all or just first per track
- `m_parity` - test schematic parity
- `m_refillZones` - refill zones before DRC
- `m_exitCodeViolations` - nonzero exit on violations

---

## 3. DRC Engine Dependencies

### Files in `pcbnew/drc/`

| File | Purpose |
|------|---------|
| `drc_engine.h/cpp` | Main DRC engine - loads rules, runs providers |
| `drc_test_provider.h/cpp` | Base class for test providers |
| `drc_rule.h/cpp` | DRC rule definitions |
| `drc_rule_condition.h/cpp` | Rule condition expressions |
| `drc_rule_parser.h/cpp` | Custom DRC rule file parser |
| `drc_item.h/cpp` | DRC violation items |
| `drc_report.h/cpp` | Output report generation (JSON/text) |
| `drc_rtree.h` | R-tree spatial index for clearance checks |
| `drc_cache_generator.h/cpp` | Generates DRC spatial caches |
| `drc_creepage_utils.h/cpp` | Creepage distance calculations |
| `drc_interactive_courtyard_clearance.h/cpp` | Interactive courtyard checks |
| `drc_length_report.h` | Length/delay reporting |

### DRC Test Providers (27 providers)

| Provider | Checks |
|----------|--------|
| `drc_test_provider_annular_width` | Via annular ring width |
| `drc_test_provider_connection_width` | Minimum connection width |
| `drc_test_provider_connectivity` | Dangling tracks/vias, isolated copper |
| `drc_test_provider_copper_clearance` | Copper-to-copper clearance |
| `drc_test_provider_courtyard_clearance` | Footprint courtyard overlaps |
| `drc_test_provider_creepage` | Creepage distance |
| `drc_test_provider_diff_pair_coupling` | Differential pair coupling |
| `drc_test_provider_disallow` | Keep-out zone violations |
| `drc_test_provider_edge_clearance` | Board edge clearance |
| `drc_test_provider_footprint_checks` | Footprint integrity |
| `drc_test_provider_hole_size` | Drill hole sizes |
| `drc_test_provider_hole_to_hole` | Hole-to-hole clearance |
| `drc_test_provider_library_parity` | Library vs board parity |
| `drc_test_provider_matched_length` | Length matching |
| `drc_test_provider_misc` | Miscellaneous checks |
| `drc_test_provider_physical_clearance` | Physical clearance |
| `drc_test_provider_schematic_parity` | Schematic vs PCB parity |
| `drc_test_provider_silk_clearance` | Silkscreen clearance |
| `drc_test_provider_sliver_checker` | Copper sliver detection |
| `drc_test_provider_solder_mask` | Solder mask checks |
| `drc_test_provider_text_dims` | Text dimension checks |
| `drc_test_provider_text_mirroring` | Text mirroring checks |
| `drc_test_provider_track_angle` | Track angle constraints |
| `drc_test_provider_track_segment_length` | Track segment length |
| `drc_test_provider_track_width` | Track width |
| `drc_test_provider_via_diameter` | Via diameter |
| `drc_test_provider_zone_connections` | Zone connection checks |

### DRC_ENGINE Class

```cpp
class DRC_ENGINE  // inherits nothing significant
{
    BOARD*                      m_board;           // The board to check
    BOARD_DESIGN_SETTINGS*      m_designSettings;  // Design rules
    std::vector<DRC_RULE>       m_rules;           // Loaded DRC rules
    std::vector<DRC_TEST_PROVIDER*> m_testProviders; // Registered providers
    REPORTER*                   m_reporter;        // Status reporting
    PROGRESS_REPORTER*          m_progressReporter;// Progress callback

    // Key methods:
    void InitEngine(wxFileName& aRulePath);  // Load rules, init providers
    void RunTests(EDA_UNITS units, bool reportAllTrackErrors, bool testFootprints);
    bool IsErrorLimitExceeded(int errorCode);
    DRC_CONSTRAINT EvalRules(DRC_CONSTRAINT_T type, BOARD_ITEM* a, BOARD_ITEM* b, PCB_LAYER_ID layer);
};
```

### DRC_TEST_PROVIDER Base Class

```cpp
class DRC_TEST_PROVIDER : public UNITS_PROVIDER
{
    DRC_ENGINE*  m_drcEngine;

    virtual bool Run() = 0;                    // Execute the test
    virtual const wxString GetName() const = 0; // Provider name

    // Helper methods:
    void reportViolation(DRC_ITEM* item, VECTOR2I pos, PCB_LAYER_ID layer);
    bool reportPhase(const wxString& msg);     // Progress reporting
    void reportAux(const wxString& msg);       // Debug output
};
```

### DRC Rule System

- **DRC_RULE**: Named rule with conditions and constraints
- **DRC_RULE_CONDITION**: Expression-based conditions (`A.hasExactNetclass('Power')`)
- **DRC_CONSTRAINT**: Specific constraint (clearance, width, etc.) with min/opt/max values
- **DRC_RULES_PARSER**: Parses `.kicad_dru` rule files (inherits `DRC_RULES_LEXER` → `DSNLEXER`)
- **Implicit rules**: Generated from `BOARD_DESIGN_SETTINGS` and netclass settings

---

## 4. BOARD Data Structure

### File: `pcbnew/board.h`

**Inheritance:**
```
EDA_ITEM
  └── BOARD_ITEM
        └── BOARD_ITEM_CONTAINER
              └── BOARD
```

**Key includes from board.h:**
```cpp
#include <board_item_container.h>
#include <board_stackup_manager/board_stackup.h>
#include <component_classes/component_class_manager.h>
#include <embedded_files.h>
#include <common.h>
#include <convert_shape_list_to_polygon.h>
#include <geometry/shape_poly_set.h>
#include <hash.h>
#include <layer_ids.h>
#include <lset.h>
#include <netinfo.h>
#include <pcb_item_containers.h>
#include <pcb_plot_params.h>
#include <title_block.h>
#include <tools/pcb_selection.h>
#include <project.h>
```

**Container types (from pcb_item_containers.h):**
```cpp
typedef std::vector<PCB_MARKER*>    MARKERS;
typedef std::vector<ZONE*>          ZONES;
typedef std::deque<PCB_TRACK*>      TRACKS;
typedef std::deque<FOOTPRINT*>      FOOTPRINTS;
typedef std::deque<PCB_GROUP*>      GROUPS;
typedef std::deque<PCB_GENERATOR*>  GENERATORS;
typedef std::deque<BOARD_ITEM*>     DRAWINGS;
```

**Key BOARD members:**
```cpp
class BOARD {
    TRACKS                  m_tracks;
    FOOTPRINTS              m_footprints;
    DRAWINGS                m_drawings;
    ZONES                   m_zones;
    MARKERS                 m_markers;
    GROUPS                  m_groups;
    NETINFO_LIST            m_NetInfo;       // Net definitions
    BOARD_DESIGN_SETTINGS*  m_designSettings;
    PROJECT*                m_project;

    // DRC-relevant methods:
    std::shared_ptr<CONNECTIVITY_DATA> GetConnectivity();
    BOARD_DESIGN_SETTINGS& GetDesignSettings();
    const ZONES& Zones() const;
    const TRACKS& Tracks() const;
    const FOOTPRINTS& Footprints() const;
};
```

### BOARD_DESIGN_SETTINGS

Location: `include/board_design_settings.h`

Contains all design rules that DRC uses:
- Track width/clearance defaults
- Via size defaults
- Netclass definitions
- DRC rule severity settings
- Board stackup info
- `std::shared_ptr<DRC_ENGINE> m_DRCEngine` - owned DRC engine reference

### Key Board Item Types

| Class | File | Purpose |
|-------|------|---------|
| `FOOTPRINT` | `pcbnew/footprint.h` | Component footprint (pads, shapes, fields) |
| `PAD` | `pcbnew/pad.h` | Footprint pad (SMD, through-hole, etc.) |
| `PCB_TRACK` | `pcbnew/pcb_track.h` | Copper trace segment |
| `PCB_VIA` | `pcbnew/pcb_track.h` | Via (through, blind, micro) |
| `PCB_ARC` | `pcbnew/pcb_track.h` | Arc trace segment |
| `ZONE` | `pcbnew/zone.h` | Copper zone (pour, keepout) |
| `PCB_SHAPE` | `pcbnew/pcb_shape.h` | Board graphics (lines, arcs, polygons) |
| `PCB_TEXT` | `pcbnew/pcb_text.h` | Text objects |
| `PCB_FIELD` | `pcbnew/pcb_field.h` | Component field values |
| `NETINFO_ITEM` | `pcbnew/netinfo.h` | Net definition (name, code, netclass) |

---

## 5. PCB File Parser

### Parser Chain

```
File on disk (.kicad_pcb)
  → FILE_LINE_READER (richio.h) - line-by-line buffered reading
    → DSNLEXER (include/dsnlexer.h) - tokenizer for S-expression format
      → PCB_LEXER (build/release/common/pcb_lexer.h) - auto-generated keyword tokens
        → PCB_IO_KICAD_SEXPR_PARSER (pcbnew/pcb_io/kicad_sexpr/) - semantic parser
          → BOARD* (complete board data structure)
```

### Key Parser Files

| File | Class | Purpose |
|------|-------|---------|
| `include/richio.h` | `LINE_READER`, `FILE_LINE_READER`, `STRING_LINE_READER` | File I/O abstraction |
| `include/dsnlexer.h` | `DSNLEXER` | Base S-expression tokenizer |
| `build/release/common/pcb_lexer.h` | `PCB_LEXER` | Generated PCB keyword lexer |
| `pcbnew/pcb_io/kicad_sexpr/pcb_io_kicad_sexpr.h` | `PCB_IO_KICAD_SEXPR` | PCB I/O plugin |
| `pcbnew/pcb_io/kicad_sexpr/pcb_io_kicad_sexpr_parser.h` | `PCB_IO_KICAD_SEXPR_PARSER` | PCB file parser |

### PCB_IO_KICAD_SEXPR_PARSER Key Methods

| Method | Purpose |
|--------|---------|
| `Parse()` | Top-level entry, returns `BOARD_ITEM*` |
| `parseBOARD()` / `parseBOARD_unchecked()` | Parse complete board |
| `parseHeader()` | File version, generator |
| `parseLayers()` | Layer definitions |
| `parseSetup()` | Design settings, stackup |
| `parseFOOTPRINT()` | Component footprints |
| `parsePAD()` | Pads with padstacks |
| `parsePCB_TRACK()` / `parsePCB_VIA()` | Traces and vias |
| `parseZONE()` | Copper zones |
| `parseXY()` | Coordinate pairs |
| `parseBoardUnits()` | Distance values |

### LINE_READER / OUTPUTFORMATTER (richio.h)

**Input classes:**
- `LINE_READER` - abstract base for line-based reading
- `FILE_LINE_READER` - reads from `FILE*`
- `STRING_LINE_READER` - reads from `std::string` (**key for WASM** - no file I/O needed)
- `INPUTSTREAM_LINE_READER` - reads from `wxInputStream`

**Output classes:**
- `OUTPUTFORMATTER` - abstract base for formatted output
- `STRING_FORMATTER` - outputs to `std::string` (**key for WASM**)
- `FILE_OUTPUTFORMATTER` - outputs to file

---

## 6. wxWidgets Dependency Analysis

### Categories of wxWidgets Usage

#### A. App Framework (MUST replace)
| Usage | Location | WASM Strategy |
|-------|----------|---------------|
| `wxAppConsole` | `kicad_cli.cpp` | Replace with direct `main()` |
| `IMPLEMENT_APP_CONSOLE` | `kicad_cli.cpp` | Remove macro, call init directly |
| `wxApp::OnInit/OnRun/OnExit` | `kicad_cli.cpp` | Replace with plain functions |
| `PGM_BASE` | `common/pgm_base.cpp` | Stub or minimal reimplementation |

#### B. String Handling (pervasive, MUST abstract)
| Usage | Prevalence | WASM Strategy |
|-------|------------|---------------|
| `wxString` | ~1000s of uses across DRC+common | Typedef to `std::string` or wrapper class |
| `wxString::Format()` | Common in DRC engine | Use `fmt::format()` or `snprintf` |
| `wxS()` / `wxT()` | String literal macros | Define as no-ops |
| `From_UTF8()` / `To_UTF8()` | Conversion macros | Pass-through for UTF-8 |
| `_()` translation macro | Pervasive | Define as identity function |

**DRC-specific wxString usage:** ~15 uses in `drc_engine.cpp` (mostly `wxString::Format` for rule names). Each test provider has 2-5 wxString uses.

#### C. File System (replace for WASM)
| Usage | Location | WASM Strategy |
|-------|----------|---------------|
| `wxFileName` | Parser, DRC rule loading | Replace with `std::filesystem::path` or string ops |
| `wxDir` | Library enumeration | Not needed for single-file DRC |
| `wxFFile` | File reading | Use `STRING_LINE_READER` instead |
| `wxStandardPaths` | Path resolution | Hardcode or remove |

#### D. Logging (stub)
| Usage | Location | WASM Strategy |
|-------|----------|---------------|
| `wxLogError` | Error reporting | Redirect to `console.error` / stderr |
| `wxLogWarning` | Warnings | Redirect to `console.warn` / stderr |
| `wxLogDebug` | Debug output | No-op or conditional |
| `wxLogTrace` | Trace output | No-op |
| `wxLogFatalError` | Fatal errors | `throw` or `abort()` |

**Good news**: DRC engine code (`pcbnew/drc/`) has **zero** `wxLogError`/`wxLogWarning` calls. Logging is primarily in common code.

#### E. GUI Dependencies (remove)
| Usage | Location | WASM Strategy |
|-------|----------|---------------|
| `wxWindow*` | KIWAY, some reporters | Not needed for CLI mode |
| `wxFrame*` | KIWAY | Not needed |
| `wxStatusBar*` | Reporter | Not needed |
| `wxMsgDlg` | Error dialogs | Not needed |

#### F. Other wx Utilities
| Usage | Location | WASM Strategy |
|-------|----------|---------------|
| `wxGetEnv` | Environment variables | Use `getenv()` |
| `wxMemoryInputStream` | Clipboard parsing | Use `STRING_LINE_READER` |
| `wxTokenizer` | String splitting | Use `std::string` ops |

### Key Finding: DRC Code is Relatively Clean

The DRC engine itself (`pcbnew/drc/`) has:
- **0** direct `PGM_BASE`/`Pgm()` references
- **0** direct `SETTINGS_MANAGER` references
- **0** `wxLogError`/`wxLogWarning` calls
- **~15** `wxString` uses per file (mostly for names/messages)
- **0** GUI dependencies

DRC accesses settings only through `BOARD_DESIGN_SETTINGS` which is owned by `BOARD`.

---

## 7. External Library Dependencies

### Required for DRC

| Library | Usage | WASM Feasibility |
|---------|-------|-----------------|
| **Clipper2** | Polygon boolean operations (zone fills, clearance checks) | Compiles to WASM; include source |
| **kimath** (internal) | Geometry, vectors, shapes | Pure C++ math; compiles trivially |
| **fmtlib** | String formatting | Header-only-ish; compiles well |
| **magic_enum** | Enum reflection | Header-only; no issues |
| **Boost** (`ptr_container`) | `boost::ptr_map` in parser cache | Replace with `std::map<K, unique_ptr<V>>` |
| **protobuf** | API schema (only for api.v1.schema.json warning) | Not needed for DRC |
| **libcurl** | Network operations | Not needed for DRC |

### Required for STEP Export

| Library | Usage | WASM Feasibility |
|---------|-------|-----------------|
| **OpenCASCADE (OCCT)** | 3D geometry, STEP file generation | Use opencascade.js instead |
| **opencascade.js** | WASM port of OCCT | npm package, ready to use |

**STEP export code**: 6 files in `pcbnew/exporters/step/`, ~6362 lines total, 183 references to OCCT classes (TopoDS, BRep, XCAFDoc). Deeply integrated with OCCT.

### Internal Libraries Required

| Library | Location | Purpose | Approximate Size |
|---------|----------|---------|-----------------|
| `libs/kimath/` | Math/geometry | Vectors, shapes, polygons | ~50 files |
| `libs/core/` | Core utilities | UTF-8, base64, type helpers | ~20 files |
| `common/` | Shared code | Richio, DSNLEXER, string utils | ~100 files |
| `pcbnew/` | PCB-specific | Board model, DRC, parser | ~200 files |

---

## 8. Connectivity System

DRC heavily uses the connectivity system for net-aware checks:

```
BOARD::GetConnectivity()
  → CONNECTIVITY_DATA (pcbnew/connectivity/)
      → CONNECTIVITY_ALGO - builds connectivity graph
      → CN_ITEM, CN_CLUSTER - connectivity primitives
```

Files in `pcbnew/connectivity/`:
- `connectivity_data.h/cpp` - main connectivity data manager
- `connectivity_algo.h/cpp` - connectivity algorithm
- `connectivity_items.h/cpp` - connectivity item wrappers

This system is needed for:
- Unconnected items checking
- Net-based clearance rules
- Zone connectivity
- Ratsnest calculation

---

## 9. Stubbing Strategy

### Tier 1: Must Have (core DRC functionality)

**Keep as-is (compile directly):**
- `pcbnew/drc/` - All DRC engine and test provider files
- `libs/kimath/` - All geometry/math code
- `pcbnew/connectivity/` - Connectivity system
- `common/dsnlexer.cpp` - S-expression tokenizer
- `pcbnew/pcb_io/kicad_sexpr/pcb_io_kicad_sexpr_parser.cpp` - PCB parser

**Stub/replace:**
- `wxString` → typedef to `std::string` with compatibility methods
- `wxFileName` → simple string-based path handling
- `_()` i18n macro → identity function
- `KIWAY` → stub that directly calls pcbnew code (no dynamic loading)
- `PGM_BASE` → minimal stub with essential settings
- `REPORTER` → implementation that outputs to string buffer
- `PROGRESS_REPORTER` → no-op implementation
- File I/O → use `STRING_LINE_READER` / `STRING_FORMATTER`

### Tier 2: Nice to Have (full DRC)

- Zone fills (requires Clipper2)
- Library parity checks (requires footprint library access)
- Schematic parity (requires schematic data - out of scope)

### Tier 3: STEP Export (separate effort)

- Replace native OCCT with opencascade.js
- Extract board outline and component placement from BOARD
- Generate STEP via JavaScript API

---

## 10. Recommended WASM Build Approach

### Phase 1: Minimal wxWidgets Abstraction Layer

Create a header `wx_stubs.h` that provides:
```cpp
// String
using wxString = std::string;
#define wxT(x) x
#define wxS(x) x
#define _(x) x
inline std::string From_UTF8(const char* s) { return s ? s : ""; }
inline const char* To_UTF8(const std::string& s) { return s.c_str(); }

// Logging
#define wxLogError(...)   fprintf(stderr, __VA_ARGS__)
#define wxLogWarning(...) fprintf(stderr, __VA_ARGS__)
#define wxLogDebug(...)
#define wxLogTrace(...)
#define wxLogFatalError(...) { fprintf(stderr, __VA_ARGS__); abort(); }

// File paths
class wxFileName { /* minimal path handling */ };
```

**Challenge**: `wxString` is deeply embedded. It's not just `std::string` - it has `Format()`, `Printf()`, comparison operators with `const char*`, `IsEmpty()`, etc. A compatibility wrapper class may be needed.

### Phase 2: Source File Extraction

Estimated files to extract/compile:
- DRC: ~30 files from `pcbnew/drc/`
- Board model: ~20 files from `pcbnew/` (board, footprint, pad, track, zone, etc.)
- Parser: ~5 files from `pcbnew/pcb_io/kicad_sexpr/`
- Common: ~30 files from `common/` (richio, dsnlexer, string_utils, etc.)
- Connectivity: ~5 files from `pcbnew/connectivity/`
- Math: ~20 files from `libs/kimath/`
- Core: ~10 files from `libs/core/`
- **Total: ~120 files** (rough estimate)

### Phase 3: Emscripten Compilation

```bash
emcmake cmake -B build-wasm \
    -DCMAKE_BUILD_TYPE=Release \
    -DKICAD_WASM=ON
cmake --build build-wasm
```

Emscripten flags:
```
-s MODULARIZE=1
-s EXPORT_ES6=1
-s EXPORTED_FUNCTIONS=[...]
-s EXPORTED_RUNTIME_METHODS=['ccall','cwrap','FS']
-s ALLOW_MEMORY_GROWTH=1
```

---

## 11. Risk Assessment

| Risk | Severity | Mitigation |
|------|----------|------------|
| wxString replacement breaks compilation | HIGH | Invest in robust wxString compatibility class |
| Include chain pulls in unexpected dependencies | HIGH | Iterative build: add files one at a time |
| Connectivity system requires too much of pcbnew | MEDIUM | May need to stub some connectivity features |
| Generated files (pcb_lexer.h) need build system changes | MEDIUM | Pre-generate and commit to WASM build |
| Thread pool / mutex usage in DRC | MEDIUM | Emscripten supports pthreads; may need `-pthread` |
| Zone fill performance in WASM | LOW | Clipper2 is efficient; may be slower but functional |
| STEP export requires separate opencascade.js integration | LOW | Can be done independently from DRC |

---

## 12. Quick Reference: File Locations

```
kicad-src/
├── kicad/
│   ├── kicad_cli.cpp                    # CLI entry point
│   └── cli/
│       ├── command_pcb_drc.cpp          # DRC command
│       └── command_pcb_export_3d.cpp    # STEP export command
├── pcbnew/
│   ├── board.h/cpp                      # BOARD class
│   ├── footprint.h/cpp                  # FOOTPRINT class
│   ├── pad.h/cpp                        # PAD class
│   ├── pcb_track.h/cpp                  # PCB_TRACK, PCB_VIA, PCB_ARC
│   ├── zone.h/cpp                       # ZONE class
│   ├── netinfo.h                        # NETINFO_ITEM, NETINFO_LIST
│   ├── drc/                             # DRC engine (30+ files)
│   ├── connectivity/                    # Connectivity system
│   ├── pcb_io/kicad_sexpr/             # PCB file parser
│   └── exporters/step/                  # STEP export (6 files)
├── common/
│   ├── richio.cpp                       # File I/O abstractions
│   ├── dsnlexer.cpp                     # S-expression tokenizer
│   └── string_utils.cpp                 # String utilities
├── include/
│   ├── richio.h                         # LINE_READER, OUTPUTFORMATTER
│   ├── dsnlexer.h                       # DSNLEXER base class
│   ├── board_design_settings.h          # Design rules (DRC uses this)
│   ├── layer_ids.h                      # Layer ID enums
│   ├── kiway.h                          # Module loading
│   ├── reporter.h                       # REPORTER interface (no wx deps!)
│   └── kiid.h                           # UUID handling
├── libs/
│   ├── kimath/                          # Math/geometry library
│   │   ├── include/math/               # vector2d, box2, util
│   │   └── include/geometry/           # shapes, polygons, R-tree
│   └── core/                            # Core utilities
│       └── include/core/               # utf8, type helpers
└── build/release/common/
    └── pcb_lexer.h                      # Auto-generated lexer tokens
```
