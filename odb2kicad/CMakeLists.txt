cmake_minimum_required(VERSION 3.10)
project(odb2kicad LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(odb2kicad
    src/main.cpp
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

target_include_directories(odb2kicad PRIVATE src)
