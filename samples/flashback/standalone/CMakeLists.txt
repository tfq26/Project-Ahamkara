# Flashback Standalone Build
# ===========================
# Clean-room CMake project that consumes pre-built Ahamkara and Wish packages
# via find_package. No Ahamkara or Wish source code is referenced.
#
# Usage:
#   cmake -S samples/flashback/standalone -B build/flashback-standalone \
#     -DCMAKE_PREFIX_PATH=/path/to/installed/ahamkara;/path/to/installed/wish
#   cmake --build build/flashback-standalone
#   ctest --test-dir build/flashback-standalone

cmake_minimum_required(VERSION 3.20)
project(FlashbackStandalone VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Find installed products ─────────────────────────────────────────────
find_package(Ahamkara CONFIG REQUIRED
    COMPONENTS Core Runtime
)
find_package(Wish CONFIG QUIET)

# Make the compatibility check module available from the installed package
list(APPEND CMAKE_MODULE_PATH "${Ahamkara_DIR}")

# Optional: run cross-product compatibility check
include(CrossProductCompatibilityCheck)
check_ahamkara_wish_compatibility(
    AHAMKARA_ABI_MAJOR 1  AHAMKARA_ABI_MINOR 0  AHAMKARA_ABI_PATCH 0
    WISH_ABI_MAJOR     1  WISH_ABI_MINOR     0
    WISH_PROTO_MAJOR   1  WISH_PROTO_MINOR   0
)

# ── Flashback game module ───────────────────────────────────────────────
# The source files are the same as the in-tree build, but they compile
# against *installed* headers only — no engine source include paths.

set(FLASHBACK_GAME_MODULE_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/../game_module/src/game_module.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../game_module/src/flashback_error_catalog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../game_module/src/flashback_error_presentation.cpp
)

set(FLASHBACK_GAME_MODULE_HDR
    ${CMAKE_CURRENT_SOURCE_DIR}/../game_module/include
)

add_library(flashback_game_module STATIC
    ${FLASHBACK_GAME_MODULE_SRC}
)

target_compile_features(flashback_game_module PUBLIC cxx_std_20)

target_include_directories(flashback_game_module
    PUBLIC
        ${FLASHBACK_GAME_MODULE_HDR}
)

target_link_libraries(flashback_game_module
    PUBLIC
        Ahamkara::Runtime
        Ahamkara::Core
)

# ── Game module tests ───────────────────────────────────────────────────
enable_testing()

add_executable(flashback_game_module_tests
    ${CMAKE_CURRENT_SOURCE_DIR}/../game_module/tests/game_module_tests.cpp
)

target_compile_features(flashback_game_module_tests PRIVATE cxx_std_20)

target_include_directories(flashback_game_module_tests
    PRIVATE
        ${FLASHBACK_GAME_MODULE_HDR}
)

target_link_libraries(flashback_game_module_tests
    PRIVATE
        flashback_game_module
        Ahamkara::Runtime
        Ahamkara::Core
)

add_test(NAME flashback_game_module_tests
    COMMAND flashback_game_module_tests
)

# ── Error system tests ──────────────────────────────────────────────────
add_executable(flashback_error_tests
    ${CMAKE_CURRENT_SOURCE_DIR}/../game_module/tests/flashback_error_tests.cpp
)

target_compile_features(flashback_error_tests PRIVATE cxx_std_20)

target_include_directories(flashback_error_tests
    PRIVATE
        ${FLASHBACK_GAME_MODULE_HDR}
)

target_link_libraries(flashback_error_tests
    PRIVATE
        flashback_game_module
        Ahamkara::Runtime
        Ahamkara::Core
)

add_test(NAME flashback_error_tests
    COMMAND flashback_error_tests
)

# ── Summary ─────────────────────────────────────────────────────────────
message(STATUS "Flashback standalone configured")
message(STATUS "  Ahamkara found: ${Ahamkara_DIR}")
if(Wish_FOUND)
    message(STATUS "  Wish found:     ${Wish_DIR}")
else()
    message(STATUS "  Wish:           NOT found (optional)")
endif()
message(STATUS "  Targets: flashback_game_module, flashback_game_module_tests, flashback_error_tests")
