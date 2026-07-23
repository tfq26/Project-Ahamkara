# Sanitizer Build Presets

Ahamkara provides CMake presets for [AddressSanitizer (ASan)] and
[UndefinedBehaviorSanitizer (UBSan)] to help detect memory errors and undefined
behaviour during development and CI.

[AddressSanitizer (ASan)]: https://clang.llvm.org/docs/AddressSanitizer.html
[UndefinedBehaviorSanitizer (UBSan)]: https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html

## Prerequisites

- **Compiler**: GCC or Clang (Apple Clang requires the `-fsanitize` flag to be
  supported; Xcode 12+ recommended).
- **Linux**: `libasan` and `libubsan` runtime libraries are typically included
  with the compiler. On Ubuntu you may need to install them separately:

  ```sh
  # GCC
  sudo apt install libasan8 libubsan1

  # Clang
  sudo apt install libclang-rt-dev
  ```

- **macOS**: Xcode ships the sanitizer runtimes with the toolchain. No
  additional packages are required.

> **Windows**: ASan and UBSan are not currently supported via these presets.
> Only GCC and Clang-based toolchains are supported.

## Presets

| Preset         | Inherits | Sanitizers        | Binary Directory            |
|----------------|----------|-------------------|-----------------------------|
| `debug-asan`   | `debug`  | AddressSanitizer  | `build/debug-asan`          |
| `debug-ubsan`  | `debug`  | UBSan             | `build/debug-ubsan`         |

Both presets inherit from the `debug` preset, so they include debug symbols,
no optimisation, and `compile_commands.json` for LSP/clangd.

## Usage

### Configure

```sh
# AddressSanitizer
cmake --preset debug-asan

# UndefinedBehaviorSanitizer
cmake --preset debug-ubsan
```

### Build

```sh
cmake --build --preset debug-asan
cmake --build --preset debug-ubsan
```

### Test

```sh
ctest --test-dir build/debug-asan --output-on-failure
ctest --test-dir build/debug-ubsan --output-on-failure
```

## What Each Sanitizer Detects

### AddressSanitizer (ASan)

- Buffer overflows (heap, stack, globals)
- Use-after-free
- Use-after-return
- Double-free / invalid free
- Memory leaks (enabled via `ASAN_OPTIONS=detect_leaks=1` on Linux)

### UndefinedBehaviorSanitizer (UBSan)

- Integer overflow
- Invalid shifts
- Null pointer dereference (with `-fsanitize=null`)
- Misaligned pointers
- Signed integer overflow
- Division by zero
- And many more undefined behaviour categories

## Platform-Specific Caveats

### Linux (GCC / Clang)

Fully supported. No known issues.

### macOS (Apple Clang)

- ASan and UBSan are supported but may impose a significant performance
  overhead.
- macOS System Integrity Protection (SIP) may interfere with ASan's ability to
  detect certain memory issues. This is generally not a problem for debug
  builds.

### Windows (MSVC)

Not supported via these presets. MSVC has its own
[/fsanitize] flags that are not compatible with the GCC/Clang `-fsanitize`
flag format.

[/fsanitize]: https://learn.microsoft.com/en-us/cpp/sanitizers/asan

## CI Integration

To run sanitizer builds in CI, add steps like:

```yaml
- name: Configure (ASan)
  run: cmake --preset debug-asan

- name: Build (ASan)
  run: cmake --build --preset debug-asan

- name: Test (ASan)
  run: ctest --test-dir build/debug-asan --output-on-failure
```

## Troubleshooting

**`undefined reference to __asan_report_error`**
The ASan runtime library is not being linked. Ensure you are using a compiler
that supports `-fsanitize=address` and that the sanitizer runtime is installed.

**`AddressSanitizer:DEADLYSIGNAL`**
A memory error has been detected. Inspect the stack trace to locate the
offending code. Common causes:
- Buffer overflow
- Use-after-free
- Stack buffer overflow

**`SUMMARY: UndefinedBehaviorSanitizer: undefined-behavior`**
The program triggered undefined behaviour. The diagnostic includes the source
file, line number, and a description of the issue.

**Build takes much longer with ASan**
ASan adds instrumentation that increases compilation time and runtime overhead.
This is expected. Use the standard `debug` preset for fast iteration and only
use `debug-asan` when specifically hunting memory errors.
