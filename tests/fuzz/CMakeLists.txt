# ──────────────────────────────────────────────────────────────────────────────
# Fuzz testing targets.
#
# Builds when AHAMKARA_BUILD_FUZZ_TESTS is ON.
# Supports two modes:
#   1. libFuzzer (Clang + -fsanitize=fuzzer) — for thorough fuzzing.
#   2. Standalone driver — for CI sanity checks and compilers without libFuzzer.
#
# Usage:
#   # libFuzzer mode (recommended for local fuzzing):
#   cmake --preset debug \
#     -DAHAMKARA_BUILD_FUZZ_TESTS=ON \
#     -DCMAKE_CXX_COMPILER=clang++-21
#   cmake --build build/debug --target ahamkara_fuzz_udp_packet_parse
#   ./build/debug/tests/fuzz/ahamkara_fuzz_udp_packet_parse \
#     -max_total_time=30 -runs=100000
#
#   # Standalone mode (any compiler):
#   cmake --preset debug -DAHAMKARA_BUILD_FUZZ_TESTS=ON
#   cmake --build build/debug --target ahamkara_fuzz_udp_packet_parse
#   ./build/debug/tests/fuzz/ahamkara_fuzz_udp_packet_parse
#   ./build/debug/tests/fuzz/ahamkara_fuzz_udp_packet_parse /path/to/crash
# ──────────────────────────────────────────────────────────────────────────────

# Detect libFuzzer support.
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-fsanitize=fuzzer" CXX_SUPPORTS_FUZZER)

if(CXX_SUPPORTS_FUZZER)
    set(FUZZ_SANITIZE_FLAGS "-fsanitize=fuzzer,address" "-fno-omit-frame-pointer")
    set(FUZZ_DRIVER_SRC "")
    set(FUZZ_MODE "libFuzzer + AddressSanitizer")
    # libFuzzer CLI flags for CTest sanity checks.
    set(FUZZ_CTEST_ARGS "-max_total_time=5" "-runs=10000" "-print_pcs=0")
else()
    # Standalone driver (no libFuzzer).  Compiles with any compiler but without
    # ASan so it works even when the Clang-rt runtime is not installed.
    set(FUZZ_SANITIZE_FLAGS "-fno-omit-frame-pointer")
    set(FUZZ_DRIVER_SRC "fuzz_standalone_driver.cpp")
    set(FUZZ_MODE "standalone driver")
    # Standalone driver flags.
    set(FUZZ_CTEST_ARGS "-runs=10000")
endif()

# Common include directories.
set(FUZZ_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../engine/core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../engine/network/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../game/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../wish/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../client/include
)

# Common library dependencies.
set(FUZZ_LIBS
    ae_network
    ahamkara_game
    ae_core
)

# ---- Helper: define a fuzz target -------------------------------------------
function(add_ahamkara_fuzz_target TARGET_NAME FUZZ_SRC)
    add_executable(${TARGET_NAME}
        ${FUZZ_SRC}
        ${FUZZ_DRIVER_SRC}
    )

    target_compile_features(${TARGET_NAME} PRIVATE cxx_std_20)
    target_include_directories(${TARGET_NAME} PRIVATE ${FUZZ_INCLUDES})

    target_compile_options(${TARGET_NAME}
        PRIVATE
            ${FUZZ_SANITIZE_FLAGS}
            -g
    )
    target_link_options(${TARGET_NAME}
        PRIVATE
            ${FUZZ_SANITIZE_FLAGS}
    )

    target_link_libraries(${TARGET_NAME} PRIVATE ${FUZZ_LIBS})

    # Register a short CTest sanity check.
    add_test(
        NAME ${TARGET_NAME}_sanity
        COMMAND ${TARGET_NAME} ${FUZZ_CTEST_ARGS}
    )
endfunction()

# ---- Fuzz target: UDP packet parsing ----------------------------------------
add_ahamkara_fuzz_target(ahamkara_fuzz_udp_packet_parse
    fuzz_udp_packet_parse.cpp
)

# ---- Fuzz target: game state serialization ----------------------------------
add_ahamkara_fuzz_target(ahamkara_fuzz_game_state_serialization
    fuzz_game_state_serialization.cpp
)

message(STATUS "Fuzz targets enabled (mode: ${FUZZ_MODE})")
