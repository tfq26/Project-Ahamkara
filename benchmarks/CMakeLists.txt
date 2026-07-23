# ---------------------------------------------------------------------------
# Ahamkara Microbenchmarks
#
# These benchmarks use Google Benchmark and are NOT registered with CTest.
# They are intended for ad-hoc performance investigation and regression
# detection during development.
#
# Build with:
#   cmake -B build/bench -DAHAMKARA_BUILD_BENCHMARKS=ON
#   cmake --build build/bench --target ahamkara_core_benchmarks
#   cmake --build build/bench --target ahamkara_network_benchmarks
#
# Or build all benchmark targets:
#   cmake --build build/bench --target benchmarks
#
# Run individual benchmarks:
#   ./build/bench/benchmarks/ahamkara_core_benchmarks
#   ./build/bench/benchmarks/ahamkara_network_benchmarks
#
# For detailed output (filter by name, set iterations, etc.):
#   ./build/bench/benchmarks/ahamkara_core_benchmarks \
#       --benchmark_filter="BM_FrameAllocator" \
#       --benchmark_repetitions=5
# ---------------------------------------------------------------------------

# -- Core Benchmarks ---------------------------------------------------------
add_executable(ahamkara_core_benchmarks
    src/core_benchmarks.cpp
)

target_compile_features(ahamkara_core_benchmarks PRIVATE cxx_std_20)
target_include_directories(ahamkara_core_benchmarks
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../engine/core/include
)

target_link_libraries(ahamkara_core_benchmarks
    PRIVATE
        ae_core
        benchmark::benchmark
)

# -- Network Benchmarks ------------------------------------------------------
add_executable(ahamkara_network_benchmarks
    src/network_benchmarks.cpp
)

target_compile_features(ahamkara_network_benchmarks PRIVATE cxx_std_20)
target_include_directories(ahamkara_network_benchmarks
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../engine/core/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../engine/network/include
)

target_link_libraries(ahamkara_network_benchmarks
    PRIVATE
        ae_network
        ae_core
        benchmark::benchmark
)

# -- Convenience target ------------------------------------------------------
# Build ALL benchmarks via: cmake --build . --target benchmarks
add_custom_target(benchmarks
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target ahamkara_core_benchmarks --target ahamkara_network_benchmarks
    COMMENT "Building all Ahamkara benchmarks"
)
