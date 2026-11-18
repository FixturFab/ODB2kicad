cmake_minimum_required(VERSION 3.10)
project(odb2kicad LANGUAGES CXX)

# Optimized unless told otherwise: a large real-world job converts in seconds, not minutes
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Core library: all source files except main.cpp and wasm_bindings.cpp
add_library(odb2kicad_core STATIC
    src/odb2kicad_lib.cpp
    src/util/string_utils.cpp
    src/util/odb_file.cpp
    src/odb_parser/odb_parser.cpp
    src/odb_parser/info_parser.cpp
    src/odb_parser/matrix_parser.cpp
    src/odb_parser/stephdr_parser.cpp
    src/odb_parser/profile_parser.cpp
    src/odb_parser/features_parser.cpp
    src/odb_parser/components_parser.cpp
    src/odb_parser/eda_parser.cpp
    src/odb_parser/netlist_parser.cpp
    src/odb_parser/tools_parser.cpp
    src/odb_parser/attrlist_parser.cpp
    src/odb_parser/font_parser.cpp
    src/odb_parser/symbols.cpp
    src/odb_parser/contour.cpp
    src/writer/kicad_writer.cpp
    src/writer/polygon.cpp
    src/writer/keyhole.cpp
    src/writer/panel.cpp
)
target_include_directories(odb2kicad_core PUBLIC src)

if(EMSCRIPTEN)
    # WASM build — Stage 2+
    add_executable(odb2kicad_wasm src/wasm_bindings.cpp)
    target_link_libraries(odb2kicad_wasm PRIVATE odb2kicad_core)

    # Size optimization: -Oz for smallest binary, -flto for link-time optimization.
    # -fwasm-exceptions: C++ exceptions work as they do natively. Without it Emscripten turns every
    # throw (a std::stoi on an unexpected token, an out-of-range value, out of memory) into an
    # abort() that kills the module ("Aborted()") instead of the error convert() reports.
    # Needs WebAssembly exception handling: Chrome/Edge 95+, Firefox 100+, Safari 15.2+, Node 17+.
    # Memory grows on demand up to MAXIMUM_MEMORY=4GB, the most a 32-bit WebAssembly module can
    # address (Emscripten's default stops at 2 GB). An 11 MB rigid-flex archive peaks near 200 MB,
    # 415 MB with every document layer drawn; past the cap convert() reports "out of memory".
    # Single-threaded: no SharedArrayBuffer, so the page needs no COOP/COEP headers.
    target_compile_options(odb2kicad_core PRIVATE -Oz -flto -fwasm-exceptions)
    target_compile_options(odb2kicad_wasm PRIVATE -Oz -flto -fwasm-exceptions)

    set_target_properties(odb2kicad_wasm PROPERTIES
        SUFFIX ".mjs"
        LINK_FLAGS "\
            -Oz \
            -flto \
            -fwasm-exceptions \
            -s MODULARIZE=1 \
            -s EXPORT_ES6=1 \
            -s EXPORTED_RUNTIME_METHODS=['FS'] \
            -s ALLOW_MEMORY_GROWTH=1 \
            -s MAXIMUM_MEMORY=4GB \
            -s ENVIRONMENT='web,node' \
            -s FORCE_FILESYSTEM=1 \
            -lembind"
    )
else()
    # Native CLI build
    add_executable(odb2kicad src/main.cpp)
    target_link_libraries(odb2kicad PRIVATE odb2kicad_core)

    # Unit tests (test/unit): ctest, or test/run_unit_tests.sh without cmake
    add_executable(odb2kicad_unit_tests test/unit/unit_tests.cpp)
    target_link_libraries(odb2kicad_unit_tests PRIVATE odb2kicad_core)
    enable_testing()
    add_test(NAME unit_tests COMMAND odb2kicad_unit_tests)
endif()
