cmake_minimum_required(VERSION 3.25)

project(rpcmp VERSION 0.1.0 LANGUAGES C CXX)

include(CTest)
find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)

option(RPCMP_ENABLE_CLANG_TOOLS "Enable the pinned clang-format and clang-tidy checks" OFF)
set(RPCMP_LLVM_VERSION "" CACHE STRING "Required clang-format and clang-tidy version")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB_RECURSE RPCMP_CXX_FILES CONFIGURE_DEPENDS
  "${CMAKE_SOURCE_DIR}/core/*.cpp"
  "${CMAKE_SOURCE_DIR}/core/*.hpp"
  "${CMAKE_SOURCE_DIR}/utility/*.cpp"
  "${CMAKE_SOURCE_DIR}/utility/*.hpp"
  "${CMAKE_SOURCE_DIR}/spikes/*.cpp"
  "${CMAKE_SOURCE_DIR}/spikes/*.hpp"
  "${CMAKE_SOURCE_DIR}/tests/*.cpp"
  "${CMAKE_SOURCE_DIR}/tests/*.hpp")
set(RPCMP_TIDY_FILES ${RPCMP_CXX_FILES})
list(FILTER RPCMP_TIDY_FILES INCLUDE REGEX "[.]cpp$")
list(FILTER RPCMP_TIDY_FILES EXCLUDE REGEX "[/\\]tests[/\\]architecture[/\\]fixtures[/\\]")
list(FILTER RPCMP_TIDY_FILES EXCLUDE REGEX "[/\\]spikes[/\\]pocket[/\\]openfpgaos[/\\]desktop_main[.]cpp$")
# This optional reference-backed experiment needs its pinned generated header.
# Analyze it with hybrid-renderer-tidy.ps1 after preparing the reference; a
# normal host checkout does not require downloaded engine sources under out/.
list(FILTER RPCMP_TIDY_FILES EXCLUDE REGEX "[/\\]spikes[/\\]pocket[/\\]openfpgaos[/\\]hybrid_renderer[.]cpp$")

if(RPCMP_ENABLE_CLANG_TOOLS)
  if(RPCMP_LLVM_VERSION STREQUAL "")
    message(FATAL_ERROR "RPCMP_LLVM_VERSION must be set when clang tools are enabled")
  endif()

  set(_rpcmp_scoop_llvm_hint "$ENV{USERPROFILE}/scoop/apps/llvm/current/bin")
  find_program(RPCMP_CLANG_FORMAT NAMES clang-format HINTS "${_rpcmp_scoop_llvm_hint}" REQUIRED)
  find_program(RPCMP_CLANG_TIDY NAMES clang-tidy HINTS "${_rpcmp_scoop_llvm_hint}" REQUIRED)

  foreach(_rpcmp_tool IN ITEMS RPCMP_CLANG_FORMAT RPCMP_CLANG_TIDY)
    execute_process(
      COMMAND "${${_rpcmp_tool}}" --version
      OUTPUT_VARIABLE _rpcmp_tool_version
      ERROR_VARIABLE _rpcmp_tool_version_error
      RESULT_VARIABLE _rpcmp_tool_version_result)
    if(NOT _rpcmp_tool_version_result EQUAL 0 OR
       NOT _rpcmp_tool_version MATCHES "version ${RPCMP_LLVM_VERSION}([^0-9]|$)")
      message(FATAL_ERROR
        "${_rpcmp_tool} must report LLVM ${RPCMP_LLVM_VERSION}; got: "
        "${_rpcmp_tool_version}${_rpcmp_tool_version_error}")
    endif()
  endforeach()

  add_custom_target(format
    COMMAND "${RPCMP_CLANG_FORMAT}" -i ${RPCMP_CXX_FILES}
    COMMENT "Formatting C++ sources with clang-format ${RPCMP_LLVM_VERSION}"
    VERBATIM)
  add_custom_target(format-check
    COMMAND "${RPCMP_CLANG_FORMAT}" --dry-run --Werror ${RPCMP_CXX_FILES}
    COMMENT "Checking C++ formatting with clang-format ${RPCMP_LLVM_VERSION}"
    VERBATIM)
  add_custom_target(tidy-check
    COMMAND "${RPCMP_CLANG_TIDY}" -p "${CMAKE_BINARY_DIR}"
            --config-file "${CMAKE_SOURCE_DIR}/.clang-tidy" ${RPCMP_TIDY_FILES}
    COMMENT "Analyzing C++ sources with clang-tidy ${RPCMP_LLVM_VERSION}"
    VERBATIM)
endif()

