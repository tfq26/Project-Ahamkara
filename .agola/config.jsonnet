{
  runs: [
    {
      name: "build and test",
      tasks: [
        {
          name: "lint",
          runtime: {
            arch: "amd64",
            containers: [{ image: "debian:bookworm-slim" }],
          },
          environment: { DEBIAN_FRONTEND: "noninteractive" },
          steps: [
            { type: "clone" },
            { type: "run", command: "apt-get update -qq && apt-get install -y -qq git cmake ninja-build clang clang-tidy clang-format" },
            { type: "run", command: "cmake --preset debug" },
            { type: "run", command: "cmake --build --preset debug" },
            { type: "run", command: "bash ./scripts/lint.sh --base-ref origin/main --compile-db build/debug || echo Lint issues found" },
          ],
        },
        {
          name: "build-debug",
          runtime: {
            arch: "amd64",
            containers: [{ image: "debian:bookworm-slim" }],
          },
          environment: { DEBIAN_FRONTEND: "noninteractive" },
          steps: [
            { type: "clone" },
            { type: "run", command: "apt-get update -qq && apt-get install -y -qq git cmake ninja-build clang build-essential" },
            { type: "run", command: "cmake --preset debug && cmake --build --preset debug" },
          ],
        },
        {
          name: "test-debug",
          depends: ["build-debug"],
          runtime: {
            arch: "amd64",
            containers: [{ image: "debian:bookworm-slim" }],
          },
          environment: { DEBIAN_FRONTEND: "noninteractive" },
          steps: [
            { type: "clone" },
            { type: "run", command: "apt-get update -qq && apt-get install -y -qq git cmake ninja-build clang build-essential" },
            { type: "run", command: "cmake --preset debug && cmake --build --preset debug && ctest --test-dir build/debug --output-on-failure" },
          ],
        },
        {
          name: "build-debug-headless",
          runtime: {
            arch: "amd64",
            containers: [{ image: "debian:bookworm-slim" }],
          },
          environment: { DEBIAN_FRONTEND: "noninteractive" },
          steps: [
            { type: "clone" },
            { type: "run", command: "apt-get update -qq && apt-get install -y -qq git cmake ninja-build clang build-essential" },
            { type: "run", command: "cmake --preset debug-headless && cmake --build --preset debug-headless" },
          ],
        },
        {
          name: "test-debug-headless",
          depends: ["build-debug-headless"],
          runtime: {
            arch: "amd64",
            containers: [{ image: "debian:bookworm-slim" }],
          },
          environment: { DEBIAN_FRONTEND: "noninteractive" },
          steps: [
            { type: "clone" },
            { type: "run", command: "apt-get update -qq && apt-get install -y -qq git cmake ninja-build clang build-essential" },
            { type: "run", command: "cmake --preset debug-headless && cmake --build --preset debug-headless && ctest --test-dir build/debug-headless --output-on-failure" },
          ],
        },
      ],
    },
  ],
}
