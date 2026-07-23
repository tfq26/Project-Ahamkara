# Benchmarking

Ahamkara uses [Google Benchmark](https://github.com/google/benchmark) for
microbenchmarking engine and game hot paths. Benchmarks are **not** part of the
normal test suite or CI pipeline — they must be explicitly enabled at configure
time.

## Quick Start

```sh
# Configure with benchmarks enabled
cmake -B build/bench -DAHAMKARA_BUILD_BENCHMARKS=ON

# Build all benchmarks
cmake --build build/bench --target benchmarks

# Run engine/core benchmarks
./build/bench/benchmarks/ahamkara_core_benchmarks

# Run engine/network benchmarks
./build/bench/benchmarks/ahamkara_network_benchmarks
```

## Build Targets

| Target                          | Description                           |
|---------------------------------|---------------------------------------|
| `ahamkara_core_benchmarks`      | engine/core benchmarks                |
| `ahamkara_network_benchmarks`   | engine/network benchmarks             |
| `benchmarks` (alias)            | Build all benchmark targets           |

## Running Benchmarks

### Basic Run

```sh
./build/bench/benchmarks/ahamkara_core_benchmarks
```

### Filter by Name

```sh
./build/bench/benchmarks/ahamkara_core_benchmarks \
    --benchmark_filter="BM_FrameAllocator"
```

### Adjust Iterations / Repetitions

```sh
./build/bench/benchmarks/ahamkara_core_benchmarks \
    --benchmark_repetitions=10 \
    --benchmark_min_time=1.0
```

### Export Results as JSON

```sh
./build/bench/benchmarks/ahamkara_core_benchmarks \
    --benchmark_format=json > results_core.json
```

### Compare Two Runs

```sh
./build/bench/ahamkara_core_benchmarks \
    --benchmark_format=json > before.json
# make changes, rebuild, run again
./build/bench/ahamkara_core_benchmarks \
    --benchmark_format=json > after.json
python -m json.tool before.json after.json  # or use your own comparison script
```

## Adding New Benchmarks

### 1. Create a new benchmark source file

Place benchmarks in `benchmarks/src/`. Follow the naming convention:

- `benchmarks/src/<module>_benchmarks.cpp`

### 2. Write a benchmark

```cpp
#include <benchmark/benchmark.h>
#include "ae/core/my_component.h"

static void BM_MyFunction(benchmark::State& state) {
    // Setup (runs once per thread)
    MyComponent comp;
    comp.init();

    for (auto _ : state) {
        // Code under measurement
        comp.hot_path();
        benchmark::DoNotOptimize(comp);
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_MyFunction)
    ->Arg(64)          // Parameterised benchmark
    ->Unit(benchmark::kMicrosecond);

// ---

BENCHMARK_MAIN();
```

### 3. Register the target in `benchmarks/CMakeLists.txt`

```cmake
add_executable(ahamkara_my_module_benchmarks
    src/my_module_benchmarks.cpp
)

target_compile_features(ahamkara_my_module_benchmarks PRIVATE cxx_std_20)
target_include_directories(ahamkara_my_module_benchmarks
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../engine/my_module/include
)

target_link_libraries(ahamkara_my_module_benchmarks
    PRIVATE
        ae_my_module
        benchmark::benchmark
)
```

### 4. Update the `benchmarks` convenience target (optional)

Add your new target to the `COMMAND` list of the custom `benchmarks` target:

```cmake
add_custom_target(benchmarks
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
        --target ahamkara_core_benchmarks
        --target ahamkara_network_benchmarks
        --target ahamkara_my_module_benchmarks
    COMMENT "Building all Ahamkara benchmarks"
)
```

## Best Practices

1. **Keep benchmarks focused** — measure one code path per benchmark function.
2. **Use `DoNotOptimize` / `ClobberMemory`** to prevent the compiler from
   eliding the code under test.
3. **Parameterise with `->Arg(N)`** or `->Range(start, end)` to test multiple
   input sizes.
4. **Don't include setup in the measured loop** — put setup before `for (auto _ : state)`.
5. **Use meaningful units** — `benchmark::kMicrosecond` for sub-millisecond,
   `benchmark::kMillisecond` for longer operations.
6. **Run in Release mode** for realistic results: `-DCMAKE_BUILD_TYPE=Release`.
7. **Pin CPU frequency** (Linux) for reproducible numbers:
   ```sh
   sudo cpupower frequency-set --governor performance
   ```
8. **Document results** — when investigating a regression, save JSON output
   and add a note to `docs/reports/`.

## Disabling Benchmarks

Benchmarks are disabled by default. They do NOT run during:

- `ctest` (not registered with CTest)
- Normal CI builds (the `AHAMKARA_BUILD_BENCHMARKS` option is OFF)
- `cmake --build --preset debug` (unless explicitly enabled)

To re-enable, pass `-DAHAMKARA_BUILD_BENCHMARKS=ON` at configure time.
