cmake_minimum_required(VERSION 3.20)
project(kicad-drc-wasm LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Paths to the existing KiCad source and build trees ──────────────────────
set(KICAD_SRC  "${CMAKE_CURRENT_SOURCE_DIR}/../kicad-src")
set(KICAD_BUILD "${KICAD_SRC}/build/release")

if(EMSCRIPTEN)
    # ========================================================================
    # WASM BUILD (via emcmake cmake)
    # ========================================================================

    # ── Include directories ─────────────────────────────────────────────────
    # wx stubs MUST come first to override system wx headers
    set(WX_STUB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/stubs")

    set(KICAD_INCLUDE_DIRS
        ${WX_STUB_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/platform
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
        ${KICAD_SRC}/thirdparty/nanosvg
        ${KICAD_SRC}/thirdparty/dxflib_qcad
        ${KICAD_SRC}/thirdparty/tinyspline_lib
        ${KICAD_SRC}/thirdparty/compoundfilereader
        ${KICAD_SRC}/thirdparty/delaunator
        ${KICAD_SRC}/thirdparty/nanoflann
        ${KICAD_SRC}/thirdparty/zint
        ${KICAD_SRC}/thirdparty/nanodbc
        ${KICAD_SRC}/thirdparty/turtle/include
    )

    # ── Compile definitions ─────────────────────────────────────────────────
    # Must match KiCad build defines that affect class layout and code paths
    add_compile_definitions(
        KICAD_IPC_API
        PCBNEW
        USINGZ
        NDEBUG
        HAVE_STDINT_H
        JSON_HAS_FILESYSTEM=0
        _FILE_OFFSET_BITS=64
        KICAD_WASM=1
        # kiway.h requires LIB_ENV_VAR to be defined for the platform.
        # We define __linux__ to satisfy the #if chain in kiway.h.
        # Emscripten is POSIX-compatible so this is safe for compile purposes.
        __linux__
        # Don't define WXUSINGDLL or __WXGTK__ - we use our own stubs
    )

    # ── Compile options ─────────────────────────────────────────────────────
    add_compile_options(
        -O3
        -flto
        -fexceptions
        -pthread
        -Wno-attributes
        -Wno-ignored-attributes
        -Wno-deprecated-declarations
        -Wno-unused-variable
        -Wno-unused-function
        -Wno-sign-compare
        -Wno-missing-field-initializers
        -Wno-deprecated-copy
        -Wno-non-pod-varargs
    )

    # Force-include wx/string.h for KiCad targets to mimic PCH
    # (many KiCad headers assume wxString is available)
    set(KICAD_FORCE_INCLUDE_FLAGS -include ${WX_STUB_DIR}/wx/string.h)

    # ── Thirdparty libraries ────────────────────────────────────────────────

    # Clipper2
    add_library(clipper2 STATIC
        ${KICAD_SRC}/thirdparty/clipper2/Clipper2Lib/src/clipper.engine.cpp
        ${KICAD_SRC}/thirdparty/clipper2/Clipper2Lib/src/clipper.offset.cpp
        ${KICAD_SRC}/thirdparty/clipper2/Clipper2Lib/src/clipper.rectclip.cpp
    )
    target_include_directories(clipper2 PUBLIC ${KICAD_SRC}/thirdparty/clipper2/Clipper2Lib/include)
    target_compile_definitions(clipper2 PUBLIC USINGZ)

    # fmt
    add_library(fmt_lib STATIC
        ${KICAD_SRC}/thirdparty/fmt/src/format.cc
        ${KICAD_SRC}/thirdparty/fmt/src/os.cc
    )
    target_include_directories(fmt_lib PUBLIC ${KICAD_SRC}/thirdparty/fmt/include)

    # delaunator
    add_library(delaunator STATIC
        ${KICAD_SRC}/thirdparty/delaunator/delaunator.cpp
    )
    target_include_directories(delaunator PUBLIC ${KICAD_SRC}/thirdparty/delaunator)

    # ── Core library ────────────────────────────────────────────────────────
    add_library(core STATIC
        ${KICAD_SRC}/libs/core/base64.cpp
        ${KICAD_SRC}/libs/core/observable.cpp
        ${KICAD_SRC}/libs/core/profile.cpp
        ${KICAD_SRC}/libs/core/utf8.cpp
        ${KICAD_SRC}/libs/core/version_compare.cpp
        ${KICAD_SRC}/libs/core/wx_stl_compat.cpp
    )
    target_include_directories(core PUBLIC
        ${KICAD_SRC}/libs/core/include
        ${KICAD_INCLUDE_DIRS}
    )
    target_compile_options(core PRIVATE ${KICAD_FORCE_INCLUDE_FLAGS})

    # ── Kimath library ──────────────────────────────────────────────────────
    add_library(kimath STATIC
        ${KICAD_SRC}/libs/kimath/src/bezier_curves.cpp
        ${KICAD_SRC}/libs/kimath/src/convert_basic_shapes_to_polygon.cpp
        ${KICAD_SRC}/libs/kimath/src/md5_hash.cpp
        ${KICAD_SRC}/libs/kimath/src/transform.cpp
        ${KICAD_SRC}/libs/kimath/src/trigo.cpp
        ${KICAD_SRC}/libs/kimath/src/geometry/arc_chord_params.cpp
        ${KICAD_SRC}/libs/kimath/src/geometry/corner_operations.cpp
        ${KICAD_SRC}/libs/kimath/src/geometry/distribute.cpp
        ${KICAD_SRC}/libs/kimath/src/geometry/eda_angle.cpp
        ${KICAD_SRC}/libs/kimath/src/geometry/ellipse.cpp
        ${KICAD_SRC}/libs/kimath/src/geometry/circle.cpp
        ${KICAD_SRC}/libs/kimath/src/geometry/convex_hull.cpp
        ${KICAD_SRC}/libs/kimath/src/geometry/direction_45.cpp
        ${KICAD_SRC}/libs/kimath/src/geometry/geometry_utils.cpp
        ${KICAD_SRC}/libs/kimath/src/geometry/half_line.cpp
        ${KICAD_SRC}/libs/kimath/src/geometry/intersection.cpp
        ${KICAD_SRC}/libs/kimath/src/geometry/line.cpp
        ${KICAD_SRC}/libs/kimath/src/geometry/nearest.cpp
        ${KICAD_SRC}/libs/kimath/src/geometry/oval.cpp
        ${KICAD_SRC}/libs/kimath/src/geometry/roundrect.cpp
        ${KICAD_SRC}/libs/kimath/src/geometry/seg.cpp
        ${KICAD_SRC}/libs/kimath/src/geometry/shape.cpp
        ${KICAD_SRC}/libs/kimath/src/geometry/shape_arc.cpp
        ${KICAD_SRC}/libs/kimath/src/geometry/shape_collisions.cpp
        ${KICAD_SRC}/libs/kimath/src/geometry/shape_compound.cpp
        ${KICAD_SRC}/libs/kimath/src/geometry/shape_file_io.cpp
        ${KICAD_SRC}/libs/kimath/src/geometry/shape_line_chain.cpp
        ${KICAD_SRC}/libs/kimath/src/geometry/shape_nearest_points.cpp
        ${KICAD_SRC}/libs/kimath/src/geometry/shape_poly_set.cpp
        ${KICAD_SRC}/libs/kimath/src/geometry/shape_rect.cpp
        ${KICAD_SRC}/libs/kimath/src/geometry/shape_segment.cpp
        ${KICAD_SRC}/libs/kimath/src/geometry/vector_utils.cpp
        ${KICAD_SRC}/libs/kimath/src/geometry/shape_utils.cpp
        ${KICAD_SRC}/libs/kimath/src/geometry/vertex_set.cpp
        ${KICAD_SRC}/libs/kimath/src/math/vector2.cpp
        ${KICAD_SRC}/libs/kimath/src/math/util.cpp
    )
    target_include_directories(kimath PUBLIC
        ${KICAD_SRC}/libs/kimath/include
        ${KICAD_INCLUDE_DIRS}
    )
    target_compile_options(kimath PRIVATE ${KICAD_FORCE_INCLUDE_FLAGS})
    target_link_libraries(kimath PUBLIC clipper2 core)

    # ── Sexpr library ───────────────────────────────────────────────────────
    add_library(sexpr STATIC
        ${KICAD_SRC}/libs/sexpr/sexpr.cpp
        ${KICAD_SRC}/libs/sexpr/sexpr_parser.cpp
    )
    target_include_directories(sexpr PUBLIC
        ${KICAD_SRC}/libs/sexpr/include
        ${KICAD_INCLUDE_DIRS}
    )
    target_compile_options(sexpr PRIVATE ${KICAD_FORCE_INCLUDE_FLAGS})

    # ── Platform stubs ──────────────────────────────────────────────────────
    add_library(kiplatform_stubs STATIC
        ${CMAKE_CURRENT_SOURCE_DIR}/platform/kiplatform_wasm.cpp
    )
    target_include_directories(kiplatform_stubs PUBLIC
        ${KICAD_SRC}/libs/kiplatform/include
        ${KICAD_INCLUDE_DIRS}
    )
    target_compile_options(kiplatform_stubs PRIVATE ${KICAD_FORCE_INCLUDE_FLAGS})

    # ── WASM API (our DRC wrapper) ──────────────────────────────────────────
    add_library(kicad_drc_api STATIC
        src/api.cpp
    )
    target_include_directories(kicad_drc_api PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${KICAD_INCLUDE_DIRS}
    )
    target_compile_options(kicad_drc_api PRIVATE ${KICAD_FORCE_INCLUDE_FLAGS})

    # ── Kicommon subset (selected files from what would be libkicommon.so) ──
    # We carefully select only files needed by DRC, avoiding UI-heavy code
    add_library(kicommon_wasm STATIC
        # Core utilities
        ${KICAD_SRC}/common/common.cpp
        ${KICAD_SRC}/common/exceptions.cpp
        ${KICAD_SRC}/common/string_utils.cpp
        ${KICAD_SRC}/common/richio.cpp
        ${KICAD_SRC}/common/kiid.cpp
        ${KICAD_SRC}/common/singleton.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/platform/pgm_base_wasm.cpp
        ${KICAD_SRC}/common/thread_pool.cpp
        ${KICAD_SRC}/common/advanced_config.cpp
        ${KICAD_SRC}/common/dsnlexer.cpp
        ${KICAD_SRC}/common/eda_units.cpp
        ${KICAD_SRC}/common/layer_id.cpp
        ${KICAD_SRC}/common/lib_id.cpp
        ${KICAD_SRC}/common/lset.cpp
        ${KICAD_SRC}/common/lseq.cpp
        ${KICAD_SRC}/common/netclass.cpp
        ${KICAD_SRC}/common/reporter.cpp
        ${KICAD_SRC}/common/page_info.cpp
        ${KICAD_SRC}/common/title_block.cpp
        ${KICAD_SRC}/common/locale_io.cpp
        ${KICAD_SRC}/common/trace_helpers.cpp
        ${KICAD_SRC}/common/wildcards_and_files_ext.cpp
        ${KICAD_SRC}/common/wx_filename.cpp
        ${KICAD_SRC}/common/env_paths.cpp
        # env_vars.cpp excluded - static globals use wxString::Format with wxString varargs
        # which crashes because our stub's vsnprintf can't handle non-POD varargs.
        # Stubs provided in linker_stubs.cpp.
        ${KICAD_SRC}/common/paths.cpp
        ${KICAD_SRC}/common/project.cpp
        ${KICAD_SRC}/common/eda_pattern_match.cpp
        ${KICAD_SRC}/common/markup_parser.cpp
        ${KICAD_SRC}/common/build_version.cpp
        ${KICAD_SRC}/common/search_stack.cpp
        ${KICAD_SRC}/common/embedded_files.cpp
        ${KICAD_SRC}/common/increment.cpp
        ${KICAD_SRC}/common/json_conversions.cpp
        ${KICAD_SRC}/common/xnode.cpp
        ${KICAD_SRC}/common/array_axis.cpp
        ${KICAD_SRC}/common/array_options.cpp
        ${KICAD_SRC}/common/hash_eda.cpp
        ${KICAD_SRC}/common/config_params.cpp
        # I/O
        ${KICAD_SRC}/common/io/kicad/kicad_io_utils.cpp
        ${KICAD_SRC}/common/io/io_base.cpp
        ${KICAD_SRC}/common/io/io_utils.cpp
        # API
        ${KICAD_SRC}/common/api/serializable.cpp
        # Settings
        ${KICAD_SRC}/common/settings/app_settings.cpp
        ${KICAD_SRC}/common/settings/aui_settings.cpp
        ${KICAD_SRC}/common/settings/bom_settings.cpp
        ${KICAD_SRC}/common/settings/color_settings.cpp
        ${KICAD_SRC}/common/settings/common_settings.cpp
        ${KICAD_SRC}/common/settings/grid_settings.cpp
        ${KICAD_SRC}/common/settings/json_settings.cpp
        ${KICAD_SRC}/common/settings/kicad_settings.cpp
        ${KICAD_SRC}/common/settings/layer_settings_utils.cpp
        ${KICAD_SRC}/common/settings/nested_settings.cpp
        ${KICAD_SRC}/common/settings/parameters.cpp
        ${KICAD_SRC}/common/settings/settings_manager.cpp
        # Project
        ${KICAD_SRC}/common/project/board_project_settings.cpp
        ${KICAD_SRC}/common/project/component_class_settings.cpp
        ${KICAD_SRC}/common/project/net_settings.cpp
        ${KICAD_SRC}/common/project/project_file.cpp
        ${KICAD_SRC}/common/project/project_local_settings.cpp
        ${KICAD_SRC}/common/project/tuning_profiles.cpp
        # Libraries
        ${KICAD_SRC}/common/libraries/library_table.cpp
        ${KICAD_SRC}/common/libraries/library_table_parser.cpp
        # Font
        ${KICAD_SRC}/common/newstroke_font.cpp
        ${KICAD_SRC}/common/font/fontconfig.cpp
        ${KICAD_SRC}/common/font/version_info.cpp
        # GAL color
        ${KICAD_SRC}/common/gal/color4d.cpp
        # Jobs (needed for DRC job types)
        ${KICAD_SRC}/common/jobs/job.cpp
        ${KICAD_SRC}/common/jobs/job_rc.cpp
        ${KICAD_SRC}/common/jobs/job_pcb_drc.cpp
        ${KICAD_SRC}/common/jobs/job_registry.cpp
        ${KICAD_SRC}/common/jobs/job_dispatcher.cpp
        # Widgets stubs (non-UI)
        ${KICAD_SRC}/common/widgets/progress_reporter_base.cpp
        ${KICAD_SRC}/common/widgets/report_severity.cpp
        # JSON schema
        ${KICAD_SRC}/common/json_schema_validator.cpp
        # DRC needs these
        ${KICAD_SRC}/common/kiway.cpp
        ${KICAD_SRC}/common/kiway_holder.cpp
        # CLI progress
        ${KICAD_SRC}/common/cli_progress_reporter.cpp
        # Bitmap info (metadata only, no GUI)
        ${KICAD_SRC}/common/bitmap_info.cpp
        # Design block (data model only)
        ${KICAD_SRC}/common/design_block.cpp
        # Text eval (math parser only)
        ${KICAD_SRC}/common/text_eval/text_eval_parser.cpp
        # DPI scaling (no-UI path)
        ${KICAD_SRC}/common/dpi_scaling.cpp
        # Database settings
        ${KICAD_SRC}/common/database/database_lib_settings.cpp
        # Searchhelpfile
        ${KICAD_SRC}/common/searchhelpfilefullpath.cpp
        # Launch ext (stub needed)
        ${KICAD_SRC}/common/launch_ext.cpp
        # UI events (event definitions only)
        ${KICAD_SRC}/common/ui_events.cpp
        # App monitor (can be minimal)
        ${KICAD_SRC}/common/app_monitor.cpp
        # Core data model (needed by DRC transitively)
        ${KICAD_SRC}/common/eda_item.cpp
        ${KICAD_SRC}/common/eda_shape.cpp
        ${KICAD_SRC}/common/eda_text.cpp
        ${KICAD_SRC}/common/marker_base.cpp
        ${KICAD_SRC}/common/rc_item.cpp
        ${KICAD_SRC}/common/properties/property_mgr.cpp
        ${KICAD_SRC}/common/reference_image.cpp
        ${KICAD_SRC}/common/stroke_params.cpp
        ${KICAD_SRC}/common/bitmap_base.cpp
        ${KICAD_SRC}/common/render_settings.cpp
        ${KICAD_SRC}/common/origin_transforms.cpp
        ${KICAD_SRC}/common/filter_reader.cpp
        ${KICAD_SRC}/common/libeval_compiler/libeval_compiler.cpp
        ${KICAD_SRC}/common/template_fieldnames.cpp
        ${KICAD_SRC}/common/gr_text.cpp
        ${KICAD_SRC}/common/text_eval/text_eval_wrapper.cpp
        # Generated keyword files (from build dir)
        ${KICAD_BUILD}/common/pcb_keywords.cpp
        ${KICAD_BUILD}/common/drc_rules_keywords.cpp
        ${KICAD_BUILD}/common/pcb_plot_params_keywords.cpp
        ${KICAD_BUILD}/common/embedded_files_keywords.cpp
        ${KICAD_BUILD}/common/stroke_params_keywords.cpp
        ${KICAD_BUILD}/common/lib_table_keywords.cpp
        ${KICAD_BUILD}/common/template_fieldnames_keywords.cpp
        # Drawing sheet (needed by DRC report)
        ${KICAD_SRC}/common/drawing_sheet/ds_draw_item.cpp
        ${KICAD_SRC}/common/drawing_sheet/ds_data_model.cpp
        ${KICAD_SRC}/common/drawing_sheet/ds_data_item.cpp
        ${KICAD_SRC}/common/drawing_sheet/drawing_sheet_default_description.cpp
        ${KICAD_SRC}/common/drawing_sheet/drawing_sheet_parser.cpp
        ${KICAD_SRC}/common/drawing_sheet/ds_data_model_io.cpp
        ${KICAD_BUILD}/common/drawing_sheet/drawing_sheet_keywords.cpp
        # EDA group (used by eda_item)
        ${KICAD_SRC}/common/eda_group.cpp
        # Netlist reader (schematic parity DRC)
        ${KICAD_SRC}/common/netlist_reader/netlist.cpp
        # Library manager
        ${KICAD_SRC}/common/libraries/library_manager.cpp
        # ui_common.cpp removed - too many GUI deps; stubs in linker_stubs.cpp
        # PCB render settings
        ${KICAD_SRC}/pcbnew/pcb_painter.cpp
        # Footprint library adapter
        ${KICAD_SRC}/pcbnew/footprint_library_adapter.cpp
        # Linker stubs for unneeded symbols (protobuf, kiapi, zint, etc.)
        ${CMAKE_CURRENT_SOURCE_DIR}/src/linker_stubs.cpp
    )
    target_include_directories(kicommon_wasm PUBLIC ${KICAD_INCLUDE_DIRS})
    target_link_libraries(kicommon_wasm PUBLIC core kimath sexpr fmt_lib kiplatform_stubs delaunator)

    # ── GAL subset (minimal, for DRC provider transitive deps) ──────────────
    add_library(gal_wasm STATIC
        ${KICAD_SRC}/common/gal/painter.cpp
        ${KICAD_SRC}/common/gal/gal_display_options.cpp
        ${KICAD_SRC}/common/gal/graphics_abstraction_layer.cpp
        # cursors.cpp excluded - heavy GUI deps (wxImage XPM), not needed for DRC
        ${KICAD_SRC}/common/callback_gal.cpp
        ${KICAD_SRC}/common/view/view_item.cpp
        ${KICAD_SRC}/common/view/view.cpp
        ${KICAD_SRC}/common/view/view_controls.cpp
        ${KICAD_SRC}/common/view/view_group.cpp
        ${KICAD_SRC}/common/view/view_overlay.cpp
        ${KICAD_SRC}/common/view/zoom_controller.cpp
        ${KICAD_SRC}/common/font/font.cpp
        ${KICAD_SRC}/common/font/glyph.cpp
        ${KICAD_SRC}/common/font/stroke_font.cpp
        ${KICAD_SRC}/common/font/outline_font.cpp
        ${KICAD_SRC}/common/font/outline_decomposer.cpp
        ${KICAD_SRC}/common/font/text_attributes.cpp
    )
    target_include_directories(gal_wasm PUBLIC ${KICAD_INCLUDE_DIRS})
    target_link_libraries(gal_wasm PUBLIC kicommon_wasm)

    # ── PCB Common (board data model, DRC engine, parser) ───────────────────
    add_library(pcbcommon_wasm STATIC
        # Board data model
        ${KICAD_SRC}/pcbnew/board.cpp
        ${KICAD_SRC}/pcbnew/board_commit.cpp
        ${KICAD_SRC}/pcbnew/board_connected_item.cpp
        ${KICAD_SRC}/pcbnew/board_design_settings.cpp
        ${KICAD_SRC}/pcbnew/board_item.cpp
        ${KICAD_SRC}/pcbnew/footprint.cpp
        ${KICAD_SRC}/pcbnew/fix_board_shape.cpp
        ${KICAD_SRC}/pcbnew/layer_utils.cpp
        ${KICAD_SRC}/pcbnew/netinfo_item.cpp
        ${KICAD_SRC}/pcbnew/netinfo_list.cpp
        ${KICAD_SRC}/pcbnew/pad.cpp
        ${KICAD_SRC}/pcbnew/pad_utils.cpp
        ${KICAD_SRC}/pcbnew/padstack.cpp
        ${KICAD_SRC}/pcbnew/pcb_point.cpp
        ${KICAD_SRC}/pcbnew/pcb_target.cpp
        ${KICAD_SRC}/pcbnew/pcb_reference_image.cpp
        ${KICAD_SRC}/pcbnew/pcb_field.cpp
        ${KICAD_SRC}/pcbnew/pcb_table.cpp
        ${KICAD_SRC}/pcbnew/pcb_tablecell.cpp
        ${KICAD_SRC}/pcbnew/pcb_text.cpp
        ${KICAD_SRC}/pcbnew/pcb_textbox.cpp
        ${KICAD_SRC}/pcbnew/pcb_barcode.cpp
        ${KICAD_SRC}/pcbnew/pcb_dimension.cpp
        ${KICAD_SRC}/pcbnew/pcb_shape.cpp
        ${KICAD_SRC}/pcbnew/pcb_group.cpp
        ${KICAD_SRC}/pcbnew/pcb_marker.cpp
        ${KICAD_SRC}/pcbnew/pcb_track.cpp
        ${KICAD_SRC}/pcbnew/pcb_generator.cpp
        ${KICAD_SRC}/pcbnew/zone.cpp
        ${KICAD_SRC}/pcbnew/zone_settings.cpp
        ${KICAD_SRC}/pcbnew/project_pcb.cpp
        ${KICAD_SRC}/pcbnew/collectors.cpp
        ${KICAD_SRC}/pcbnew/convert_shape_list_to_polygon.cpp
        ${KICAD_SRC}/pcbnew/generators_mgr.cpp
        ${KICAD_SRC}/pcbnew/pcb_origin_transforms.cpp
        ${KICAD_SRC}/pcbnew/pcb_plot_params.cpp
        ${KICAD_SRC}/pcbnew/pcb_screen.cpp
        ${KICAD_SRC}/pcbnew/pcbnew_settings.cpp
        ${KICAD_SRC}/pcbnew/pcb_board_outline.cpp
        # Board stackup
        ${KICAD_SRC}/pcbnew/board_stackup_manager/board_stackup.cpp
        # Teardrop
        ${KICAD_SRC}/pcbnew/teardrop/teardrop.cpp
        ${KICAD_SRC}/pcbnew/teardrop/teardrop_parameters.cpp
        ${KICAD_SRC}/pcbnew/teardrop/teardrop_utils.cpp
        # Ratsnest
        ${KICAD_SRC}/pcbnew/ratsnest/ratsnest_data.cpp
        # Connectivity
        ${KICAD_SRC}/pcbnew/connectivity/connectivity_algo.cpp
        ${KICAD_SRC}/pcbnew/connectivity/connectivity_data.cpp
        ${KICAD_SRC}/pcbnew/connectivity/connectivity_items.cpp
        ${KICAD_SRC}/pcbnew/connectivity/from_to_cache.cpp
        ${KICAD_SRC}/pcbnew/connectivity/topo_match.cpp
        # DRC core (in pcbcommon)
        ${KICAD_SRC}/pcbnew/drc/drc_engine.cpp
        ${KICAD_SRC}/pcbnew/drc/drc_cache_generator.cpp
        ${KICAD_SRC}/pcbnew/drc/drc_item.cpp
        ${KICAD_SRC}/pcbnew/drc/drc_rule.cpp
        ${KICAD_SRC}/pcbnew/drc/drc_rule_condition.cpp
        ${KICAD_SRC}/pcbnew/drc/drc_rule_parser.cpp
        ${KICAD_SRC}/pcbnew/drc/drc_test_provider.cpp
        # PCB expression evaluator
        ${KICAD_SRC}/pcbnew/pcbexpr_evaluator.cpp
        ${KICAD_SRC}/pcbnew/pcbexpr_functions.cpp
        # PCB I/O
        ${KICAD_SRC}/pcbnew/pcb_io/pcb_io.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/pcb_io_mgr_wasm.cpp
        ${KICAD_SRC}/pcbnew/pcb_io/kicad_sexpr/pcb_io_kicad_sexpr.cpp
        ${KICAD_SRC}/pcbnew/pcb_io/kicad_sexpr/pcb_io_kicad_sexpr_parser.cpp
        ${KICAD_SRC}/pcbnew/pcb_io/kicad_legacy/pcb_io_kicad_legacy.cpp
        # Component classes
        ${KICAD_SRC}/pcbnew/component_classes/component_class.cpp
        ${KICAD_SRC}/pcbnew/component_classes/component_class_assignment_rule.cpp
        ${KICAD_SRC}/pcbnew/component_classes/component_class_cache_proxy.cpp
        ${KICAD_SRC}/pcbnew/component_classes/component_class_manager.cpp
        # Settings
        ${KICAD_SRC}/pcbnew/footprint_editor_settings.cpp
        # Length/delay
        ${KICAD_SRC}/pcbnew/length_delay_calculation/length_delay_calculation.cpp
        ${KICAD_SRC}/pcbnew/length_delay_calculation/length_delay_calculation_item.cpp
        ${KICAD_SRC}/pcbnew/length_delay_calculation/tuning_profile_parameters_user_defined.cpp
        # Router meander (needed by board_design_settings)
        ${KICAD_SRC}/pcbnew/router/pns_meander.cpp
    )
    target_include_directories(pcbcommon_wasm PUBLIC ${KICAD_INCLUDE_DIRS})
    target_link_libraries(pcbcommon_wasm PUBLIC kicommon_wasm gal_wasm clipper2)

    # ── DRC Test Providers (self-registering, need --whole-archive) ─────────
    # ALL providers EXCEPT library_parity (crashes in static init, not needed in WASM anyway)
    add_library(drc_test_providers STATIC
        ${KICAD_SRC}/pcbnew/drc/drc_test_provider_annular_width.cpp
        ${KICAD_SRC}/pcbnew/drc/drc_test_provider_disallow.cpp
        ${KICAD_SRC}/pcbnew/drc/drc_test_provider_creepage.cpp
        ${KICAD_SRC}/pcbnew/drc/drc_test_provider_connectivity.cpp
        ${KICAD_SRC}/pcbnew/drc/drc_test_provider_connection_width.cpp
        ${KICAD_SRC}/pcbnew/drc/drc_test_provider_copper_clearance.cpp
        ${KICAD_SRC}/pcbnew/drc/drc_test_provider_physical_clearance.cpp
        ${KICAD_SRC}/pcbnew/drc/drc_test_provider_courtyard_clearance.cpp
        ${KICAD_SRC}/pcbnew/drc/drc_test_provider_edge_clearance.cpp
        ${KICAD_SRC}/pcbnew/drc/drc_test_provider_footprint_checks.cpp
        ${KICAD_SRC}/pcbnew/drc/drc_test_provider_hole_to_hole.cpp
        ${KICAD_SRC}/pcbnew/drc/drc_test_provider_hole_size.cpp
        ${KICAD_SRC}/pcbnew/drc/drc_test_provider_schematic_parity.cpp
        ${KICAD_SRC}/pcbnew/drc/drc_test_provider_misc.cpp
        ${KICAD_SRC}/pcbnew/drc/drc_test_provider_text_dims.cpp
        ${KICAD_SRC}/pcbnew/drc/drc_test_provider_track_angle.cpp
        ${KICAD_SRC}/pcbnew/drc/drc_test_provider_track_width.cpp
        ${KICAD_SRC}/pcbnew/drc/drc_test_provider_track_segment_length.cpp
        ${KICAD_SRC}/pcbnew/drc/drc_test_provider_zone_connections.cpp
        ${KICAD_SRC}/pcbnew/drc/drc_test_provider_via_diameter.cpp
        ${KICAD_SRC}/pcbnew/drc/drc_test_provider_solder_mask.cpp
        ${KICAD_SRC}/pcbnew/drc/drc_test_provider_silk_clearance.cpp
        ${KICAD_SRC}/pcbnew/drc/drc_test_provider_matched_length.cpp
        ${KICAD_SRC}/pcbnew/drc/drc_test_provider_diff_pair_coupling.cpp
        ${KICAD_SRC}/pcbnew/drc/drc_test_provider_sliver_checker.cpp
        ${KICAD_SRC}/pcbnew/drc/drc_test_provider_text_mirroring.cpp
    )
    target_include_directories(drc_test_providers PUBLIC ${KICAD_INCLUDE_DIRS})
    target_link_libraries(drc_test_providers PUBLIC pcbcommon_wasm)

    # ── DRC support files (regular link, no --whole-archive needed) ────────
    add_library(drc_support STATIC
        ${KICAD_SRC}/pcbnew/drc/drc_report.cpp
        ${KICAD_SRC}/pcbnew/drc/drc_creepage_utils.cpp
        ${KICAD_SRC}/pcbnew/drc/drc_interactive_courtyard_clearance.cpp
        ${KICAD_SRC}/pcbnew/zone_filler.cpp
        ${KICAD_SRC}/pcbnew/board_stackup_manager/stackup_predefined_prms.cpp
    )
    target_include_directories(drc_support PUBLIC ${KICAD_INCLUDE_DIRS})
    target_link_libraries(drc_support PUBLIC pcbcommon_wasm)

    # ── JSON schema validator ───────────────────────────────────────────────
    add_library(json_schema_validator STATIC
        ${KICAD_SRC}/thirdparty/json_schema_validator/json-validator.cpp
        ${KICAD_SRC}/thirdparty/json_schema_validator/json-uri.cpp
        ${KICAD_SRC}/thirdparty/json_schema_validator/json-patch.cpp
        ${KICAD_SRC}/thirdparty/json_schema_validator/string-format-check.cpp
        ${KICAD_SRC}/thirdparty/json_schema_validator/json-schema-draft7.json.cpp
    )
    target_include_directories(json_schema_validator PUBLIC
        ${KICAD_SRC}/thirdparty/json_schema_validator
        ${KICAD_SRC}/thirdparty/nlohmann_json
    )

    # ── Minimal test executable (for bisecting static ctor crash) ────────
    add_executable(minimal_test
        src/minimal_test.cpp
        src/api.cpp
        src/diag_main.cpp
    )
    target_include_directories(minimal_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${KICAD_INCLUDE_DIRS}
    )
    # Test: split drc_providers
    target_link_libraries(minimal_test PRIVATE
        -Wl,--whole-archive
        drc_test_providers
        -Wl,--no-whole-archive
        drc_support
        pcbcommon_wasm
        gal_wasm
        kicommon_wasm
        kiplatform_stubs
        kimath
        sexpr
        core
        clipper2
        fmt_lib
        delaunator
        json_schema_validator
    )
    set_target_properties(minimal_test PROPERTIES
        SUFFIX ".mjs"
        LINK_FLAGS "\
            -sEXPORT_ES6=1 \
            -sMODULARIZE=1 \
            -sEXPORT_NAME=createMinimalTest \
            -sEXPORTED_FUNCTIONS=[\"_minimal_ping\",\"_malloc\",\"_free\"] \
            -sEXPORTED_RUNTIME_METHODS=[\"ccall\",\"cwrap\",\"UTF8ToString\"] \
            -sALLOW_MEMORY_GROWTH=1 \
            -sINITIAL_MEMORY=67108864 \
            -sFILESYSTEM=1 \
            -sENVIRONMENT=node \
            -sNO_DISABLE_EXCEPTION_CATCHING \
            -sASSERTIONS=2 \
            -sABORTING_MALLOC=0 \
        "
    )

    # ── Final WASM executable ───────────────────────────────────────────────
    add_executable(kicad_drc
        src/api.cpp
        src/diag_main.cpp
    )

    target_include_directories(kicad_drc PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${KICAD_INCLUDE_DIRS}
    )

    # Link everything - use whole-archive for test providers to preserve self-registration
    target_link_libraries(kicad_drc PRIVATE
        -Wl,--whole-archive
        drc_test_providers
        -Wl,--no-whole-archive
        drc_support
        pcbcommon_wasm
        gal_wasm
        kicommon_wasm
        kiplatform_stubs
        kimath
        sexpr
        core
        clipper2
        fmt_lib
        delaunator
        json_schema_validator
    )

    # Emscripten-specific link flags
    set_target_properties(kicad_drc PROPERTIES
        SUFFIX ".mjs"
        LINK_FLAGS "\
            -sEXPORT_ES6=1 \
            -sMODULARIZE=1 \
            -sEXPORT_NAME=createKicadDRC \
            -sEXPORTED_FUNCTIONS=[\"_kicad_load_pcb\",\"_kicad_run_drc\",\"_kicad_get_drc_results\",\"_kicad_cleanup\",\"_diag_ping\",\"_malloc\",\"_free\"] \
            -sEXPORTED_RUNTIME_METHODS=[\"ccall\",\"cwrap\",\"UTF8ToString\",\"stringToUTF8\",\"lengthBytesUTF8\"] \
            -O3 \
            -flto \
            -sALLOW_MEMORY_GROWTH=1 \
            -sINITIAL_MEMORY=67108864 \
            -sSTACK_SIZE=16777216 \
            -sFILESYSTEM=1 \
            -sENVIRONMENT=node,web,worker \
            -sNO_DISABLE_EXCEPTION_CATCHING \
            -sASSERTIONS=0 \
            -sSTACK_OVERFLOW_CHECK=0 \
            -pthread \
            -sPTHREAD_POOL_SIZE=4 \
        "
    )

else()
    # ========================================================================
    # NATIVE BUILD (standard cmake)
    # ========================================================================

    # ── Include directories (mirrors pcbcommon build flags) ──────────────────
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

    # ── Find wxWidgets (needed by KiCad libraries) ──────────────────────────
    find_package(wxWidgets REQUIRED COMPONENTS gl core base net xml propgrid stc richtext html aui webview)
    include(${wxWidgets_USE_FILE})

    # ── Find Boost ──────────────────────────────────────────────────────────
    find_package(Boost REQUIRED)

    # ── Find Python (needed by libpcbcommon) ────────────────────────────────
    find_package(Python3 COMPONENTS Development)

    # ── OpenCascade include dir ─────────────────────────────────────────────
    set(OCC_INCLUDE_DIR "/usr/include/opencascade")

    # ── Static library: kicad_drc ───────────────────────────────────────────
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

    # ── Collect DRC test provider .o files from pcbnew_kiface_objects ───────
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

    # ── Test executable ─────────────────────────────────────────────────────
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

endif()
