cmake_minimum_required(VERSION 3.25)

project(rpcmp VERSION 0.1.0 LANGUAGES CXX)

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
  core/contracts/src/device_op.cpp
  core/contracts/src/validation.cpp)
target_include_directories(rpcmp_contracts PUBLIC core/contracts/include)
rpcmp_production_options(rpcmp_contracts)

add_library(rpcmp_runtime STATIC
  core/runtime/engines/mdx/mdx_decoder.cpp
  core/runtime/engines/mdx/mdx_document_machine.cpp
  core/runtime/engines/mdx/mdx_engine.cpp
  core/runtime/engines/mdx/mdx_parser.cpp
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
  core/library/src/container.cpp
  core/library/src/logical_library.cpp)
target_include_directories(rpcmp_library PUBLIC core/library/include)
target_link_libraries(rpcmp_library PUBLIC rpcmp_contracts)
rpcmp_production_options(rpcmp_library)

add_library(rpcmp_utility STATIC
  utility/src/stable_id.cpp
  utility/src/writer.cpp)
target_include_directories(rpcmp_utility PUBLIC utility/include)
target_link_libraries(rpcmp_utility PUBLIC rpcmp_library)
rpcmp_production_options(rpcmp_utility)

add_library(rpcmp_ui STATIC
  core/ui/src/text_ui.cpp)
target_include_directories(rpcmp_ui PUBLIC core/ui/include)
target_link_libraries(rpcmp_ui PUBLIC rpcmp_contracts)
rpcmp_production_options(rpcmp_ui)

add_executable(rpcmp_headless_m0 core/platform/host/headless_main.cpp)
target_link_libraries(rpcmp_headless_m0 PRIVATE rpcmp_runtime)
rpcmp_production_options(rpcmp_headless_m0)

add_executable(rpcmp_mock_ui_m0 core/platform/host/mock_ui_main.cpp)
target_link_libraries(rpcmp_mock_ui_m0 PRIVATE rpcmp_ui)
rpcmp_production_options(rpcmp_mock_ui_m0)

add_library(rpcmp_pocket_comparison_probe STATIC
  spikes/pocket/common/src/comparison_probe.cpp
  spikes/pocket/common/src/mdx_hardware_probe.cpp)
target_include_directories(rpcmp_pocket_comparison_probe PUBLIC
  spikes/pocket/common/include)
target_link_libraries(rpcmp_pocket_comparison_probe PUBLIC rpcmp_runtime)
rpcmp_production_options(rpcmp_pocket_comparison_probe)

add_library(rpcmp_pocket_app_compile_check OBJECT
  spikes/pocket/openfpgaos/main.cpp)
target_link_libraries(rpcmp_pocket_app_compile_check PRIVATE
  rpcmp_library
  rpcmp_pocket_comparison_probe)
rpcmp_production_options(rpcmp_pocket_app_compile_check)

add_executable(rpcmp_pocket_comparison_probe_host spikes/pocket/host/main.cpp)
target_link_libraries(rpcmp_pocket_comparison_probe_host PRIVATE rpcmp_pocket_comparison_probe)
rpcmp_production_options(rpcmp_pocket_comparison_probe_host)

if(BUILD_TESTING)
  add_executable(rpcmp_contract_tests tests/contracts/contract_tests.cpp)
  target_include_directories(rpcmp_contract_tests PRIVATE tests/support)
  target_link_libraries(rpcmp_contract_tests PRIVATE rpcmp_contracts)
  rpcmp_test_options(rpcmp_contract_tests)
  add_test(NAME contracts COMMAND rpcmp_contract_tests)

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
  target_link_libraries(rpcmp_stable_id_tests PRIVATE rpcmp_utility)
  rpcmp_test_options(rpcmp_stable_id_tests)
  add_test(NAME stable_id COMMAND rpcmp_stable_id_tests)

  add_executable(rpcmp_writer_tests tests/utility/writer_tests.cpp)
  target_include_directories(rpcmp_writer_tests PRIVATE tests/support)
  target_link_libraries(rpcmp_writer_tests PRIVATE rpcmp_utility)
  target_compile_definitions(rpcmp_writer_tests PRIVATE RPCMP_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
  rpcmp_test_options(rpcmp_writer_tests)
  add_test(NAME writer COMMAND rpcmp_writer_tests)

  add_executable(rpcmp_m1_acceptance_tests tests/library/m1_acceptance_tests.cpp)
  target_include_directories(rpcmp_m1_acceptance_tests PRIVATE tests/support)
  target_link_libraries(rpcmp_m1_acceptance_tests PRIVATE rpcmp_utility)
  rpcmp_test_options(rpcmp_m1_acceptance_tests)
  add_test(NAME m1_acceptance COMMAND rpcmp_m1_acceptance_tests)

  add_executable(rpcmp_ui_tests tests/ui/text_ui_tests.cpp)
  target_include_directories(rpcmp_ui_tests PRIVATE tests/support)
  target_link_libraries(rpcmp_ui_tests PRIVATE rpcmp_ui)
  rpcmp_test_options(rpcmp_ui_tests)
  add_test(NAME ui COMMAND rpcmp_ui_tests)

  add_executable(rpcmp_pocket_comparison_probe_tests
    tests/pocket/comparison_probe_tests.cpp)
  target_include_directories(rpcmp_pocket_comparison_probe_tests PRIVATE tests/support)
  target_link_libraries(rpcmp_pocket_comparison_probe_tests PRIVATE
    rpcmp_pocket_comparison_probe)
  rpcmp_test_options(rpcmp_pocket_comparison_probe_tests)
  add_test(NAME pocket_comparison_probe COMMAND rpcmp_pocket_comparison_probe_tests)

  add_test(
    NAME architecture_positive
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/architecture/check_dependencies.py
            --root ${CMAKE_SOURCE_DIR})
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
