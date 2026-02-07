cmake_minimum_required(VERSION 3.20)
project(kicad-drc-wasm LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Paths to the KiCad source tree ──────────────────────────────────────────
set(KICAD_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../kicad-src")
set(KICAD_BUILD "${KICAD_SRC}/build/release")

# ── Include directories ─────────────────────────────────────────────────────
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

# ── Compile definitions ─────────────────────────────────────────────────────
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
    # Don't define WXUSINGDLL or __WXGTK__ - we use our own stubs
)

# ── Compile options ──────────────────────────────────────────────────────────
add_compile_options(
    -Wno-attributes
    -Wno-ignored-attributes
    -Wno-deprecated-declarations
    -Wno-unused-variable
    -Wno-unused-function
    -Wno-sign-compare
    -Wno-missing-field-initializers
    -Wno-deprecated-copy
    -pthread
)

# ── Thirdparty libraries ────────────────────────────────────────────────────

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
    ${KICAD_SRC}/thirdparty/delaunator/include/delaunator_impl.cpp
)
target_include_directories(delaunator PUBLIC ${KICAD_SRC}/thirdparty/delaunator)

# ── Core library ─────────────────────────────────────────────────────────────
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

# ── Kimath library ──────────────────────────────────────────────────────────
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
target_link_libraries(kimath PUBLIC clipper2 core)

# ── Sexpr library ────────────────────────────────────────────────────────────
add_library(sexpr STATIC
    ${KICAD_SRC}/libs/sexpr/sexpr.cpp
    ${KICAD_SRC}/libs/sexpr/sexpr_parser.cpp
)
target_include_directories(sexpr PUBLIC
    ${KICAD_SRC}/libs/sexpr/include
    ${KICAD_INCLUDE_DIRS}
)

# ── Platform stubs ───────────────────────────────────────────────────────────
add_library(kiplatform_stubs STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/platform/kiplatform_wasm.cpp
)
target_include_directories(kiplatform_stubs PUBLIC
    ${KICAD_SRC}/libs/kiplatform/include
    ${KICAD_INCLUDE_DIRS}
)

# ── WASM API (our DRC wrapper) ───────────────────────────────────────────────
add_library(kicad_drc_api STATIC
    src/api.cpp
)
target_include_directories(kicad_drc_api PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${KICAD_INCLUDE_DIRS}
)

# ── Kicommon subset (selected files from what would be libkicommon.so) ──────
# We carefully select only files needed by DRC, avoiding UI-heavy code
add_library(kicommon_wasm STATIC
    # Core utilities
    ${KICAD_SRC}/common/common.cpp
    ${KICAD_SRC}/common/exceptions.cpp
    ${KICAD_SRC}/common/string_utils.cpp
    ${KICAD_SRC}/common/richio.cpp
    ${KICAD_SRC}/common/kiid.cpp
    ${KICAD_SRC}/common/singleton.cpp
    ${KICAD_SRC}/common/pgm_base.cpp
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
    ${KICAD_SRC}/common/env_vars.cpp
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
    ${KICAD_SRC}/common/api/api_utils.cpp
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
    ${KICAD_SRC}/common/libraries/library_manager.cpp
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
    # Widgets stubs
    ${KICAD_SRC}/common/widgets/progress_reporter_base.cpp
    ${KICAD_SRC}/common/widgets/report_severity.cpp
    ${KICAD_SRC}/common/widgets/ui_common.cpp
    # JSON schema
    ${KICAD_SRC}/common/json_schema_validator.cpp
    # Dialog shim (stub)
    ${KICAD_SRC}/common/confirm.cpp
    ${KICAD_SRC}/common/dialog_shim.cpp
    ${KICAD_SRC}/common/dpi_scaling.cpp
    ${KICAD_SRC}/common/dpi_scaling_common.cpp
    # DRC needs these
    ${KICAD_SRC}/common/kiway.cpp
    ${KICAD_SRC}/common/kiway_holder.cpp
    ${KICAD_SRC}/common/kiway_mail.cpp
    # CLI progress
    ${KICAD_SRC}/common/cli_progress_reporter.cpp
    # Bitmap (for PCB_REFERENCE_IMAGE)
    ${KICAD_SRC}/common/bitmap.cpp
    ${KICAD_SRC}/common/bitmap_info.cpp
    ${KICAD_SRC}/common/bitmap_store.cpp
    # Design block
    ${KICAD_SRC}/common/design_block.cpp
    ${KICAD_SRC}/common/design_block_io.cpp
    ${KICAD_SRC}/common/design_block_library_adapter.cpp
    # Other needed
    ${KICAD_SRC}/common/gestfich.cpp
    ${KICAD_SRC}/common/eda_doc.cpp
    ${KICAD_SRC}/common/systemdirsappend.cpp
    ${KICAD_SRC}/common/searchhelpfilefullpath.cpp
    ${KICAD_SRC}/common/launch_ext.cpp
    ${KICAD_SRC}/common/notifications_manager.cpp
    ${KICAD_SRC}/common/app_monitor.cpp
    # Scripting stub
    ${KICAD_SRC}/scripting/python_scripting.cpp
    # Text eval
    ${KICAD_SRC}/common/text_eval/text_eval_wrapper.cpp
    ${KICAD_SRC}/common/text_eval/text_eval_parser.cpp
    # History
    ${KICAD_SRC}/common/local_history.cpp
    ${KICAD_SRC}/common/history_lock.cpp
    # Background jobs
    ${KICAD_SRC}/common/background_jobs_monitor.cpp
    # Tool / toolbar
    ${KICAD_SRC}/common/tool/ui/toolbar_configuration.cpp
    ${KICAD_SRC}/common/tool/ui/toolbar_context_menu_registry.cpp
    # Database
    ${KICAD_SRC}/common/database/database_lib_settings.cpp
    # Asset archive
    ${KICAD_SRC}/common/asset_archive.cpp
    # scintilla_tricks stub
    ${KICAD_SRC}/common/scintilla_tricks.cpp
    # grid_tricks
    ${KICAD_SRC}/common/grid_tricks.cpp
    # ui_events
    ${KICAD_SRC}/common/ui_events.cpp
    # kidialog
    ${KICAD_SRC}/common/kidialog.cpp
)
target_include_directories(kicommon_wasm PUBLIC ${KICAD_INCLUDE_DIRS})
target_link_libraries(kicommon_wasm PUBLIC core kimath sexpr fmt_lib kiplatform_stubs delaunator)

