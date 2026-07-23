// ============================================================================
// Ahamkara — engine/network Microbenchmarks
//
// Benchmark hot paths in the network library:
//   - SequenceTracker      (per-packet sequence/ACK bookkeeping)
//   - InterpolationBuffer  (template circular-buffer snapshot interpolation)
//
// Build and run:
//   cmake -B build/bench -DAHAMKARA_BUILD_BENCHMARKS=ON
//   cmake --build build/bench --target ahamkara_network_benchmarks
//   ./build/bench/benchmarks/ahamkara_network_benchmarks
// ============================================================================

#include <benchmark/benchmark.h>

#include "ae/network/sequence_tracker.h"
#include "ae/network/interpolation_buffer.h"
#include "ae/network/packet_envelope.h"

#include <random>
#include <vector>

// ============================================================================
// SequenceTracker Benchmarks
// ============================================================================

// Benchmark: prepare_outgoing() — the cheapest path: just bump a counter and
// copy latest ACK state into an envelope.
static void BM_SequenceTracker_PrepareOutgoing(benchmark::State& state) {
    ae::SequenceTracker tracker;

    for (auto _ : state) {
        auto env = tracker.prepare_outgoing();
        benchmark::DoNotOptimize(env);
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_SequenceTracker_PrepareOutgoing)
    ->Unit(benchmark::kMicrosecond);

// Benchmark: process_incoming() with in-order packets (no gaps).  This is the
// common-case path where every packet arrives in sequence.
static void BM_SequenceTracker_ProcessInOrder(benchmark::State& state) {
    ae::SequenceTracker tracker;

    for (auto _ : state) {
        for (ae::u16 seq = 0; seq < 1000; ++seq) {
            ae::PacketEnvelope env;
            env.sequence = seq;
            env.ack_sequence = 0;
            env.ack_bitfield = 0;
            tracker.process_incoming(env);
        }
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_SequenceTracker_ProcessInOrder)
    ->Unit(benchmark::kMicrosecond);

// Benchmark: process_incoming() with out-of-order delivery (worst-case for
// the ack_bitfield management).  Every packet arrives after a random delay,
// causing a mix of new and old sequence paths.
static void BM_SequenceTracker_ProcessOutOfOrder(benchmark::State& state) {
    ae::SequenceTracker tracker;
    std::mt19937 rng(42);
    constexpr int kNumPackets = 1000;

    // Generate a shuffled sequence of packet numbers.
    std::vector<ae::u16> packets(kNumPackets);
    for (int i = 0; i < kNumPackets; ++i) {
        packets[static_cast<std::size_t>(i)] = static_cast<ae::u16>(i);
    }
    std::shuffle(packets.begin(), packets.end(), rng);

    for (auto _ : state) {
        for (ae::u16 seq : packets) {
            ae::PacketEnvelope env;
            env.sequence = seq;
            env.ack_sequence = 0;
            env.ack_bitfield = 0;
            tracker.process_incoming(env);
        }
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_SequenceTracker_ProcessOutOfOrder)
    ->Unit(benchmark::kMicrosecond);

// Benchmark: prepare_outgoing + process_incoming round-trip, simulating the
// full per-packet bookkeeping overhead on both sides of a connection.
static void BM_SequenceTracker_RoundTrip(benchmark::State& state) {
    const int num_packets = state.range(0);
    ae::SequenceTracker client_tracker;
    ae::SequenceTracker server_tracker;

    for (auto _ : state) {
        for (int i = 0; i < num_packets; ++i) {
            // Client sends → server receives
            auto client_env = client_tracker.prepare_outgoing();
            server_tracker.process_incoming(client_env);

            // Server sends → client receives
            auto server_env = server_tracker.prepare_outgoing();
            client_tracker.process_incoming(server_env);
        }
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_SequenceTracker_RoundTrip)
    ->Arg(100)
    ->Arg(1000)
    ->Arg(5000)
    ->Unit(benchmark::kMicrosecond);

// ============================================================================
// InterpolationBuffer Benchmarks
// ============================================================================

// A trivial sample type for interpolation benchmarks.
struct SampleVec3 {
    double x, y, z;
};

// Linear interpolation for SampleVec3.
SampleVec3 LerpVec3(const SampleVec3& a, const SampleVec3& b, float t) {
    return {
        a.x + (b.x - a.x) * static_cast<double>(t),
        a.y + (b.y - a.y) * static_cast<double>(t),
        a.z + (b.z - a.z) * static_cast<double>(t)
    };
}

// Benchmark: push samples into the interpolation buffer (common case —
// monotonically increasing timestamps within the capacity).
static void BM_InterpolationBuffer_Push(benchmark::State& state) {
    const int num_samples = state.range(0);

    ae::InterpolationConfig cfg;
    cfg.capacity = 256;
    auto buf = ae::InterpolationBuffer<SampleVec3>(LerpVec3, cfg);

    double ts = 0.0;
    for (auto _ : state) {
        for (int i = 0; i < num_samples; ++i) {
            ts += 0.016;  // ~60 FPS intervals
            buf.push(ts, SampleVec3{1.0, 2.0, 3.0});
        }
        benchmark::ClobberMemory();
        buf.reset();
        ts = 0.0;
    }
}
BENCHMARK(BM_InterpolationBuffer_Push)
    ->Arg(64)
    ->Arg(128)
    ->Arg(256)
    ->Unit(benchmark::kMicrosecond);

// Benchmark: sample (interpolate) from the buffer — exercises the bracket
// search and lerp call for the normal interpolation case.
static void BM_InterpolationBuffer_Sample(benchmark::State& state) {
    const int num_samples = state.range(0);

    ae::InterpolationConfig cfg;
    cfg.capacity = 256;
    cfg.delay_seconds = 0.1;
    auto buf = ae::InterpolationBuffer<SampleVec3>(LerpVec3, cfg);

    // Pre-fill with num_samples entries at 60 Hz.
    double ts = 0.0;
    for (int i = 0; i < num_samples; ++i) {
        ts += 0.016;
        buf.push(ts, SampleVec3{static_cast<double>(i), 2.0, 3.0});
    }

    // Sample at various render times within range.
    double render_time = 0.05;
    SampleVec3 result;
    for (auto _ : state) {
        auto status = buf.sample(render_time, result);
        benchmark::DoNotOptimize(result);
        benchmark::DoNotOptimize(status);
        render_time += 0.001;
        if (render_time > ts) {
            render_time = 0.05;
        }
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_InterpolationBuffer_Sample)
    ->Arg(64)
    ->Arg(128)
    ->Arg(256)
    ->Unit(benchmark::kMicrosecond);

// Benchmark: push + sample interleaved, simulating a real game-loop pattern
// where the buffer receives new snapshots and renders interpolated frames.
static void BM_InterpolationBuffer_PushAndSample(benchmark::State& state) {
    ae::InterpolationConfig cfg;
    cfg.capacity = 256;
    cfg.delay_seconds = 0.1;
    auto buf = ae::InterpolationBuffer<SampleVec3>(LerpVec3, cfg);

    double ts = 0.0;
    double render_time = 0.05;
    SampleVec3 result;

    for (auto _ : state) {
        // Simulate 60 game loop iterations.
        for (int frame = 0; frame < 60; ++frame) {
            ts += 0.016;
            buf.push(ts, SampleVec3{1.0, 2.0, 3.0});
            render_time += 0.016;

            auto status = buf.sample(render_time, result);
            benchmark::DoNotOptimize(result);
            benchmark::DoNotOptimize(status);
        }
        benchmark::ClobberMemory();
        buf.reset();
        ts = 0.0;
        render_time = 0.05;
    }
}
BENCHMARK(BM_InterpolationBuffer_PushAndSample)
    ->Unit(benchmark::kMicrosecond);

// Benchmark: interpolation buffer underrun path (sample before any data).
static void BM_InterpolationBuffer_Underrun(benchmark::State& state) {
    ae::InterpolationConfig cfg;
    cfg.capacity = 64;
    auto buf = ae::InterpolationBuffer<SampleVec3>(LerpVec3, cfg);

    buf.push(1.0, SampleVec3{1.0, 2.0, 3.0});

    SampleVec3 result;
    for (auto _ : state) {
        auto status = buf.sample(0.5, result);  // render_time before first sample
        benchmark::DoNotOptimize(result);
        benchmark::DoNotOptimize(status);
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_InterpolationBuffer_Underrun)
    ->Unit(benchmark::kMicrosecond);

// Benchmark: buffer empty path (fastest path — just return Empty status).
static void BM_InterpolationBuffer_Empty(benchmark::State& state) {
    ae::InterpolationConfig cfg;
    cfg.capacity = 64;
    auto buf = ae::InterpolationBuffer<SampleVec3>(LerpVec3, cfg);

    SampleVec3 result;
    for (auto _ : state) {
        auto status = buf.sample(1.0, result);
        benchmark::DoNotOptimize(result);
        benchmark::DoNotOptimize(status);
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_InterpolationBuffer_Empty)
    ->Unit(benchmark::kMicrosecond);

// ============================================================================
// PacketEnvelope Benchmarks
// ============================================================================

// Benchmark: PacketEnvelope construction and field assignment overhead.
static void BM_PacketEnvelope_Construct(benchmark::State& state) {
    for (auto _ : state) {
        ae::PacketEnvelope env;
        env.sequence = 12345;
        env.ack_sequence = 9999;
        env.ack_bitfield = 0xDEADBEEF;
        benchmark::DoNotOptimize(env);
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_PacketEnvelope_Construct)
    ->Unit(benchmark::kMicrosecond);

// ============================================================================
// Main
// ============================================================================

BENCHMARK_MAIN();
