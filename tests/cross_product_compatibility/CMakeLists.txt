# Cross-Product Compatibility Test
# =================================
# Standalone CMake project that exercises version compatibility checks
# between Ahamkara engine and Wish backend. Used by the CI workflow to
# prove both compatible and intentionally incompatible combinations.
#
# This project is NOT included in the main build; it is configured separately
# by the cross-product compatibility CI job or the compatibility-smoke.sh
# script.
#
# Compatible combination:  succeeds (configure + build + run)
# Incompatible combination: fails at configure time with a diagnostic

cmake_minimum_required(VERSION 3.20)
project(CrossProductCompatibilityTest VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Find installed products ─────────────────────────────────────────────
find_package(Ahamkara CONFIG REQUIRED COMPONENTS Core Runtime)

# The compatibility check module is installed alongside Ahamkara
list(APPEND CMAKE_MODULE_PATH "${Ahamkara_DIR}")

# ── Compatibility check ─────────────────────────────────────────────────
# The *_EXPECTED variables can be overridden on the command line to simulate
# incompatible version combinations:
#
#   cmake -DAHAMKARA_ABI_MAJOR_EXPECTED=2 ...
#
# When the expected version does not match the installed version the check
# issues a FATAL_ERROR with a stable diagnostic.

include(CrossProductCompatibilityCheck)

# Defaults match the current in-tree baked versions.
set(_default_ae_abi_major 1)
set(_default_ae_abi_minor 0)
set(_default_ae_abi_patch 0)
set(_default_wish_abi_major 1)
set(_default_wish_abi_minor 0)
set(_default_wish_proto_major 1)
set(_default_wish_proto_minor 0)

set(AHAMKARA_ABI_MAJOR_EXPECTED  "${AHAMKARA_ABI_MAJOR_EXPECTED}"  CACHE STRING "Expected Ahamkara ABI major")
set(AHAMKARA_ABI_MINOR_EXPECTED  "${AHAMKARA_ABI_MINOR_EXPECTED}"  CACHE STRING "Expected Ahamkara ABI minor")
set(AHAMKARA_ABI_PATCH_EXPECTED  "${AHAMKARA_ABI_PATCH_EXPECTED}"  CACHE STRING "Expected Ahamkara ABI patch")
set(WISH_ABI_MAJOR_EXPECTED      "${WISH_ABI_MAJOR_EXPECTED}"      CACHE STRING "Expected Wish ABI major")
set(WISH_ABI_MINOR_EXPECTED      "${WISH_ABI_MINOR_EXPECTED}"      CACHE STRING "Expected Wish ABI minor")
set(WISH_PROTO_MAJOR_EXPECTED    "${WISH_PROTO_MAJOR_EXPECTED}"    CACHE STRING "Expected Wish protocol major")
set(WISH_PROTO_MINOR_EXPECTED    "${WISH_PROTO_MINOR_EXPECTED}"    CACHE STRING "Expected Wish protocol minor")

# Fall back to defaults when not overridden
if(NOT AHAMKARA_ABI_MAJOR_EXPECTED)
    set(AHAMKARA_ABI_MAJOR_EXPECTED "${_default_ae_abi_major}")
endif()
if(NOT AHAMKARA_ABI_MINOR_EXPECTED)
    set(AHAMKARA_ABI_MINOR_EXPECTED "${_default_ae_abi_minor}")
endif()
if(NOT AHAMKARA_ABI_PATCH_EXPECTED)
    set(AHAMKARA_ABI_PATCH_EXPECTED "${_default_ae_abi_patch}")
endif()
if(NOT WISH_ABI_MAJOR_EXPECTED)
    set(WISH_ABI_MAJOR_EXPECTED "${_default_wish_abi_major}")
endif()
if(NOT WISH_ABI_MINOR_EXPECTED)
    set(WISH_ABI_MINOR_EXPECTED "${_default_wish_abi_minor}")
endif()
if(NOT WISH_PROTO_MAJOR_EXPECTED)
    set(WISH_PROTO_MAJOR_EXPECTED "${_default_wish_proto_major}")
endif()
if(NOT WISH_PROTO_MINOR_EXPECTED)
    set(WISH_PROTO_MINOR_EXPECTED "${_default_wish_proto_minor}")
endif()

message(STATUS "Cross-product compatibility test")
message(STATUS "  Ahamkara ABI expected: ${AHAMKARA_ABI_MAJOR_EXPECTED}.${AHAMKARA_ABI_MINOR_EXPECTED}.${AHAMKARA_ABI_PATCH_EXPECTED}")
message(STATUS "  Wish ABI expected:     ${WISH_ABI_MAJOR_EXPECTED}.${WISH_ABI_MINOR_EXPECTED}")
message(STATUS "  Wish proto expected:   ${WISH_PROTO_MAJOR_EXPECTED}.${WISH_PROTO_MINOR_EXPECTED}")

check_ahamkara_wish_compatibility(
    AHAMKARA_ABI_MAJOR ${AHAMKARA_ABI_MAJOR_EXPECTED}
    AHAMKARA_ABI_MINOR ${AHAMKARA_ABI_MINOR_EXPECTED}
    AHAMKARA_ABI_PATCH ${AHAMKARA_ABI_PATCH_EXPECTED}
    WISH_ABI_MAJOR     ${WISH_ABI_MAJOR_EXPECTED}
    WISH_ABI_MINOR     ${WISH_ABI_MINOR_EXPECTED}
    WISH_PROTO_MAJOR   ${WISH_PROTO_MAJOR_EXPECTED}
    WISH_PROTO_MINOR   ${WISH_PROTO_MINOR_EXPECTED}
)

# ── Smoke binary: verify runtime compatibility ──────────────────────────
add_executable(compatibility_smoke
    main.cpp
)

target_compile_features(compatibility_smoke PRIVATE cxx_std_20)

target_link_libraries(compatibility_smoke
    PRIVATE
        Ahamkara::Core
        Ahamkara::Runtime
)

enable_testing()
add_test(NAME compatibility_smoke COMMAND compatibility_smoke)

message(STATUS "Cross-product compatibility: all checks passed")