# ── GAL subset (minimal, for DRC provider transitive deps) ──────────────────
add_library(gal_wasm STATIC
    ${KICAD_SRC}/common/gal/painter.cpp
    ${KICAD_SRC}/common/gal/gal_display_options.cpp
    ${KICAD_SRC}/common/gal/graphics_abstraction_layer.cpp
    ${KICAD_SRC}/common/gal/cursors.cpp
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

# ── PCB Common (board data model, DRC engine, parser) ────────────────────────
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
    ${KICAD_SRC}/pcbnew/ratsnest/ratsnest_view_item.cpp
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
    ${KICAD_SRC}/pcbnew/pcb_io/pcb_io_mgr.cpp
    ${KICAD_SRC}/pcbnew/pcb_io/kicad_sexpr/pcb_io_kicad_sexpr.cpp
    ${KICAD_SRC}/pcbnew/pcb_io/kicad_sexpr/pcb_io_kicad_sexpr_parser.cpp
    ${KICAD_SRC}/pcbnew/pcb_io/kicad_legacy/pcb_io_kicad_legacy.cpp
    ${KICAD_SRC}/pcbnew/pcb_io/eagle/pcb_io_eagle.cpp
    ${KICAD_SRC}/pcbnew/pcb_io/geda/pcb_io_geda.cpp
    # Component classes
    ${KICAD_SRC}/pcbnew/component_classes/component_class.cpp
    ${KICAD_SRC}/pcbnew/component_classes/component_class_assignment_rule.cpp
    ${KICAD_SRC}/pcbnew/component_classes/component_class_cache_proxy.cpp
    ${KICAD_SRC}/pcbnew/component_classes/component_class_manager.cpp
    # Netlist
    ${KICAD_SRC}/pcbnew/netlist_reader/kicad_netlist_reader.cpp
    ${KICAD_SRC}/pcbnew/netlist_reader/legacy_netlist_reader.cpp
    ${KICAD_SRC}/pcbnew/netlist_reader/netlist_reader.cpp
    ${KICAD_SRC}/pcbnew/netlist_reader/pcb_netlist.cpp
    # Settings
    ${KICAD_SRC}/pcbnew/footprint_editor_settings.cpp
    # Length/delay
    ${KICAD_SRC}/pcbnew/length_delay_calculation/length_delay_calculation.cpp
    ${KICAD_SRC}/pcbnew/length_delay_calculation/length_delay_calculation_item.cpp
    ${KICAD_SRC}/pcbnew/length_delay_calculation/tuning_profile_parameters_user_defined.cpp
    # Router meander (needed by board_design_settings)
    ${KICAD_SRC}/pcbnew/router/pns_meander.cpp
    # API
    ${KICAD_SRC}/pcbnew/api/api_pcb_enums.cpp
    ${KICAD_SRC}/pcbnew/api/api_pcb_utils.cpp
    # PCB painter, view, draw panel
    ${KICAD_SRC}/pcbnew/pcb_painter.cpp
    ${KICAD_SRC}/pcbnew/pcb_view.cpp
    ${KICAD_SRC}/pcbnew/pcb_draw_panel_gal.cpp
    ${KICAD_SRC}/pcbnew/pcb_base_frame.cpp
    # Clipboard
    ${KICAD_SRC}/pcbnew/kicad_clipboard.cpp
    # Selection layer
    ${KICAD_SRC}/pcbnew/sel_layer.cpp
    # Tools subset
    ${KICAD_SRC}/pcbnew/tools/pcb_grid_helper.cpp
    ${KICAD_SRC}/pcbnew/tools/pcb_actions.cpp
    ${KICAD_SRC}/pcbnew/tools/pcb_editor_conditions.cpp
    ${KICAD_SRC}/pcbnew/tools/pcb_viewer_tools.cpp
    # Footprint library adapter
    ${KICAD_SRC}/pcbnew/footprint_library_adapter.cpp
    # Widgets needed by pcbcommon
    ${KICAD_SRC}/common/widgets/net_selector.cpp
    ${KICAD_SRC}/common/widgets/netclass_selector.cpp
    ${KICAD_SRC}/common/widgets/area_selector.cpp
)
target_include_directories(pcbcommon_wasm PUBLIC ${KICAD_INCLUDE_DIRS})
target_link_libraries(pcbcommon_wasm PUBLIC kicommon_wasm gal_wasm clipper2)

