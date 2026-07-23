// Standalone fuzz driver — used when libFuzzer is not available.
//
// This provides the main() entry point that reads fuzz input from a file
// (or generates random data) and forwards it to LLVMFuzzerTestOneInput.
// When libFuzzer is available (Clang + -fsanitize=fuzzer), this file is
// excluded and libFuzzer provides its own driver.
//
// Usage:
//   ./fuzz_target                    # 10k random iterations
//   ./fuzz_target /path/to/crash     # replay a specific input
//   ./fuzz_target -runs=50000        # custom iteration count

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <random>
#include <vector>

// The fuzz entry point — defined in the target's .cpp file.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data,
                                      std::size_t size);

namespace {

void print_usage(const char* prog) {
    std::fprintf(stderr,
                 "Usage: %s [OPTIONS] [FILE]\n"
                 "Options:\n"
                 "  -runs=N    Run N iterations with random data (default: "
                 "10000)\n"
                 "  FILE       Replay a single input from FILE\n",
                 prog);
}

int run_random(std::size_t num_runs) {
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> byte_dist(0, 255);
    std::uniform_int_distribution<int> size_dist(0, 4096);

    for (std::size_t i = 0; i < num_runs; ++i) {
        const std::size_t buf_size = static_cast<std::size_t>(size_dist(rng));
        std::vector<std::uint8_t> buf(buf_size);
        for (auto& b : buf) {
            b = static_cast<std::uint8_t>(byte_dist(rng));
        }

        const int result =
            LLVMFuzzerTestOneInput(buf.data(), buf.size());
        if (result != 0) {
            std::fprintf(
                stderr,
                "Fuzz target returned non-zero (%d) at iteration %zu\n",
                result, i);
            return 1;
        }
    }

    std::fprintf(stdout, "Completed %zu random iterations (no crashes)\n",
                 num_runs);
    return 0;
}

int run_file(const char* path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        std::fprintf(stderr, "Error: cannot open '%s'\n", path);
        return 1;
    }

    const auto size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<std::uint8_t> buf(static_cast<std::size_t>(size));
    if (!file.read(reinterpret_cast<char*>(buf.data()), size)) {
        std::fprintf(stderr, "Error: failed to read '%s'\n", path);
        return 1;
    }

    const int result = LLVMFuzzerTestOneInput(buf.data(), buf.size());
    if (result == 0) {
        std::fprintf(stdout, "Processed '%s' (%zu bytes) — no crash\n", path,
                     static_cast<std::size_t>(size));
    } else {
        std::fprintf(stderr, "Fuzz target returned non-zero (%d)\n", result);
    }
    return result;
}

}  // anonymous namespace

int main(int argc, char* argv[]) {
    if (argc > 1) {
        if (std::strncmp(argv[1], "-runs=", 6) == 0) {
            const std::size_t num_runs =
                static_cast<std::size_t>(std::atoll(argv[1] + 6));
            if (num_runs == 0) {
                std::fprintf(stderr, "Invalid -runs value\n");
                return 1;
            }
            return run_random(num_runs);
        }
        if (std::strcmp(argv[1], "-h") == 0 ||
            std::strcmp(argv[1], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
        // Treat as file path
        return run_file(argv[1]);
    }

    // Default: random iterations.
    return run_random(10000);
}
