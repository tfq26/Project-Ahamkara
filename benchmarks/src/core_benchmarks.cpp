// ============================================================================
// Ahamkara — engine/core Microbenchmarks
//
// Benchmark hot paths in the core engine library:
//   - FrameAllocator  (linear bump allocator with ring-buffer slots)
//   - JobSystem       (multi-threaded work-stealing job dispatcher)
//   - Math helpers    (Vec3, Mat4, perspective, look_at)
//
// Build and run:
//   cmake -B build/bench -DAHAMKARA_BUILD_BENCHMARKS=ON
//   cmake --build build/bench --target ahamkara_core_benchmarks
//   ./build/bench/benchmarks/ahamkara_core_benchmarks
// ============================================================================

#include <benchmark/benchmark.h>

#include "ae/core/frame_allocator.h"
#include "ae/core/job_system.h"
#include "ae/core/math.h"

#include <atomic>
#include <chrono>
#include <random>
#include <vector>

// ============================================================================
// FrameAllocator Benchmarks
// ============================================================================

// Baseline: allocate a single block in the current frame slot.
static void BM_FrameAllocator_Allocate(benchmark::State& state) {
    ae::FrameAllocator alloc(64 * 1024 * 1024, 3);  // 64 MB arena, 3 slots
    alloc.end_frame();

    const std::size_t block_size = static_cast<std::size_t>(state.range(0));
    for (auto _ : state) {
        void* p = alloc.allocate(block_size, alignof(std::max_align_t));
        benchmark::DoNotOptimize(p);
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_FrameAllocator_Allocate)
    ->Arg(16)
    ->Arg(64)
    ->Arg(256)
    ->Arg(1024)
    ->Unit(benchmark::kMicrosecond);

// Benchmark: allocate many small blocks sequentially in one frame.
static void BM_FrameAllocator_AllocateMany(benchmark::State& state) {
    ae::FrameAllocator alloc(64 * 1024 * 1024, 3);
    alloc.end_frame();

    const int num_allocations = state.range(0);
    std::vector<void*> ptrs(static_cast<std::size_t>(num_allocations));

    for (auto _ : state) {
        for (int i = 0; i < num_allocations; ++i) {
            ptrs[static_cast<std::size_t>(i)] = alloc.allocate(64, alignof(std::max_align_t));
        }
        benchmark::DoNotOptimize(ptrs.data());
        benchmark::ClobberMemory();
        alloc.reset_all();
        alloc.end_frame();
    }
}
BENCHMARK(BM_FrameAllocator_AllocateMany)
    ->Arg(10)
    ->Arg(100)
    ->Arg(1000)
    ->Unit(benchmark::kMicrosecond);

// Benchmark: end_frame rotation (ring-buffer advance). Measure overhead of
// advancing the slot pointer and resetting the next slot's offset.
static void BM_FrameAllocator_EndFrame(benchmark::State& state) {
    ae::FrameAllocator alloc(64 * 1024 * 1024, 3);

    for (auto _ : state) {
        alloc.end_frame();
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_FrameAllocator_EndFrame)
    ->Unit(benchmark::kMicrosecond);

// Benchmark: allocate-then-end_frame cycle, simulating a per-frame allocator
// pattern used in game loops.
static void BM_FrameAllocator_AllocateAndCycle(benchmark::State& state) {
    ae::FrameAllocator alloc(64 * 1024 * 1024, 3);

    const int allocs_per_frame = state.range(0);
    for (auto _ : state) {
        for (int i = 0; i < allocs_per_frame; ++i) {
            benchmark::DoNotOptimize(alloc.allocate(64, alignof(std::max_align_t)));
        }
        alloc.end_frame();
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_FrameAllocator_AllocateAndCycle)
    ->Arg(1)
    ->Arg(10)
    ->Arg(100)
    ->Unit(benchmark::kMicrosecond);

// ============================================================================
// JobSystem Benchmarks
// ============================================================================

// Benchmark: submit and wait for a single empty job.
static void BM_JobSystem_SubmitWait(benchmark::State& state) {
    const int thread_count = state.range(0);

    for (auto _ : state) {
        ae::JobSystem js;
        js.init(thread_count);
        auto h = js.submit([]() { /* no-op */ });
        js.wait(h);
        js.shutdown();
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_JobSystem_SubmitWait)
    ->Arg(0)    // auto-detect
    ->Arg(2)
    ->Arg(4)
    ->Unit(benchmark::kMicrosecond);

// Benchmark: submit many independent jobs (dispatch) and wait for all.
static void BM_JobSystem_Dispatch(benchmark::State& state) {
    const int num_jobs = state.range(0);

    ae::JobSystem js;
    js.init(4);

    std::atomic<int> counter{0};
    for (auto _ : state) {
        counter.store(0, std::memory_order_relaxed);
        auto handles = js.dispatch(num_jobs, [&counter](int) {
            counter.fetch_add(1, std::memory_order_relaxed);
        });
        for (auto& h : handles) {
            js.wait(h);
        }
        benchmark::ClobberMemory();
    }
    js.shutdown();
}
BENCHMARK(BM_JobSystem_Dispatch)
    ->Arg(10)
    ->Arg(100)
    ->Arg(1000)
    ->Unit(benchmark::kMicrosecond);

// Benchmark: submit dependent jobs (chain of length N).
static void BM_JobSystem_DependencyChain(benchmark::State& state) {
    const int chain_length = state.range(0);

    for (auto _ : state) {
        ae::JobSystem js;
        js.init(2);

        ae::JobSystem::JobHandle prev;
        for (int i = 0; i < chain_length; ++i) {
            if (i == 0) {
                prev = js.submit([]() { /* no-op */ });
            } else {
                prev = js.submit_after(prev, []() { /* no-op */ });
            }
        }
        js.wait(prev);
        js.shutdown();
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_JobSystem_DependencyChain)
    ->Arg(5)
    ->Arg(20)
    ->Arg(50)
    ->Unit(benchmark::kMicrosecond);

// Benchmark: lifecycle overhead — init, submit one job, shutdown (creates and
// destroys worker threads each iteration).  This stresses thread creation and
// the internal job queue initialisation paths.
static void BM_JobSystem_Lifecycle(benchmark::State& state) {
    (void)state;
    for (auto _ : state) {
        ae::JobSystem js;
        js.init(2);
        [[maybe_unused]] auto h = js.submit([]() { /* no-op */ });
        js.shutdown();
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_JobSystem_Lifecycle)
    ->Unit(benchmark::kMicrosecond);

// ============================================================================
// Math Benchmarks
// ============================================================================

// Benchmark: Vec3 length(), normalized(), dot(), cross().
static void BM_Math_Vec3Operations(benchmark::State& state) {
    ae::Vec3 a(1.0F, 2.0F, 3.0F);
    ae::Vec3 b(4.0F, 5.0F, 6.0F);

    float result_float = 0.0F;
    ae::Vec3 result_vec;

    for (auto _ : state) {
        result_float = a.length();
        benchmark::DoNotOptimize(result_float);

        result_vec = a.normalized();
        benchmark::DoNotOptimize(result_vec);

        result_float = ae::dot(a, b);
        benchmark::DoNotOptimize(result_float);

        result_vec = ae::cross(a, b);
        benchmark::DoNotOptimize(result_vec);

        result_vec = a + b;
        benchmark::DoNotOptimize(result_vec);

        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_Math_Vec3Operations)
    ->Unit(benchmark::kMicrosecond);

// Benchmark: Vec3 arithmetic — additions, subtractions, scalar multiplication.
static void BM_Math_Vec3Arithmetic(benchmark::State& state) {
    ae::Vec3 a(1.0F, 2.0F, 3.0F);
    ae::Vec3 result;

    for (auto _ : state) {
        result = a + a;
        benchmark::DoNotOptimize(result);
        result = a - a;
        benchmark::DoNotOptimize(result);
        result = a * 2.0F;
        benchmark::DoNotOptimize(result);
        result = a / 2.0F;
        benchmark::DoNotOptimize(result);
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_Math_Vec3Arithmetic)
    ->Unit(benchmark::kMicrosecond);

// Benchmark: Mat4 multiplication.
static void BM_Math_Mat4Multiply(benchmark::State& state) {
    ae::Mat4 a = ae::Mat4::identity();
    ae::Mat4 b = ae::Mat4::translation(ae::Vec3(10.0F, 20.0F, 30.0F));
    ae::Mat4 result;

    for (auto _ : state) {
        result = a * b;
        benchmark::DoNotOptimize(result);
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_Math_Mat4Multiply)
    ->Unit(benchmark::kMicrosecond);

// Benchmark: Mat4 perspective() construction.
static void BM_Math_Mat4Perspective(benchmark::State& state) {
    ae::Mat4 result;
    for (auto _ : state) {
        result = ae::Mat4::perspective(90.0F, 16.0F / 9.0F, 0.1F, 1000.0F);
        benchmark::DoNotOptimize(result);
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_Math_Mat4Perspective)
    ->Unit(benchmark::kMicrosecond);

// Benchmark: Mat4 look_at() construction.
static void BM_Math_Mat4LookAt(benchmark::State& state) {
    ae::Vec3 eye(0.0F, 10.0F, 20.0F);
    ae::Vec3 target(0.0F, 0.0F, 0.0F);
    ae::Vec3 up(0.0F, 1.0F, 0.0F);
    ae::Mat4 result;

    for (auto _ : state) {
        result = ae::Mat4::look_at(eye, target, up);
        benchmark::DoNotOptimize(result);
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_Math_Mat4LookAt)
    ->Unit(benchmark::kMicrosecond);

// Benchmark: Mat4::transform_point — apply a transformation matrix to a point.
static void BM_Math_Mat4TransformPoint(benchmark::State& state) {
    ae::Mat4 m = ae::Mat4::translation(ae::Vec3(5.0F, 10.0F, 15.0F));
    ae::Vec3 point(1.0F, 2.0F, 3.0F);
    ae::Vec3 result;

    for (auto _ : state) {
        result = m.transform_point(point);
        benchmark::DoNotOptimize(result);
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_Math_Mat4TransformPoint)
    ->Unit(benchmark::kMicrosecond);

// ============================================================================
// Main
// ============================================================================

BENCHMARK_MAIN();
