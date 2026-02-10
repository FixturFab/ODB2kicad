cmake_minimum_required(VERSION 3.10)
project(odb2kicad LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Core library: all source files except main.cpp and wasm_bindings.cpp
add_library(odb2kicad_core STATIC
    src/odb2kicad_lib.cpp
    src/util/string_utils.cpp
    src/odb_parser/odb_parser.cpp
    src/odb_parser/info_parser.cpp
    src/odb_parser/matrix_parser.cpp
    src/odb_parser/stephdr_parser.cpp
    src/odb_parser/profile_parser.cpp
    src/odb_parser/features_parser.cpp
    src/odb_parser/components_parser.cpp
    src/odb_parser/eda_parser.cpp
    src/odb_parser/netlist_parser.cpp
    src/odb_parser/symbols.cpp
    src/writer/kicad_writer.cpp
)
target_include_directories(odb2kicad_core PUBLIC src)

if(EMSCRIPTEN)
    # WASM build — Stage 2+
    add_executable(odb2kicad_wasm src/wasm_bindings.cpp)
    target_link_libraries(odb2kicad_wasm PRIVATE odb2kicad_core)

    set_target_properties(odb2kicad_wasm PROPERTIES
        SUFFIX ".mjs"
        LINK_FLAGS "\
            -s MODULARIZE=1 \
            -s EXPORT_ES6=1 \
            -s EXPORTED_RUNTIME_METHODS=['FS'] \
            -s ALLOW_MEMORY_GROWTH=1 \
            -s ENVIRONMENT='web,node' \
            -s FORCE_FILESYSTEM=1 \
            -lembind"
    )
else()
    # Native CLI build
    add_executable(odb2kicad src/main.cpp)
    target_link_libraries(odb2kicad PRIVATE odb2kicad_core)
endif()
