cmake_minimum_required(VERSION 3.20)
project(kicad-drc-wasm LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Paths to the existing KiCad source and build trees ──────────────────────
set(KICAD_SRC  "${CMAKE_CURRENT_SOURCE_DIR}/../kicad-src")
set(KICAD_BUILD "${KICAD_SRC}/build/release")

# ── Include directories (mirrors pcbcommon build flags) ─────────────────────
set(KICAD_INCLUDE_DIRS
    ${KICAD_SRC}/include
    ${KICAD_SRC}/common
    ${KICAD_SRC}/common/dialogs
    ${KICAD_SRC}/common/widgets
    ${KICAD_SRC}/common/dialog_about
    ${KICAD_SRC}/libs/sexpr/include
    ${KICAD_SRC}/resources/bitmaps_png
    ${KICAD_SRC}/3d-viewer
    ${KICAD_SRC}/pcbnew
    ${KICAD_SRC}/kicad
    ${KICAD_BUILD}
    ${KICAD_SRC}/scripting
    ${KICAD_BUILD}/common
    ${KICAD_SRC}/libs/core/include
    ${KICAD_SRC}/thirdparty/argparse/include
    ${KICAD_BUILD}/api/cpp
    ${KICAD_BUILD}/api/cpp/api
    ${KICAD_SRC}/thirdparty/pegtl
    ${KICAD_SRC}/thirdparty/magic_enum/magic_enum
    ${KICAD_SRC}/thirdparty/libcontext
    ${KICAD_SRC}/libs/kimath/include
    ${KICAD_SRC}/thirdparty/clipper2/Clipper2Lib/include
    ${KICAD_SRC}/thirdparty/rtree
    ${KICAD_SRC}/libs/kiplatform/include
    ${KICAD_SRC}/thirdparty/nlohmann_json
    ${KICAD_SRC}/thirdparty/dynamic_bitset
    ${KICAD_SRC}/thirdparty/libpopcnt
    ${KICAD_SRC}/thirdparty/expected/include
    ${KICAD_SRC}/thirdparty/picosha2
    ${KICAD_SRC}/thirdparty/json_schema_validator
    ${KICAD_SRC}/thirdparty/thread-pool
    ${KICAD_SRC}/thirdparty/fast_float/include
    ${KICAD_SRC}/thirdparty/fmt/include
    ${KICAD_SRC}/thirdparty/rapidcsv
    ${KICAD_SRC}/libs/kinng/include
    ${KICAD_BUILD}/include/gal/shaders
    ${KICAD_SRC}/thirdparty/nanosvg
    ${KICAD_SRC}/thirdparty/dxflib_qcad
    ${KICAD_SRC}/thirdparty/tinyspline_lib
    ${KICAD_SRC}/thirdparty/compoundfilereader
    ${KICAD_SRC}/thirdparty/delaunator
    ${KICAD_SRC}/thirdparty/nanoflann
    ${KICAD_SRC}/thirdparty/zint
    ${KICAD_SRC}/thirdparty/nanodbc
)

# ── Find wxWidgets (needed by KiCad libraries) ─────────────────────────────
find_package(wxWidgets REQUIRED COMPONENTS gl core base net xml propgrid stc richtext html aui webview)
include(${wxWidgets_USE_FILE})

# ── Find Boost ──────────────────────────────────────────────────────────────
find_package(Boost REQUIRED)

# ── Find Python (needed by libpcbcommon) ────────────────────────────────────
find_package(Python3 COMPONENTS Development)

# ── OpenCascade include dir ─────────────────────────────────────────────────
set(OCC_INCLUDE_DIR "/usr/include/opencascade")

# ── Static library: kicad_drc ───────────────────────────────────────────────
add_library(kicad_drc STATIC
    src/api.cpp
)

target_include_directories(kicad_drc
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${KICAD_INCLUDE_DIRS}
        ${OCC_INCLUDE_DIR}
        ${Python3_INCLUDE_DIRS}
)

# Must match KiCad build defines that affect class layout and code paths.
# These were extracted from KiCad's build.ninja for the pcbcommon target.
target_compile_definitions(kicad_drc PRIVATE
    KICAD_IPC_API
    PCBNEW
    USINGZ
    NDEBUG
    HAVE_STDINT_H
    JSON_HAS_FILESYSTEM=0
    WXUSINGDLL
    __WXGTK__
    _FILE_OFFSET_BITS=64
)

target_compile_options(kicad_drc PRIVATE
    -Wno-attributes
    -Wno-ignored-attributes
)

# ── Collect DRC test provider .o files from pcbnew_kiface_objects ───────────
# These use static self-registration and would be dead-stripped without
# explicit inclusion.
file(GLOB DRC_PROVIDER_OBJS
    "${KICAD_BUILD}/pcbnew/CMakeFiles/pcbnew_kiface_objects.dir/drc/drc_test_provider_*.cpp.o"
)

# Additional .o files from pcbnew_kiface_objects needed for DRC + transitive deps
set(KIFACE_OBJ_DIR "${KICAD_BUILD}/pcbnew/CMakeFiles/pcbnew_kiface_objects.dir")
list(APPEND DRC_PROVIDER_OBJS
    ${KIFACE_OBJ_DIR}/drc/drc_report.cpp.o
    ${KIFACE_OBJ_DIR}/drc/drc_creepage_utils.cpp.o
    # Needed by I/O plugins transitively linked from pcbcommon
    ${KIFACE_OBJ_DIR}/board_stackup_manager/stackup_predefined_prms.cpp.o
    ${KIFACE_OBJ_DIR}/zone_filler.cpp.o
)

# ── Test executable ─────────────────────────────────────────────────────────
add_executable(test_drc
    test/test_drc.cpp
)

target_include_directories(test_drc PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${KICAD_INCLUDE_DIRS}
    ${OCC_INCLUDE_DIR}
    ${Python3_INCLUDE_DIRS}
)

target_compile_definitions(test_drc PRIVATE
    KICAD_IPC_API
    PCBNEW
    USINGZ
    NDEBUG
    HAVE_STDINT_H
    JSON_HAS_FILESYSTEM=0
    WXUSINGDLL
    __WXGTK__
    _FILE_OFFSET_BITS=64
)

# Link using --start-group/--end-group to resolve circular dependencies
# between KiCad's static libraries.
target_link_libraries(test_drc PRIVATE
    kicad_drc
    # DRC test providers (self-registering, must be explicitly linked)
    ${DRC_PROVIDER_OBJS}
    # Wrap static libs in a group to handle circular deps
    -Wl,--start-group
    ${KICAD_BUILD}/common/libpcbcommon.a
    ${KICAD_BUILD}/pcbnew/connectivity/libconnectivity.a
    ${KICAD_BUILD}/pcbnew/pcb_io/pcad/libpcad2kicadpcb.a
    ${KICAD_BUILD}/pcbnew/pcb_io/altium/libaltium2pcbnew.a
    ${KICAD_BUILD}/pcbnew/pcb_io/cadstar/libcadstar2pcbnew.a
    ${KICAD_BUILD}/pcbnew/pcb_io/easyeda/libeasyeda.a
    ${KICAD_BUILD}/pcbnew/pcb_io/easyedapro/libeasyedapro.a
    ${KICAD_BUILD}/pcbnew/pcb_io/fabmaster/libfabmaster.a
    ${KICAD_BUILD}/pcbnew/pcb_io/ipc2581/libipc2581.a
    ${KICAD_BUILD}/pcbnew/pcb_io/odbpp/libodbpp.a
    ${KICAD_BUILD}/common/libcommon.a
    ${KICAD_BUILD}/scripting/libscripting.a
    ${KICAD_BUILD}/3d-viewer/lib3d-viewer.a
    ${KICAD_BUILD}/3d-viewer/3d_spacenav/lib3d-viewer_spacenav.a
    ${KICAD_BUILD}/libs/kiplatform/libkiplatform.a
    ${KICAD_BUILD}/libs/core/libcore.a
    ${KICAD_BUILD}/libs/kimath/libkimath.a
    ${KICAD_BUILD}/libs/sexpr/libsexpr.a
    ${KICAD_BUILD}/libs/kinng/libkinng.a
    ${KICAD_BUILD}/utils/idftools/libidf3.a
    ${KICAD_BUILD}/thirdparty/clipper2/libclipper2.a
    ${KICAD_BUILD}/thirdparty/fmt/libfmt.a
    ${KICAD_BUILD}/thirdparty/json_schema_validator/libnlohmann_json_schema_validator.a
    ${KICAD_BUILD}/thirdparty/delaunator/libdelaunator.a
    ${KICAD_BUILD}/thirdparty/nanosvg/libnanosvg.a
    ${KICAD_BUILD}/thirdparty/dxflib_qcad/libdxflib_qcad.a
    ${KICAD_BUILD}/thirdparty/tinyspline_lib/libtinyspline_lib.a
    ${KICAD_BUILD}/thirdparty/libcontext/liblibcontext.a
    ${KICAD_BUILD}/thirdparty/nanodbc/libnanodbc.a
    ${KICAD_BUILD}/thirdparty/zint/libzint.a
    ${KICAD_BUILD}/thirdparty/markdown2html/libmarkdown_lib.a
    -Wl,--end-group
    # KiCad shared libraries
    ${KICAD_BUILD}/common/gal/libkigal.so
    ${KICAD_BUILD}/common/libkicommon.so
    ${KICAD_BUILD}/api/libkiapi.so
    ${KICAD_BUILD}/3d-viewer/3d_cache/sg/libkicad_3dsg.so
    # System libraries
    ${wxWidgets_LIBRARIES}
    ${Python3_LIBRARIES}
    Boost::headers
    GLEW GL GLU
    cairo pixman-1
    z zstd
    curl
    protobuf
    nng
    odbc
    gtk-3 gdk-3
    poppler poppler-glib
    git2
    secret-1
    gio-2.0 gobject-2.0 glib-2.0
    freetype harfbuzz fontconfig
    spnav
    m rt pthread
)

# Set RPATH so the executable can find KiCad shared libraries at runtime
set_target_properties(test_drc PROPERTIES
    BUILD_RPATH "${KICAD_BUILD}/common;${KICAD_BUILD}/common/gal;${KICAD_BUILD}/api;${KICAD_BUILD}/3d-viewer/3d_cache/sg"
)
