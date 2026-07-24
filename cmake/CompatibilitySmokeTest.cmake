# CompatibilitySmokeTest.cmake
#
# Clean-room cross-product compatibility smoke test driver.
#
# This script is invoked in an environment that has pre-built Ahamkara +
# Wish package artifacts extracted on CMAKE_PREFIX_PATH but NO source
# checkout of either product.
#
# Phases:
#   1. Compatible-version build: configures the consumer with default
#      expected versions, builds, and runs both smoke binaries.
#   2. Incompatible-version build: re-configures with deliberately wrong
#      version constants and verifies the build FAILS at compile time.
#
# Required -D arguments:
#   CONSUMER_SRC   = path to tests/compatibility_consumer/
#   PREFIX         = installation prefix containing Ahamkara + Wish packages
#   BUILD_DIR      = build directory for the consumer

if(NOT DEFINED CONSUMER_SRC OR NOT DEFINED PREFIX OR NOT DEFINED BUILD_DIR)
    message(FATAL_ERROR "CompatibilitySmokeTest.cmake missing required -D args")
endif()

set(_compat_build     "${BUILD_DIR}/compat")
set(_mismatch_build   "${BUILD_DIR}/mismatch")
set(_prefix           "${PREFIX}")

file(REMOVE_RECURSE "${_compat_build}" "${_mismatch_build}")

macro(_configure _build_dir _extra_defines)
    execute_process(
        COMMAND ${CMAKE_COMMAND}
            -S "${CONSUMER_SRC}"
            -B "${_build_dir}"
            -G Ninja
            -DCMAKE_BUILD_TYPE=Debug
            -DCMAKE_PREFIX_PATH=${_prefix}
            ${_extra_defines}
        RESULT_VARIABLE _config_rc
        OUTPUT_VARIABLE _config_out
        ERROR_VARIABLE _config_err
    )
    if(NOT _config_rc EQUAL 0)
        message(FATAL_ERROR
            "CompatibilitySmoke: configure FAILED (rc=${_config_rc}) for ${_build_dir}\n"
            "stdout:\n${_config_out}\nstderr:\n${_config_err}")
    endif()
endmacro()

macro(_build _build_dir)
    execute_process(
        COMMAND ${CMAKE_COMMAND} --build "${_build_dir}"
        RESULT_VARIABLE _build_rc
        OUTPUT_VARIABLE _build_out
        ERROR_VARIABLE _build_err
    )
    set(_build_result ${_build_rc})
endmacro()

macro(_run _build_dir _binary)
    execute_process(
        COMMAND "${_build_dir}/${_binary}"
        RESULT_VARIABLE _run_rc
        OUTPUT_VARIABLE _run_out
        ERROR_VARIABLE _run_err
    )
endmacro()

# ==========================================================================
# Phase 1 — Compatible-version
# ==========================================================================
message(STATUS "")
message(STATUS "═══ Phase 1: Compatible version build + test ═══")
message(STATUS "")

_configure("${_compat_build}" "")
message(STATUS "Building compatible targets...")
_build("${_compat_build}")
if(_build_result)
    message(FATAL_ERROR "Compatible build FAILED (rc=${_build_result})")
endif()

message(STATUS "Running compatibility_consumer...")
_run("${_compat_build}" "compatibility_consumer")
if(NOT _run_rc EQUAL 0)
    message(FATAL_ERROR "compatibility_consumer FAILED (rc=${_run_rc})")
endif()
message(STATUS "compatibility_consumer: PASSED")

message(STATUS "Running flashback_headless_smoke...")
_run("${_compat_build}" "flashback_headless_smoke")
if(NOT _run_rc EQUAL 0)
    message(FATAL_ERROR "flashback_headless_smoke FAILED (rc=${_run_rc})")
endif()
message(STATUS "flashback_headless_smoke: PASSED")

# ==========================================================================
# Phase 2 — Incompatible-version (expect build failure)
# ==========================================================================
message(STATUS "")
message(STATUS "═══ Phase 2: Incompatible version build (expect FAILURE) ═══")
message(STATUS "")

_configure("${_mismatch_build}"
    "-DAE_ABI_VERSION_OVERRIDE=999"
    "-DWISH_ABI_VERSION_OVERRIDE=999"
    "-DAE_NET_PROTOCOL_OVERRIDE=999"
    "-DWISH_SESSION_PROTOCOL_OVERRIDE=999"
)

message(STATUS "Building intentionally-incompatible targets (should fail)...")
_build("${_mismatch_build}")

if(_build_result EQUAL 0)
    message(FATAL_ERROR
        "INCOMPATIBLE build unexpectedly SUCCEEDED. "
        "The version-check static_assert should have caught the mismatch.\n"
        "Check that version.h constants are being referenced correctly.")
else()
    message(STATUS "Incompatible build correctly FAILED (build result = ${_build_result})")
    message(STATUS "")
    message(STATUS "Build error log:")
    message(STATUS "${_build_err}")
endif()

# ==========================================================================
# Summary
# ==========================================================================
message(STATUS "")
message(STATUS "══════════════════════════════════════════════════════════════")
message(STATUS "  Compatible artifacts ............ PASSED (build + 2 tests)")
message(STATUS "  Incompatible artifacts .......... PASSED (build rejected)")
message(STATUS "══════════════════════════════════════════════════════════════")
message(STATUS "All cross-product compatibility smoke checks PASSED")
