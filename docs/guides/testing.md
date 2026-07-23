# Testing Guide

## Overview

The project uses a combination of:
- **Unit tests** — CTest-based smoke/integration tests under `tests/src/`
- **Fuzz tests** — libFuzzer-based harnesses under `tests/fuzz/` (Clang only)

---

## Running Unit Tests

```bash
# Configure and build (presets defined in CMakePresets.json)
cmake --preset debug
cmake --build --preset debug

# Run all tests
ctest --test-dir build/debug --output-on-failure

# Run a specific test
ctest --test-dir build/debug -R ahamkara_smoke_tests --output-on-failure
```

---

## Fuzz Testing

Fuzz tests use **libFuzzer** (part of Clang) and **AddressSanitizer** to
automatically discover crashes, buffer overflows, and other memory safety bugs
in network packet parsing and game state serialization routines.

### Prerequisites

- **Clang 14+** (Clang 21 is verified; any version with `-fsanitize=fuzzer`
  support works)
- The fuzz targets are **only built** when the compiler is Clang and the
  `AHAMKARA_BUILD_FUZZ_TESTS` option is enabled.

### Building Fuzz Targets

```bash
# Configure with Clang and enable fuzz targets
cmake --preset debug \
  -DAHAMKARA_BUILD_FUZZ_TESTS=ON \
  -DCMAKE_CXX_COMPILER=clang++-21

# Build individual fuzz target
cmake --build build/debug --target ahamkara_fuzz_udp_packet_parse
cmake --build build/debug --target ahamkara_fuzz_game_state_serialization
```

### Running Fuzz Targets

**Local development (interactive):**
```bash
# UDP packet parsing fuzzer (runs indefinitely until crash)
./build/debug/tests/fuzz/ahamkara_fuzz_udp_packet_parse

# Game state serialization fuzzer (timed run)
./build/debug/tests/fuzz/ahamkara_fuzz_game_state_serialization \
  -max_total_time=300 -runs=1000000
```

**CI sanity check** (brief, integrated via CTest):
```bash
ctest --test-dir build/debug -R ahamkara_fuzz --output-on-failure
```

### Fuzz Target Reference

| Target | File | Description |
|--------|------|-------------|
| `ahamkara_fuzz_udp_packet_parse` | `tests/fuzz/fuzz_udp_packet_parse.cpp` | Exercises all packet deserialization paths: `PlayerInput`, `ServerSnapshot`, `ClientHello`, `ServerWelcome`, `ServerReject`, `Heartbeat`, `ClientReconnect`. Also fuzzes `ByteReader` primitives and snapshot delta deserialization. |
| `ahamkara_fuzz_game_state_serialization` | `tests/fuzz/fuzz_game_state_serialization.cpp` | Constructs semi-valid game state objects from fuzzer data and exercises write-then-read round-trips. Covers `PlayerInputCommand`, `ServerSnapshot`, `ClientHelloPacket`, `SnapshotDelta`, `ReplicatedPlayerState`, and `ByteWriter` overflow bounds. |

### Fuzzer Corpus

For best results, supply a seed corpus:

```bash
mkdir -p fuzz_corpus/udp fuzz_corpus/serial
# Add example valid packets to seed corpus directories, then:
./build/debug/tests/fuzz/ahamkara_fuzz_udp_packet_parse \
  fuzz_corpus/udp/ -max_total_time=60
```

### Adding a New Fuzz Target

1. Create a new `.cpp` file in `tests/fuzz/` with an
   `extern "C" int LLVMFuzzerTestOneInput(...)` entry point.
2. Register the executable in `tests/fuzz/CMakeLists.txt` by following the
   existing pattern (add executable, set flags, link libraries).
3. Add a CTest sanity command at the bottom of the same CMake file.
4. Build and test:
   ```bash
   cmake --preset debug \
     -DAHAMKARA_BUILD_FUZZ_TESTS=ON \
     -DCMAKE_CXX_COMPILER=clang++-21
   cmake --build build/debug --target your_new_fuzz_target
   ctest --test-dir build/debug -R your_new_fuzz_target --output-on-failure
   ```

### Notes

- All fuzz targets use **AddressSanitizer** (`-fsanitize=address`) in addition
  to libFuzzer to catch memory errors immediately.
- The CI sanity step runs each fuzzer for **5 seconds with 10 000 runs**.
  This is not a coverage-oriented run; it only verifies the fuzz target
  compiles and does not instantly crash.
- To reproduce a crashing input found by a fuzzer, save the bytes to a file and
  pass it as a positional argument:
  ```bash
  ./build/debug/tests/fuzz/ahamkara_fuzz_udp_packet_parse crash-123
  ```