if(MSVC)
  string(REPLACE "/EHsc" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
endif()

function(rpcmp_production_options target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX /permissive- /utf-8 /GR- /EHs-c-)
    target_compile_definitions(${target} PRIVATE _HAS_EXCEPTIONS=0)
  else()
    target_compile_options(${target} PRIVATE
      -Wall -Wextra -Wpedantic -Werror -fno-exceptions -fno-rtti)
  endif()
endfunction()

function(rpcmp_test_options target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX /permissive- /utf-8 /EHsc)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
endfunction()

add_library(rpcmp_contracts STATIC
  core/contracts/src/catalog_validation.cpp
  core/contracts/src/device_op.cpp
  core/contracts/src/performance_history_v2.cpp
  core/contracts/src/player_state_v2.cpp
  core/contracts/src/validation.cpp)
target_include_directories(rpcmp_contracts PUBLIC core/contracts/include)
rpcmp_production_options(rpcmp_contracts)

add_library(rpcmp_runtime STATIC
  core/runtime/engines/mdx/mdx_decoder.cpp
  core/runtime/engines/mdx/mdx_document_machine.cpp
  core/runtime/engines/mdx/mdx_engine.cpp
  core/runtime/engines/mdx/mdx_parser.cpp
  core/runtime/engines/mdx/mdx_performance.cpp
  core/runtime/engines/mdx/mdx_scheduler_bridge.cpp
  core/runtime/engines/mdx/mdx_semantic.cpp
  core/runtime/engines/mdx/mdx_timeline.cpp
  core/runtime/engines/mdx/mdx_track_machine.cpp
  core/runtime/engines/mdx/mdx_ym2151_router.cpp
  core/runtime/src/device_scheduler.cpp
  core/runtime/src/fixed_ym2151_sequence.cpp
  core/runtime/src/mock_core.cpp)
target_include_directories(rpcmp_runtime PUBLIC core/runtime/include)
target_link_libraries(rpcmp_runtime PUBLIC rpcmp_contracts)
rpcmp_production_options(rpcmp_runtime)

add_library(rpcmp_library STATIC
  core/library/src/prepared_playlist.cpp
  core/library/src/container.cpp
  core/library/src/logical_library.cpp)
target_include_directories(rpcmp_library PUBLIC core/library/include)
target_link_libraries(rpcmp_library PUBLIC rpcmp_contracts)
rpcmp_production_options(rpcmp_library)

add_library(rpcmp_player STATIC
  core/player/src/catalog_session.cpp
  core/player/src/media_loop_envelope.cpp
  core/player/src/mdx_performance_mapper.cpp
  core/player/src/mdx_source_producer.cpp
  core/player/src/mdx_output_history.cpp
  core/player/src/performance_history.cpp
  core/player/src/playback_navigation.cpp
  core/player/src/playback_settings.cpp
  core/player/src/playback_transport.cpp
  core/player/src/player_session.cpp
  core/player/src/snapshot_publisher.cpp
  core/player/src/mdx_library_session.cpp)
target_include_directories(rpcmp_player PUBLIC core/player/include)
target_link_libraries(rpcmp_player PUBLIC rpcmp_library rpcmp_runtime)
rpcmp_production_options(rpcmp_player)

add_library(rpcmp_pocket_platform STATIC
  core/platform/pocket/src/device_queue_mmio.cpp)
target_include_directories(rpcmp_pocket_platform PUBLIC core/platform/pocket/include)
target_link_libraries(rpcmp_pocket_platform PUBLIC rpcmp_contracts)
rpcmp_production_options(rpcmp_pocket_platform)

add_library(rpcmp_pocket_sound STATIC
  core/platform/pocket/src/sound_mmio_client.cpp
  core/platform/pocket/src/mdx_backend.cpp)
target_include_directories(rpcmp_pocket_sound PUBLIC core/platform/pocket/include)
target_link_libraries(rpcmp_pocket_sound PUBLIC rpcmp_player rpcmp_pocket_platform)
rpcmp_production_options(rpcmp_pocket_sound)

add_library(rpcmp_utility_writer STATIC
  utility/src/stable_id.cpp
  utility/src/writer.cpp)
target_include_directories(rpcmp_utility_writer PUBLIC utility/include)
target_link_libraries(rpcmp_utility_writer PUBLIC rpcmp_library)
rpcmp_production_options(rpcmp_utility_writer)

add_library(rpcmp_utf8proc STATIC third_party/utf8proc/utf8proc.c)
target_include_directories(rpcmp_utf8proc SYSTEM PUBLIC third_party/utf8proc)
target_compile_definitions(rpcmp_utf8proc PUBLIC UTF8PROC_STATIC)
set_target_properties(rpcmp_utf8proc PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON)

set(RPCMP_CP932_TABLE "${CMAKE_CURRENT_BINARY_DIR}/generated/cp932_table.inc")
add_custom_command(OUTPUT "${RPCMP_CP932_TABLE}"
  COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tools/generate_cp932.py
    --root ${CMAKE_SOURCE_DIR} --output "${RPCMP_CP932_TABLE}"
  DEPENDS tools/generate_cp932.py third_party/metadata-sources.sha256
    third_party/cp932/CP932.TXT third_party/cp932/LICENSE.txt
    third_party/utf8proc/utf8proc.c third_party/utf8proc/utf8proc.h
    third_party/utf8proc/utf8proc_data.c third_party/utf8proc/LICENSE.md
  VERBATIM)
add_library(rpcmp_utility STATIC
  utility/src/mdx_ingest.cpp utility/src/metadata.cpp utility/src/album_ingest.cpp
  "${RPCMP_CP932_TABLE}")
target_include_directories(rpcmp_utility PUBLIC utility/include)
target_include_directories(rpcmp_utility PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(rpcmp_utility PUBLIC rpcmp_utility_writer rpcmp_runtime PRIVATE rpcmp_utf8proc)
rpcmp_production_options(rpcmp_utility)

add_executable(rpcmp_mdx_pack utility/src/mdx_pack_main.cpp)
target_link_libraries(rpcmp_mdx_pack PRIVATE rpcmp_utility)
rpcmp_production_options(rpcmp_mdx_pack)

add_library(rpcmp_utility_native STATIC utility/src/album_native.cpp)
target_link_libraries(rpcmp_utility_native PUBLIC rpcmp_utility)
rpcmp_production_options(rpcmp_utility_native)
add_executable(rpcmp_album_pack utility/src/album_pack_main.cpp)
target_link_libraries(rpcmp_album_pack PRIVATE rpcmp_utility_native)
rpcmp_production_options(rpcmp_album_pack)

add_library(rpcmp_ui STATIC
  core/ui/src/text_ui.cpp
  core/ui/src/player_input.cpp
  core/ui/src/player_ui.cpp
  core/ui/src/player_render.cpp)
target_include_directories(rpcmp_ui PUBLIC core/ui/include)
target_link_libraries(rpcmp_ui PUBLIC rpcmp_contracts)
rpcmp_production_options(rpcmp_ui)

add_executable(rpcmp_font_points utility/src/font_points_main.cpp)
target_link_libraries(rpcmp_font_points PRIVATE rpcmp_utility rpcmp_utf8proc)
rpcmp_production_options(rpcmp_font_points)
set(RPCMP_BITMAP_FONT "${CMAKE_CURRENT_BINARY_DIR}/generated/bitmap_font.inc")
add_custom_command(OUTPUT "${RPCMP_BITMAP_FONT}"
  COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tools/bitmap_font_generate.py
    --repo ${CMAKE_SOURCE_DIR} --normalizer $<TARGET_FILE:rpcmp_font_points>
    --output "${RPCMP_BITMAP_FONT}"
  DEPENDS rpcmp_font_points tools/bitmap_font_generate.py third_party/cp932/CP932.TXT
    third_party/unifont/unifont_jp-16.0.04.hex.gz
  VERBATIM)
add_library(rpcmp_pocket_canvas STATIC core/platform/pocket/src/bitmap_font.cpp
  core/platform/pocket/src/bitmap_canvas.cpp "${RPCMP_BITMAP_FONT}")
target_include_directories(rpcmp_pocket_canvas PUBLIC core/platform/pocket/include)
target_include_directories(rpcmp_pocket_canvas PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(rpcmp_pocket_canvas PUBLIC rpcmp_ui)
rpcmp_production_options(rpcmp_pocket_canvas)

add_library(rpcmp_pocket_player STATIC core/platform/pocket/src/player_application.cpp)
target_link_libraries(rpcmp_pocket_player PUBLIC rpcmp_pocket_sound rpcmp_pocket_canvas)
rpcmp_production_options(rpcmp_pocket_player)
add_library(rpcmp_pocket_player_compile_check OBJECT spikes/pocket/openfpgaos/player_main.cpp)
target_link_libraries(rpcmp_pocket_player_compile_check PRIVATE rpcmp_pocket_player)
rpcmp_production_options(rpcmp_pocket_player_compile_check)
add_library(rpcmp_hybrid_app_compile_check OBJECT spikes/pocket/openfpgaos/hybrid_main.cpp)
add_library(rpcmp_minimal_player STATIC core/player/src/minimal_player.cpp)
target_include_directories(rpcmp_minimal_player PUBLIC core/player/include)
target_link_libraries(rpcmp_minimal_player PUBLIC rpcmp_library)
rpcmp_production_options(rpcmp_minimal_player)
add_library(rpcmp_minimal_ui STATIC core/ui/src/minimal_player.cpp)
target_include_directories(rpcmp_minimal_ui PUBLIC core/ui/include)
target_link_libraries(rpcmp_minimal_ui PUBLIC rpcmp_contracts)
rpcmp_production_options(rpcmp_minimal_ui)
add_library(rpcmp_minimal_display STATIC core/platform/pocket/src/minimal_display.cpp)
target_link_libraries(rpcmp_minimal_display PUBLIC rpcmp_minimal_ui rpcmp_pocket_canvas)
rpcmp_production_options(rpcmp_minimal_display)
add_library(rpcmp_hybrid_playback STATIC spikes/pocket/openfpgaos/hybrid_playback.cpp)
target_include_directories(rpcmp_hybrid_playback PUBLIC spikes/pocket/openfpgaos)
target_link_libraries(rpcmp_hybrid_playback PUBLIC rpcmp_minimal_player)
rpcmp_production_options(rpcmp_hybrid_playback)
target_link_libraries(rpcmp_hybrid_app_compile_check PRIVATE rpcmp_hybrid_playback rpcmp_minimal_display)
rpcmp_production_options(rpcmp_hybrid_app_compile_check)

add_executable(rpcmp_headless_m0 core/platform/host/headless_main.cpp)
target_link_libraries(rpcmp_headless_m0 PRIVATE rpcmp_runtime)
rpcmp_production_options(rpcmp_headless_m0)

add_executable(rpcmp_mock_ui_m0 core/platform/host/mock_ui_main.cpp)
target_link_libraries(rpcmp_mock_ui_m0 PRIVATE rpcmp_ui)
rpcmp_production_options(rpcmp_mock_ui_m0)

add_library(rpcmp_host_svg STATIC core/platform/host/svg_canvas.cpp)
target_include_directories(rpcmp_host_svg PUBLIC core/platform/host/include)
target_link_libraries(rpcmp_host_svg PUBLIC rpcmp_ui)
rpcmp_production_options(rpcmp_host_svg)

add_executable(rpcmp_player_ui_mock core/platform/host/player_ui_main.cpp)
target_link_libraries(rpcmp_player_ui_mock PRIVATE rpcmp_host_svg)
rpcmp_production_options(rpcmp_player_ui_mock)

add_library(rpcmp_pocket_comparison_probe STATIC
  spikes/pocket/common/src/comparison_probe.cpp
  spikes/pocket/common/src/mdx_hardware_probe.cpp)
target_include_directories(rpcmp_pocket_comparison_probe PUBLIC
  spikes/pocket/common/include)
target_link_libraries(rpcmp_pocket_comparison_probe PUBLIC rpcmp_runtime)
rpcmp_production_options(rpcmp_pocket_comparison_probe)

add_library(rpcmp_m5_library_loader STATIC
  spikes/pocket/common/src/m5_library_loader.cpp)
target_include_directories(rpcmp_m5_library_loader PUBLIC spikes/pocket/common/include)
target_link_libraries(rpcmp_m5_library_loader PUBLIC rpcmp_library)
rpcmp_production_options(rpcmp_m5_library_loader)

add_library(rpcmp_m5_playback STATIC spikes/pocket/common/src/m5_playback.cpp)
target_include_directories(rpcmp_m5_playback PUBLIC spikes/pocket/common/include)
target_link_libraries(rpcmp_m5_playback PUBLIC rpcmp_player)
rpcmp_production_options(rpcmp_m5_playback)

add_library(rpcmp_pocket_app_compile_check OBJECT
  spikes/pocket/openfpgaos/main.cpp)
target_link_libraries(rpcmp_pocket_app_compile_check PRIVATE
  rpcmp_library
  rpcmp_pocket_comparison_probe)
rpcmp_production_options(rpcmp_pocket_app_compile_check)

add_library(rpcmp_m5_pocket_compile_check OBJECT
  spikes/pocket/openfpgaos/m5_session_probe.cpp)
target_link_libraries(rpcmp_m5_pocket_compile_check PRIVATE rpcmp_player)
rpcmp_production_options(rpcmp_m5_pocket_compile_check)

add_library(rpcmp_m5_audio_pocket_compile_check OBJECT
  spikes/pocket/openfpgaos/m5_audio_probe.cpp)
target_link_libraries(rpcmp_m5_audio_pocket_compile_check PRIVATE
  rpcmp_player
  rpcmp_pocket_platform)
rpcmp_production_options(rpcmp_m5_audio_pocket_compile_check)

add_library(rpcmp_m5_file_pocket_compile_check OBJECT spikes/pocket/openfpgaos/m5_file_probe.cpp)
target_link_libraries(rpcmp_m5_file_pocket_compile_check PRIVATE
  rpcmp_m5_library_loader rpcmp_m5_playback rpcmp_pocket_platform)
rpcmp_production_options(rpcmp_m5_file_pocket_compile_check)

add_library(rpcmp_m5_desktop_compile_check OBJECT
  spikes/pocket/openfpgaos/m5_session_desktop.cpp)
target_link_libraries(rpcmp_m5_desktop_compile_check PRIVATE rpcmp_player)
rpcmp_production_options(rpcmp_m5_desktop_compile_check)

add_executable(rpcmp_pocket_comparison_probe_host spikes/pocket/host/main.cpp)
target_link_libraries(rpcmp_pocket_comparison_probe_host PRIVATE rpcmp_pocket_comparison_probe)
rpcmp_production_options(rpcmp_pocket_comparison_probe_host)

add_executable(rpcmp_mdx_trace_export spikes/pocket/host/mdx_trace_main.cpp)
target_link_libraries(rpcmp_mdx_trace_export PRIVATE rpcmp_pocket_comparison_probe)
rpcmp_production_options(rpcmp_mdx_trace_export)

add_executable(rpcmp_mdx_admission spikes/pocket/host/mdx_admission_main.cpp)
target_link_libraries(rpcmp_mdx_admission PRIVATE rpcmp_runtime)
rpcmp_production_options(rpcmp_mdx_admission)

add_executable(rpcmp_m5_file_preflight spikes/pocket/host/m5_file_preflight.cpp)
target_link_libraries(rpcmp_m5_file_preflight PRIVATE rpcmp_m5_library_loader rpcmp_m5_playback rpcmp_utility)
rpcmp_production_options(rpcmp_m5_file_preflight)

add_executable(rpcmp_player_preflight spikes/pocket/host/player_preflight.cpp)
target_link_libraries(rpcmp_player_preflight PRIVATE rpcmp_player)
rpcmp_production_options(rpcmp_player_preflight)

if(BUILD_TESTING)
  add_executable(rpcmp_minimal_player_tests tests/pocket/minimal_player_tests.cpp)
  target_include_directories(rpcmp_minimal_player_tests PRIVATE tests/support)
  target_link_libraries(rpcmp_minimal_player_tests PRIVATE rpcmp_hybrid_playback rpcmp_minimal_display)
  rpcmp_test_options(rpcmp_minimal_player_tests)
  add_test(NAME minimal_player COMMAND rpcmp_minimal_player_tests)
  add_executable(rpcmp_album_ingest_tests tests/utility/album_ingest_tests.cpp)
  target_include_directories(rpcmp_album_ingest_tests PRIVATE tests/support)
  target_link_libraries(rpcmp_album_ingest_tests PRIVATE rpcmp_utility)
  target_compile_definitions(rpcmp_album_ingest_tests PRIVATE RPCMP_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
  rpcmp_test_options(rpcmp_album_ingest_tests)
  add_test(NAME album_ingest COMMAND rpcmp_album_ingest_tests)
  add_test(NAME album_cli
    COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/utility/album_cli_tests.py
      $<TARGET_FILE:rpcmp_album_pack> ${CMAKE_SOURCE_DIR}/tests/fixtures/mdx/oracle-fm.mdx.hex)
  add_executable(rpcmp_album_catalog_tests tests/library/album_catalog_tests.cpp)
  target_include_directories(rpcmp_album_catalog_tests PRIVATE tests/support)
  target_link_libraries(rpcmp_album_catalog_tests PRIVATE rpcmp_utility_writer)
  target_compile_definitions(rpcmp_album_catalog_tests PRIVATE RPCMP_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
  rpcmp_test_options(rpcmp_album_catalog_tests)
  add_test(NAME album_catalog COMMAND rpcmp_album_catalog_tests)
  add_test(NAME incremental_build
    COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/harness/incremental_build_tests.py
      ${CMAKE_COMMAND})
  add_test(NAME metadata_source_integrity
    COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tools/generate_cp932.py
      --root ${CMAKE_SOURCE_DIR})
  add_executable(rpcmp_metadata_tests tests/utility/metadata_tests.cpp)
  target_include_directories(rpcmp_metadata_tests PRIVATE tests/support)
  target_link_libraries(rpcmp_metadata_tests PRIVATE rpcmp_utility)
  rpcmp_test_options(rpcmp_metadata_tests)
  add_test(NAME metadata COMMAND rpcmp_metadata_tests)
  add_test(NAME mdx_metadata_cli
    COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/utility/mdx_metadata_cli_tests.py
      $<TARGET_FILE:rpcmp_mdx_pack> ${CMAKE_SOURCE_DIR}/tests/fixtures/mdx/oracle-fm.mdx.hex)
  add_test(NAME m5_file_preflight
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/pocket/m5_file_preflight_tests.py
      $<TARGET_FILE:rpcmp_mdx_pack> $<TARGET_FILE:rpcmp_m5_file_preflight>
      ${CMAKE_SOURCE_DIR}/tests/fixtures/mdx/oracle-fm.mdx.hex)
  add_executable(rpcmp_m5_playback_tests tests/pocket/m5_playback_tests.cpp)
  target_include_directories(rpcmp_m5_playback_tests PRIVATE tests/support)
  target_link_libraries(rpcmp_m5_playback_tests PRIVATE rpcmp_m5_playback)
  rpcmp_test_options(rpcmp_m5_playback_tests)
  add_test(NAME m5_playback COMMAND rpcmp_m5_playback_tests)

  add_executable(rpcmp_boot_crc_tests tests/pocket/boot_crc_tests.cpp)
  target_include_directories(rpcmp_boot_crc_tests PRIVATE tests/support)
  rpcmp_test_options(rpcmp_boot_crc_tests)
  add_test(NAME boot_crc COMMAND rpcmp_boot_crc_tests)
  add_executable(rpcmp_player_boot_tests tests/pocket/player_boot_tests.cpp)
  target_include_directories(rpcmp_player_boot_tests PRIVATE tests/support)
  rpcmp_test_options(rpcmp_player_boot_tests)
  add_test(NAME player_boot COMMAND rpcmp_player_boot_tests)
  add_test(NAME player_firmware_prepare
    COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/pocket/player_firmware_prepare_tests.py)
  add_test(NAME player_preflight
    COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/pocket/player_preflight_tests.py
      $<TARGET_FILE:rpcmp_album_pack> $<TARGET_FILE:rpcmp_player_preflight>)
  add_test(NAME player_package
    COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/pocket/player_package_tests.py)
  add_test(NAME hybrid_package
    COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/pocket/hybrid_package_tests.py
      --checker $<TARGET_FILE:rpcmp_minimal_player_tests>)
  add_test(NAME m3u_import
    COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/utility/m3u_import_tests.py
      --checker $<TARGET_FILE:rpcmp_minimal_player_tests>)
  add_test(NAME mdx_admission_cli
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/pocket/mdx_admission_tests.py
      $<TARGET_FILE:rpcmp_mdx_admission>
      ${CMAKE_SOURCE_DIR}/tests/fixtures/mdx/oracle-fm.mdx.hex)
  add_executable(rpcmp_m5_library_loader_tests tests/pocket/m5_library_loader_tests.cpp)
  target_include_directories(rpcmp_m5_library_loader_tests PRIVATE tests/support)
  target_link_libraries(rpcmp_m5_library_loader_tests PRIVATE rpcmp_m5_library_loader rpcmp_utility)
  rpcmp_test_options(rpcmp_m5_library_loader_tests)
  add_test(NAME m5_library_loader COMMAND rpcmp_m5_library_loader_tests)

  add_executable(rpcmp_contract_tests tests/contracts/contract_tests.cpp)
  target_include_directories(rpcmp_contract_tests PRIVATE tests/support)
  target_link_libraries(rpcmp_contract_tests PRIVATE rpcmp_contracts)
  rpcmp_test_options(rpcmp_contract_tests)
  add_test(NAME contracts COMMAND rpcmp_contract_tests)

  add_executable(rpcmp_catalog_contract_tests tests/contracts/catalog_contract_tests.cpp)
  target_include_directories(rpcmp_catalog_contract_tests PRIVATE tests/support)
  target_link_libraries(rpcmp_catalog_contract_tests PRIVATE rpcmp_contracts)
  rpcmp_test_options(rpcmp_catalog_contract_tests)
  add_test(NAME catalog_contracts COMMAND rpcmp_catalog_contract_tests)

  add_executable(rpcmp_player_state_v2_tests tests/contracts/player_state_v2_tests.cpp)
  target_include_directories(rpcmp_player_state_v2_tests PRIVATE tests/support)
  target_link_libraries(rpcmp_player_state_v2_tests PRIVATE rpcmp_contracts)
  rpcmp_test_options(rpcmp_player_state_v2_tests)
  add_test(NAME player_state_v2 COMMAND rpcmp_player_state_v2_tests)

  add_executable(rpcmp_player_command_v2_tests tests/contracts/player_command_v2_tests.cpp)
  target_include_directories(rpcmp_player_command_v2_tests PRIVATE tests/support)
  target_link_libraries(rpcmp_player_command_v2_tests PRIVATE rpcmp_contracts)
  rpcmp_test_options(rpcmp_player_command_v2_tests)
  add_test(NAME player_command_v2 COMMAND rpcmp_player_command_v2_tests)

  add_executable(rpcmp_catalog_session_tests tests/player/catalog_session_tests.cpp)
  target_include_directories(rpcmp_catalog_session_tests PRIVATE tests/support)
  target_link_libraries(rpcmp_catalog_session_tests PRIVATE rpcmp_player rpcmp_utility_writer)
  target_compile_definitions(rpcmp_catalog_session_tests PRIVATE RPCMP_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
  rpcmp_test_options(rpcmp_catalog_session_tests)
  add_test(NAME catalog_session COMMAND rpcmp_catalog_session_tests)

  add_executable(rpcmp_playback_transport_tests tests/player/playback_transport_tests.cpp)
  target_include_directories(rpcmp_playback_transport_tests PRIVATE tests/support)
  target_link_libraries(rpcmp_playback_transport_tests PRIVATE rpcmp_player)
  target_compile_definitions(rpcmp_playback_transport_tests PRIVATE RPCMP_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
  rpcmp_test_options(rpcmp_playback_transport_tests)
  add_test(NAME playback_transport COMMAND rpcmp_playback_transport_tests)

  add_executable(rpcmp_snapshot_publisher_tests tests/player/snapshot_publisher_tests.cpp)
  target_include_directories(rpcmp_snapshot_publisher_tests PRIVATE tests/support)
  target_link_libraries(rpcmp_snapshot_publisher_tests PRIVATE rpcmp_player)
  target_compile_definitions(rpcmp_snapshot_publisher_tests PRIVATE RPCMP_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
  rpcmp_test_options(rpcmp_snapshot_publisher_tests)
  add_test(NAME snapshot_publisher COMMAND rpcmp_snapshot_publisher_tests)

  add_executable(rpcmp_player_session_tests tests/player/player_session_tests.cpp)
  target_include_directories(rpcmp_player_session_tests PRIVATE tests/support)
  target_link_libraries(rpcmp_player_session_tests PRIVATE rpcmp_player)
  target_compile_definitions(rpcmp_player_session_tests PRIVATE RPCMP_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
  rpcmp_test_options(rpcmp_player_session_tests)
  add_test(NAME player_session COMMAND rpcmp_player_session_tests)

  add_executable(rpcmp_player_policy_tests tests/player/player_policy_tests.cpp)
  target_include_directories(rpcmp_player_policy_tests PRIVATE tests/support)
  target_link_libraries(rpcmp_player_policy_tests PRIVATE rpcmp_player)
  target_compile_definitions(rpcmp_player_policy_tests PRIVATE RPCMP_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
  rpcmp_test_options(rpcmp_player_policy_tests)
  add_test(NAME player_policy COMMAND rpcmp_player_policy_tests)

  add_executable(rpcmp_playback_navigation_tests tests/player/playback_navigation_tests.cpp)
  target_include_directories(rpcmp_playback_navigation_tests PRIVATE tests/support)
  target_link_libraries(rpcmp_playback_navigation_tests PRIVATE rpcmp_player)
  rpcmp_test_options(rpcmp_playback_navigation_tests)
  add_test(NAME playback_navigation COMMAND rpcmp_playback_navigation_tests)

  add_executable(rpcmp_performance_history_tests tests/player/performance_history_tests.cpp)
  target_include_directories(rpcmp_performance_history_tests PRIVATE tests/support)
  target_link_libraries(rpcmp_performance_history_tests PRIVATE rpcmp_player)
  rpcmp_test_options(rpcmp_performance_history_tests)
  add_test(NAME performance_history COMMAND rpcmp_performance_history_tests)

  add_executable(rpcmp_mdx_performance_tests tests/player/mdx_performance_tests.cpp)
  target_include_directories(rpcmp_mdx_performance_tests PRIVATE tests/support)
  target_link_libraries(rpcmp_mdx_performance_tests PRIVATE rpcmp_player)
  target_compile_definitions(rpcmp_mdx_performance_tests PRIVATE RPCMP_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
  rpcmp_test_options(rpcmp_mdx_performance_tests)
  add_test(NAME mdx_performance COMMAND rpcmp_mdx_performance_tests)

  add_executable(rpcmp_mdx_source_producer_tests tests/player/mdx_source_producer_tests.cpp)
  target_include_directories(rpcmp_mdx_source_producer_tests PRIVATE tests/support)
  target_link_libraries(rpcmp_mdx_source_producer_tests PRIVATE rpcmp_player)
  rpcmp_test_options(rpcmp_mdx_source_producer_tests)
  add_test(NAME mdx_source_producer COMMAND rpcmp_mdx_source_producer_tests)

  add_executable(rpcmp_mdx_output_history_tests tests/player/mdx_output_history_tests.cpp)
  target_include_directories(rpcmp_mdx_output_history_tests PRIVATE tests/support)
  target_link_libraries(rpcmp_mdx_output_history_tests PRIVATE rpcmp_player)
  rpcmp_test_options(rpcmp_mdx_output_history_tests)
  add_test(NAME mdx_output_history COMMAND rpcmp_mdx_output_history_tests)

  add_executable(rpcmp_playback_settings_tests tests/player/playback_settings_tests.cpp)
  target_include_directories(rpcmp_playback_settings_tests PRIVATE tests/support)
  target_link_libraries(rpcmp_playback_settings_tests PRIVATE rpcmp_player)
  target_compile_definitions(rpcmp_playback_settings_tests PRIVATE RPCMP_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
  rpcmp_test_options(rpcmp_playback_settings_tests)
  add_test(NAME playback_settings COMMAND rpcmp_playback_settings_tests)

  add_executable(rpcmp_media_loop_envelope_tests tests/player/media_loop_envelope_tests.cpp)
  target_include_directories(rpcmp_media_loop_envelope_tests PRIVATE tests/support)
  target_link_libraries(rpcmp_media_loop_envelope_tests PRIVATE rpcmp_player)
  rpcmp_test_options(rpcmp_media_loop_envelope_tests)
  add_test(NAME media_loop_envelope COMMAND rpcmp_media_loop_envelope_tests)

  add_executable(rpcmp_runtime_tests tests/runtime/mock_core_tests.cpp)
  target_include_directories(rpcmp_runtime_tests PRIVATE tests/support)
  target_link_libraries(rpcmp_runtime_tests PRIVATE rpcmp_runtime)
  rpcmp_test_options(rpcmp_runtime_tests)
  add_test(NAME runtime COMMAND rpcmp_runtime_tests)

  add_executable(rpcmp_fixed_ym2151_sequence_tests
    tests/runtime/fixed_ym2151_sequence_tests.cpp)
  target_include_directories(rpcmp_fixed_ym2151_sequence_tests PRIVATE tests/support)
  target_link_libraries(rpcmp_fixed_ym2151_sequence_tests PRIVATE rpcmp_runtime)
  rpcmp_test_options(rpcmp_fixed_ym2151_sequence_tests)
  add_test(NAME fixed_ym2151_sequence COMMAND rpcmp_fixed_ym2151_sequence_tests)

  add_executable(rpcmp_device_scheduler_tests tests/runtime/device_scheduler_tests.cpp)
  target_include_directories(rpcmp_device_scheduler_tests PRIVATE tests/support)
  target_link_libraries(rpcmp_device_scheduler_tests PRIVATE rpcmp_runtime)
  rpcmp_test_options(rpcmp_device_scheduler_tests)
  add_test(NAME device_scheduler COMMAND rpcmp_device_scheduler_tests)

  add_executable(rpcmp_mdx_parser_tests tests/runtime/mdx_parser_tests.cpp)
  target_include_directories(rpcmp_mdx_parser_tests PRIVATE tests/support)
  target_link_libraries(rpcmp_mdx_parser_tests PRIVATE rpcmp_runtime)
  target_compile_definitions(rpcmp_mdx_parser_tests PRIVATE RPCMP_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
  rpcmp_test_options(rpcmp_mdx_parser_tests)
  add_test(NAME mdx_parser COMMAND rpcmp_mdx_parser_tests)

  add_executable(rpcmp_mdx_decoder_tests tests/runtime/mdx_decoder_tests.cpp)
  target_include_directories(rpcmp_mdx_decoder_tests PRIVATE tests/support)
  target_link_libraries(rpcmp_mdx_decoder_tests PRIVATE rpcmp_runtime)
  rpcmp_test_options(rpcmp_mdx_decoder_tests)
  add_test(NAME mdx_decoder COMMAND rpcmp_mdx_decoder_tests)

  add_executable(rpcmp_mdx_semantic_tests tests/runtime/mdx_semantic_tests.cpp)
  target_include_directories(rpcmp_mdx_semantic_tests PRIVATE tests/support)
  target_link_libraries(rpcmp_mdx_semantic_tests PRIVATE rpcmp_runtime)
  rpcmp_test_options(rpcmp_mdx_semantic_tests)
  add_test(NAME mdx_semantic COMMAND rpcmp_mdx_semantic_tests)

  add_executable(rpcmp_mdx_track_machine_tests tests/runtime/mdx_track_machine_tests.cpp)
  target_include_directories(rpcmp_mdx_track_machine_tests PRIVATE tests/support)
  target_link_libraries(rpcmp_mdx_track_machine_tests PRIVATE rpcmp_runtime)
  rpcmp_test_options(rpcmp_mdx_track_machine_tests)
  add_test(NAME mdx_track_machine COMMAND rpcmp_mdx_track_machine_tests)

  add_executable(rpcmp_mdx_document_machine_tests
    tests/runtime/mdx_document_machine_tests.cpp)
  target_include_directories(rpcmp_mdx_document_machine_tests PRIVATE tests/support)
  target_link_libraries(rpcmp_mdx_document_machine_tests PRIVATE rpcmp_runtime)
  rpcmp_test_options(rpcmp_mdx_document_machine_tests)
  add_test(NAME mdx_document_machine COMMAND rpcmp_mdx_document_machine_tests)

  add_executable(rpcmp_mdx_ym2151_router_tests
    tests/runtime/mdx_ym2151_router_tests.cpp)
  target_include_directories(rpcmp_mdx_ym2151_router_tests PRIVATE tests/support)
  target_link_libraries(rpcmp_mdx_ym2151_router_tests PRIVATE rpcmp_runtime)
  rpcmp_test_options(rpcmp_mdx_ym2151_router_tests)
  add_test(NAME mdx_ym2151_router COMMAND rpcmp_mdx_ym2151_router_tests)

  add_executable(rpcmp_mdx_timeline_tests tests/runtime/mdx_timeline_tests.cpp)
  target_include_directories(rpcmp_mdx_timeline_tests PRIVATE tests/support)
  target_link_libraries(rpcmp_mdx_timeline_tests PRIVATE rpcmp_runtime)
  rpcmp_test_options(rpcmp_mdx_timeline_tests)
  add_test(NAME mdx_timeline COMMAND rpcmp_mdx_timeline_tests)

  add_executable(rpcmp_mdx_scheduler_bridge_tests
    tests/runtime/mdx_scheduler_bridge_tests.cpp)
  target_include_directories(rpcmp_mdx_scheduler_bridge_tests PRIVATE tests/support)
  target_link_libraries(rpcmp_mdx_scheduler_bridge_tests PRIVATE rpcmp_runtime)
  rpcmp_test_options(rpcmp_mdx_scheduler_bridge_tests)
  add_test(NAME mdx_scheduler_bridge COMMAND rpcmp_mdx_scheduler_bridge_tests)

  add_executable(rpcmp_mdx_engine_tests tests/runtime/mdx_engine_tests.cpp)
  target_include_directories(rpcmp_mdx_engine_tests PRIVATE tests/support)
  target_link_libraries(rpcmp_mdx_engine_tests PRIVATE rpcmp_runtime)
  rpcmp_test_options(rpcmp_mdx_engine_tests)
  add_test(NAME mdx_engine COMMAND rpcmp_mdx_engine_tests)

  add_executable(rpcmp_mdx_progress_tests tests/runtime/mdx_progress_tests.cpp)
  target_include_directories(rpcmp_mdx_progress_tests PRIVATE tests/support)
  target_link_libraries(rpcmp_mdx_progress_tests PRIVATE rpcmp_runtime)
  rpcmp_test_options(rpcmp_mdx_progress_tests)
  add_test(NAME mdx_progress COMMAND rpcmp_mdx_progress_tests)

  add_executable(rpcmp_mdx_oracle_tests tests/runtime/mdx_oracle_tests.cpp)
  target_include_directories(rpcmp_mdx_oracle_tests PRIVATE tests/support)
  target_link_libraries(rpcmp_mdx_oracle_tests PRIVATE rpcmp_runtime)
  target_compile_definitions(rpcmp_mdx_oracle_tests PRIVATE RPCMP_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
  rpcmp_test_options(rpcmp_mdx_oracle_tests)
  add_test(NAME mdx_oracle COMMAND rpcmp_mdx_oracle_tests)

  add_executable(rpcmp_mdx_render_independence_tests
    tests/integration/mdx_render_independence_tests.cpp)
  target_include_directories(rpcmp_mdx_render_independence_tests PRIVATE tests/support)
  target_link_libraries(rpcmp_mdx_render_independence_tests PRIVATE rpcmp_runtime rpcmp_ui)
  target_compile_definitions(rpcmp_mdx_render_independence_tests PRIVATE
    RPCMP_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
  rpcmp_test_options(rpcmp_mdx_render_independence_tests)
  add_test(NAME mdx_render_independence COMMAND rpcmp_mdx_render_independence_tests)

  add_executable(rpcmp_library_tests tests/library/container_tests.cpp)
  target_include_directories(rpcmp_library_tests PRIVATE tests/support)
  target_link_libraries(rpcmp_library_tests PRIVATE rpcmp_library)
  rpcmp_test_options(rpcmp_library_tests)
  add_test(NAME library COMMAND rpcmp_library_tests)

  add_executable(rpcmp_logical_library_tests tests/library/logical_library_tests.cpp)
  target_include_directories(rpcmp_logical_library_tests PRIVATE tests/support)
  target_link_libraries(rpcmp_logical_library_tests PRIVATE rpcmp_library)
  rpcmp_test_options(rpcmp_logical_library_tests)
  add_test(NAME logical_library COMMAND rpcmp_logical_library_tests)

  add_executable(rpcmp_stable_id_tests tests/utility/stable_id_tests.cpp)
  target_include_directories(rpcmp_stable_id_tests PRIVATE tests/support)
  target_link_libraries(rpcmp_stable_id_tests PRIVATE rpcmp_utility_writer)
  rpcmp_test_options(rpcmp_stable_id_tests)
  add_test(NAME stable_id COMMAND rpcmp_stable_id_tests)

  add_executable(rpcmp_writer_tests tests/utility/writer_tests.cpp)
  target_include_directories(rpcmp_writer_tests PRIVATE tests/support)
  target_link_libraries(rpcmp_writer_tests PRIVATE rpcmp_utility_writer)
  target_compile_definitions(rpcmp_writer_tests PRIVATE RPCMP_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
  rpcmp_test_options(rpcmp_writer_tests)
  add_test(NAME writer COMMAND rpcmp_writer_tests)

  add_executable(rpcmp_mdx_ingest_tests tests/utility/mdx_ingest_tests.cpp)
  target_include_directories(rpcmp_mdx_ingest_tests PRIVATE tests/support)
  target_link_libraries(rpcmp_mdx_ingest_tests PRIVATE rpcmp_utility)
  target_compile_definitions(rpcmp_mdx_ingest_tests PRIVATE RPCMP_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
  rpcmp_test_options(rpcmp_mdx_ingest_tests)
  add_test(NAME mdx_ingest COMMAND rpcmp_mdx_ingest_tests)

  add_executable(rpcmp_mdx_library_session_tests tests/player/mdx_library_session_tests.cpp)
  target_include_directories(rpcmp_mdx_library_session_tests PRIVATE tests/support)
  target_link_libraries(rpcmp_mdx_library_session_tests PRIVATE rpcmp_player rpcmp_utility)
  target_compile_definitions(rpcmp_mdx_library_session_tests
    PRIVATE RPCMP_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
  rpcmp_test_options(rpcmp_mdx_library_session_tests)
  add_test(NAME mdx_library_session COMMAND rpcmp_mdx_library_session_tests)

  add_executable(rpcmp_m1_acceptance_tests tests/library/m1_acceptance_tests.cpp)
  target_include_directories(rpcmp_m1_acceptance_tests PRIVATE tests/support)
  target_link_libraries(rpcmp_m1_acceptance_tests PRIVATE rpcmp_utility_writer)
  rpcmp_test_options(rpcmp_m1_acceptance_tests)
  add_test(NAME m1_acceptance COMMAND rpcmp_m1_acceptance_tests)

  add_executable(rpcmp_ui_tests tests/ui/text_ui_tests.cpp)
  target_include_directories(rpcmp_ui_tests PRIVATE tests/support)
  target_link_libraries(rpcmp_ui_tests PRIVATE rpcmp_ui)
  rpcmp_test_options(rpcmp_ui_tests)
  add_test(NAME ui COMMAND rpcmp_ui_tests)

  add_executable(rpcmp_player_ui_tests tests/ui/player_ui_tests.cpp)
  target_include_directories(rpcmp_player_ui_tests PRIVATE tests/support)
  target_link_libraries(rpcmp_player_ui_tests PRIVATE rpcmp_ui)
  rpcmp_test_options(rpcmp_player_ui_tests)
  add_test(NAME player_ui COMMAND rpcmp_player_ui_tests)

  add_executable(rpcmp_player_render_tests tests/ui/player_render_tests.cpp)
  target_include_directories(rpcmp_player_render_tests PRIVATE tests/support)
  target_link_libraries(rpcmp_player_render_tests PRIVATE rpcmp_host_svg)
  rpcmp_test_options(rpcmp_player_render_tests)
  add_test(NAME player_render COMMAND rpcmp_player_render_tests)

  add_executable(rpcmp_bitmap_canvas_tests tests/pocket/bitmap_canvas_tests.cpp)
  target_include_directories(rpcmp_bitmap_canvas_tests PRIVATE tests/support)
  target_link_libraries(rpcmp_bitmap_canvas_tests PRIVATE rpcmp_pocket_canvas)
  rpcmp_test_options(rpcmp_bitmap_canvas_tests)
  add_test(NAME bitmap_canvas COMMAND rpcmp_bitmap_canvas_tests)
  add_test(NAME bitmap_font_source
    COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/pocket/bitmap_font_tests.py
      --normalizer $<TARGET_FILE:rpcmp_font_points> --generated "${RPCMP_BITMAP_FONT}")
  add_test(NAME player_ui_svg
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/ui/player_ui_svg_test.py"
            "$<TARGET_FILE:rpcmp_player_ui_mock>")

  add_test(NAME jt51_hold_prepare
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_SOURCE_DIR}/tests/rtl/jt51_hold_prepare_test.py")
  add_test(NAME media_clock_math
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_SOURCE_DIR}/tests/rtl/media_clock_math.py")

  add_executable(rpcmp_pocket_comparison_probe_tests
    tests/pocket/comparison_probe_tests.cpp)
  target_include_directories(rpcmp_pocket_comparison_probe_tests PRIVATE tests/support)
  target_link_libraries(rpcmp_pocket_comparison_probe_tests PRIVATE
    rpcmp_pocket_comparison_probe)
  rpcmp_test_options(rpcmp_pocket_comparison_probe_tests)
  add_test(NAME pocket_comparison_probe COMMAND rpcmp_pocket_comparison_probe_tests)

  add_executable(rpcmp_device_queue_mmio_tests
    tests/pocket/device_queue_mmio_tests.cpp)
  target_include_directories(rpcmp_device_queue_mmio_tests PRIVATE tests/support)
  target_link_libraries(rpcmp_device_queue_mmio_tests PRIVATE rpcmp_pocket_platform)
  rpcmp_test_options(rpcmp_device_queue_mmio_tests)
  add_test(NAME device_queue_mmio COMMAND rpcmp_device_queue_mmio_tests)

  add_executable(rpcmp_sound_mmio_client_tests tests/pocket/sound_mmio_client_tests.cpp)
  target_include_directories(rpcmp_sound_mmio_client_tests PRIVATE tests/support)
  target_link_libraries(rpcmp_sound_mmio_client_tests PRIVATE rpcmp_pocket_sound)
  rpcmp_test_options(rpcmp_sound_mmio_client_tests)
  add_test(NAME sound_mmio_client COMMAND rpcmp_sound_mmio_client_tests)

  add_executable(rpcmp_mdx_backend_tests tests/pocket/mdx_backend_tests.cpp)
  target_include_directories(rpcmp_mdx_backend_tests PRIVATE tests/support)
  target_link_libraries(rpcmp_mdx_backend_tests PRIVATE rpcmp_pocket_player rpcmp_utility_writer)
  target_compile_definitions(rpcmp_mdx_backend_tests PRIVATE RPCMP_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
  rpcmp_test_options(rpcmp_mdx_backend_tests)
  add_test(NAME mdx_backend COMMAND rpcmp_mdx_backend_tests)

  add_executable(rpcmp_mdx_trace_fixture_tests tests/pocket/mdx_trace_fixture_tests.cpp)
  target_include_directories(rpcmp_mdx_trace_fixture_tests PRIVATE tests/support)
  target_link_libraries(rpcmp_mdx_trace_fixture_tests PRIVATE rpcmp_pocket_comparison_probe)
  target_compile_definitions(rpcmp_mdx_trace_fixture_tests PRIVATE
    RPCMP_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
  rpcmp_test_options(rpcmp_mdx_trace_fixture_tests)
  add_test(NAME mdx_trace_fixture COMMAND rpcmp_mdx_trace_fixture_tests)

  add_test(
    NAME architecture_positive
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/architecture/check_dependencies.py
            --root ${CMAKE_SOURCE_DIR})
  add_test(NAME architecture_catalog_boundary
    COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/architecture/catalog_boundary_tests.py)
  add_test(NAME harness
    COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tools/check_harness.py
            --root ${CMAKE_SOURCE_DIR})
  add_test(NAME harness_negative
    COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/harness/harness_tests.py
            --build-dir ${CMAKE_BINARY_DIR} --ctest ${CMAKE_CTEST_COMMAND})
  add_test(NAME pocket_apf_package
    COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/pocket/apf_package_tests.py)
  add_test(
    NAME architecture_negative
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/architecture/check_dependencies.py
            --root ${CMAKE_SOURCE_DIR}/tests/architecture/fixtures/runtime_depends_ui
            --expect-violation)
  add_test(
    NAME architecture_spike_negative
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/architecture/check_dependencies.py
            --root ${CMAKE_SOURCE_DIR}/tests/architecture/fixtures/runtime_depends_spike
            --expect-violation)
  add_test(
    NAME architecture_library_negative
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/architecture/check_dependencies.py
            --root ${CMAKE_SOURCE_DIR}/tests/architecture/fixtures/library_depends_runtime
            --expect-violation)
  add_test(
    NAME pocket_package_builder
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/pocket/package_builder_tests.py)
  add_test(
    NAME pocket_m5_bss_package
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/pocket/m5_bss_package_tests.py)
  add_test(
    NAME pocket_m5_audio_boot
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/pocket/m5_audio_boot_tests.py)
  add_test(NAME pocket_boot_package
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_SOURCE_DIR}/tests/pocket/boot_package_tests.py")
  add_test(NAME pocket_firmware_pair
    COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/pocket/firmware_pair_tests.py)
  add_test(NAME pocket_control_relink
    COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/pocket/control_relink_tests.py)
  add_test(NAME pocket_placement
    COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/pocket/placement_tests.py)
  add_test(NAME pocket_failure_package
    COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/pocket/failure_package_tests.py)
  add_test(
    NAME m4_rtl_trace
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/pocket/m4_rtl_trace_tests.py)
  add_test(
    NAME mdx_corpus_audit
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/utility/mdx_corpus_audit_tests.py)

  if(RPCMP_ENABLE_CLANG_TOOLS)
    add_test(NAME format
      COMMAND "${RPCMP_CLANG_FORMAT}" --dry-run --Werror ${RPCMP_CXX_FILES})
    add_test(NAME tidy
      COMMAND "${RPCMP_CLANG_TIDY}" -p "${CMAKE_BINARY_DIR}"
              --config-file "${CMAKE_SOURCE_DIR}/.clang-tidy" ${RPCMP_TIDY_FILES})
    set_tests_properties(format tidy PROPERTIES LABELS developer-tools)
  endif()
endif()