# ── DRC Test Providers (self-registering) ────────────────────────────────────
add_library(drc_providers STATIC
    ${KICAD_SRC}/pcbnew/drc/drc_report.cpp
    ${KICAD_SRC}/pcbnew/drc/drc_creepage_utils.cpp
    ${KICAD_SRC}/pcbnew/drc/drc_interactive_courtyard_clearance.cpp
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
    ${KICAD_SRC}/pcbnew/drc/drc_test_provider_library_parity.cpp
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
    # Zone filler needed by some DRC providers
    ${KICAD_SRC}/pcbnew/zone_filler.cpp
    # Board stackup predefined prms
    ${KICAD_SRC}/pcbnew/board_stackup_manager/stackup_predefined_prms.cpp
)
target_include_directories(drc_providers PUBLIC ${KICAD_INCLUDE_DIRS})
target_link_libraries(drc_providers PUBLIC pcbcommon_wasm)

# ── JSON schema validator ────────────────────────────────────────────────────
add_library(json_schema_validator STATIC
    ${KICAD_SRC}/thirdparty/json_schema_validator/json-schema-validator.cpp
)
target_include_directories(json_schema_validator PUBLIC
    ${KICAD_SRC}/thirdparty/json_schema_validator
    ${KICAD_SRC}/thirdparty/nlohmann_json
)

# ── Final WASM executable ────────────────────────────────────────────────────
add_executable(kicad_drc
    src/api.cpp
)

target_include_directories(kicad_drc PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${KICAD_INCLUDE_DIRS}
)

# Link everything - use whole-archive for drc_providers to preserve self-registration
target_link_libraries(kicad_drc PRIVATE
    -Wl,--whole-archive
    drc_providers
    -Wl,--no-whole-archive
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
        -sEXPORTED_FUNCTIONS=[\"_kicad_load_pcb\",\"_kicad_run_drc\",\"_kicad_get_drc_results\",\"_kicad_cleanup\",\"_malloc\",\"_free\"] \
        -sEXPORTED_RUNTIME_METHODS=[\"ccall\",\"cwrap\",\"UTF8ToString\",\"stringToUTF8\",\"lengthBytesUTF8\"] \
        -sALLOW_MEMORY_GROWTH=1 \
        -sINITIAL_MEMORY=67108864 \
        -sSTACK_SIZE=1048576 \
        -sFILESYSTEM=1 \
        -sENVIRONMENT=node,web,worker \
        -sPTHREAD_POOL_SIZE=0 \
        -sNO_DISABLE_EXCEPTION_CATCHING \
        -lembind \
        -pthread \
    "
)
