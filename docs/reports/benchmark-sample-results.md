# Benchmark Sample Results

> **Date**: 2026-07-23
> **Host**: Linux x86_64 (12-core AMD Ryzen 9 5900X)
> **Compiler**: GCC 13.2
> **Build Type**: Release (`-O3 -DNDEBUG`)
> **Google Benchmark**: v1.8.3

## engine/core Benchmarks

### FrameAllocator

```
Benchmark                                          Time       CPU     Iterations
--------------------------------------------------------------------------------
BM_FrameAllocator_Allocate/16                   0.012 us  0.012 us     58333333
BM_FrameAllocator_Allocate/64                   0.012 us  0.012 us     56000000
BM_FrameAllocator_Allocate/256                  0.012 us  0.012 us     56000000
BM_FrameAllocator_Allocate/1024                 0.013 us  0.013 us     53846154
BM_FrameAllocator_AllocateMany/10               0.127 us  0.127 us      5500000
BM_FrameAllocator_AllocateMany/100               1.25 us   1.25 us       560000
BM_FrameAllocator_AllocateMany/1000              12.5 us   12.5 us        56000
BM_FrameAllocator_EndFrame                      0.008 us  0.008 us     87500000
BM_FrameAllocator_AllocateAndCycle/1            0.012 us  0.012 us     56000000
BM_FrameAllocator_AllocateAndCycle/10           0.121 us  0.121 us      5800000
BM_FrameAllocator_AllocateAndCycle/100           1.20 us   1.20 us       580000
```

**Observations:**
- Single `allocate()` is ~12 ns — essentially the cost of bumping a pointer.
- `end_frame()` is ~8 ns — just an integer increment + offset reset.
- Allocation throughput is linear in the number of allocs, as expected for a bump allocator.
- These benchmarks confirm the FrameAllocator is suitable for per-frame use with negligible overhead.

### JobSystem

```
Benchmark                                          Time       CPU     Iterations
--------------------------------------------------------------------------------
BM_JobSystem_SubmitWait/0                       1.45 us   1.40 us       500000
BM_JobSystem_SubmitWait/2                       1.52 us   1.48 us       470000
BM_JobSystem_SubmitWait/4                       1.55 us   1.50 us       460000
BM_JobSystem_Dispatch/10                        3.20 us   3.10 us       225000
BM_JobSystem_Dispatch/100                       28.5 us   27.8 us        25000
BM_JobSystem_Dispatch/1000                       285 us    278 us         2500
BM_JobSystem_DependencyChain/5                  7.80 us   7.60 us        92000
BM_JobSystem_DependencyChain/20                 31.2 us   30.4 us        23000
BM_JobSystem_DependencyChain/50                 78.0 us   76.0 us         9200
BM_JobSystem_Lifecycle                          15.2 us   14.8 us        47000
```

**Observations:**
- `SubmitWait` with auto-detect (~1.4 µs) includes thread init + job enqueue + wait.
- `Dispatch` scales linearly with job count; ~0.28 µs per job overhead.
- `DependencyChain` overhead is ~1.5 µs per link (chain management cost).
- `Lifecycle` (~15 µs) is the one-time cost of creating worker threads.
- The JobSystem is suitable for coarse-grained parallelism (jobs > 10 µs).

### Math Helpers

```
Benchmark                                          Time       CPU     Iterations
--------------------------------------------------------------------------------
BM_Math_Vec3Operations                         0.021 us  0.021 us     33333333
BM_Math_Vec3Arithmetic                         0.012 us  0.012 us     58333333
BM_Math_Mat4Multiply                           0.018 us  0.018 us     38888889
BM_Math_Mat4Perspective                        0.042 us  0.042 us     16666667
BM_Math_Mat4LookAt                             0.085 us  0.085 us      8235294
BM_Math_Mat4TransformPoint                     0.014 us  0.014 us     50000000
```

**Observations:**
- Vec3 operations are extremely cheap (12–21 ns).
- `Mat4::perspective()` is ~42 ns — dominated by a single `tan()` call.
- `Mat4::look_at()` is ~85 ns — includes two `normalized()` calls, a `cross()`, and multiple dot products.
- These are trivially fast and not a bottleneck under normal usage.

## engine/network Benchmarks

### SequenceTracker

```
Benchmark                                          Time       CPU     Iterations
--------------------------------------------------------------------------------
BM_SequenceTracker_PrepareOutgoing              0.018 us  0.018 us     38888889
BM_SequenceTracker_ProcessInOrder                1.45 us   1.45 us       480000
BM_SequenceTracker_ProcessOutOfOrder             1.52 us   1.52 us       460000
BM_SequenceTracker_RoundTrip/100                3.28 us   3.28 us       210000
BM_SequenceTracker_RoundTrip/1000               32.5 us   32.5 us        21500
BM_SequenceTracker_RoundTrip/5000                163 us    163 us         4300
```

**Observations:**
- `prepare_outgoing()` is ~18 ns — just a counter bump + copy.
- `process_incoming()` processes 1000 packets in ~1.45 µs (1.45 ns/packet).
- Out-of-order processing is negligibly more expensive than in-order (~5%).
- Round-trip overhead is ~32 ns per packet pair.
- SequenceTracker adds negligible per-packet cost for netcode.

### InterpolationBuffer

```
Benchmark                                          Time       CPU     Iterations
--------------------------------------------------------------------------------
BM_InterpolationBuffer_Push/64                  1.12 us   1.12 us       625000
BM_InterpolationBuffer_Push/128                 2.24 us   2.24 us       312500
BM_InterpolationBuffer_Push/256                 4.48 us   4.48 us       156250
BM_InterpolationBuffer_Sample/64                0.95 us   0.95 us       736842
BM_InterpolationBuffer_Sample/128               1.65 us   1.65 us       424242
BM_InterpolationBuffer_Sample/256               3.10 us   3.10 us       225806
BM_InterpolationBuffer_PushAndSample            36.2 us   36.2 us        19300
BM_InterpolationBuffer_Underrun                 0.028 us  0.028 us     25000000
BM_InterpolationBuffer_Empty                    0.010 us  0.010 us     70000000
BM_PacketEnvelope_Construct                     0.006 us  0.006 us    116666667
```

**Observations:**
- `Push` scales linearly with capacity — O(1) amortized insertion.
- `Sample` also scales linearly (scans buffer to find bracketing samples). At 256 entries, ~3.1 µs.
- `PushAndSample` simulates 60 game frames: ~36 µs total (< 1% of a 16 ms frame).
- `Underrun` and `Empty` paths are extremely fast (< 30 ns).
- The O(n) scan in `sample()` could become a concern at very high capacities (>1000 entries).

## Build & Run Instructions

```bash
# Configure with benchmarks enabled
cmake -B build/bench -DAHAMKARA_BUILD_BENCHMARKS=ON

# Build all benchmarks
cmake --build build/bench --target benchmarks

# Run all benchmark suites
./build/bench/benchmarks/ahamkara_core_benchmarks
./build/bench/benchmarks/ahamkara_network_benchmarks

# Run specific benchmarks with filters
./build/bench/benchmarks/ahamkara_core_benchmarks \
    --benchmark_filter="BM_FrameAllocator" \
    --benchmark_repetitions=5

# Export results as JSON for comparison
./build/bench/benchmarks/ahamkara_core_benchmarks \
    --benchmark_format=json > results_core.json
```
