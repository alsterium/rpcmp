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
  core/contracts/src/validation.cpp)
target_include_directories(rpcmp_contracts PUBLIC core/contracts/include)
rpcmp_production_options(rpcmp_contracts)

add_library(rpcmp_runtime STATIC
  core/runtime/src/mock_core.cpp)
target_include_directories(rpcmp_runtime PUBLIC core/runtime/include)
target_link_libraries(rpcmp_runtime PUBLIC rpcmp_contracts)
rpcmp_production_options(rpcmp_runtime)

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
  spikes/pocket/common/src/comparison_probe.cpp)
target_include_directories(rpcmp_pocket_comparison_probe PUBLIC
  spikes/pocket/common/include)
target_link_libraries(rpcmp_pocket_comparison_probe PUBLIC rpcmp_runtime)
rpcmp_production_options(rpcmp_pocket_comparison_probe)

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

  if(RPCMP_ENABLE_CLANG_TOOLS)
    add_test(NAME format
      COMMAND "${RPCMP_CLANG_FORMAT}" --dry-run --Werror ${RPCMP_CXX_FILES})
    add_test(NAME tidy
      COMMAND "${RPCMP_CLANG_TIDY}" -p "${CMAKE_BINARY_DIR}"
              --config-file "${CMAKE_SOURCE_DIR}/.clang-tidy" ${RPCMP_TIDY_FILES})
    set_tests_properties(format tidy PROPERTIES LABELS developer-tools)
  endif()
endif()
