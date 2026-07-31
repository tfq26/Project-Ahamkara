{
  runs: [
    {
      name: "build and test",
      tasks: [
        {
          name: "lint",
          runtime: {
            arch: "amd64",
            containers: [{ image: "debian:bookworm" }],
          },
          environment: { DEBIAN_FRONTEND: "noninteractive" },
          steps: [
            { type: "run", command: "apt-get update -qq && apt-get install -y -qq git" },
            { type: "run", command: "git clone --branch \"$AGOLA_REPOSITORY_REF\" \"$AGOLA_REPOSITORY_URL\" . && git -c advice.detachedHead=false checkout \"$AGOLA_COMMIT_SHA\"" },
            { type: "run", command: "apt-get install -y -qq cmake ninja-build clang clang-tidy clang-format" },
            { type: "run", command: "cmake --preset debug" },
            { type: "run", command: "cmake --build --preset debug" },
            { type: "run", command: "bash ./scripts/lint.sh --base-ref origin/main --compile-db build/debug || echo Lint issues found" },
          ],
        },
        {
          name: "build-debug",
          runtime: {
            arch: "amd64",
            containers: [{ image: "debian:bookworm" }],
          },
          environment: { DEBIAN_FRONTEND: "noninteractive" },
          steps: [
            { type: "run", command: "apt-get update -qq && apt-get install -y -qq git" },
            { type: "run", command: "git clone --branch \"$AGOLA_REPOSITORY_REF\" \"$AGOLA_REPOSITORY_URL\" . && git -c advice.detachedHead=false checkout \"$AGOLA_COMMIT_SHA\"" },
            { type: "run", command: "apt-get update -qq && apt-get install -y -qq cmake ninja-build clang build-essential" },
            { type: "run", command: "cmake --preset debug && cmake --build --preset debug" },
          ],
        },
        {
          name: "test-debug",
          depends: ["build-debug"],
          runtime: {
            arch: "amd64",
            containers: [{ image: "debian:bookworm" }],
          },
          environment: { DEBIAN_FRONTEND: "noninteractive" },
          steps: [
            { type: "run", command: "apt-get update -qq && apt-get install -y -qq git" },
            { type: "run", command: "git clone --branch \"$AGOLA_REPOSITORY_REF\" \"$AGOLA_REPOSITORY_URL\" . && git -c advice.detachedHead=false checkout \"$AGOLA_COMMIT_SHA\"" },
            { type: "run", command: "apt-get update -qq && apt-get install -y -qq cmake ninja-build clang build-essential" },
            { type: "run", command: "cmake --preset debug && cmake --build --preset debug && ctest --test-dir build/debug --output-on-failure" },
          ],
        },
        {
          name: "build-debug-headless",
          runtime: {
            arch: "amd64",
            containers: [{ image: "debian:bookworm" }],
          },
          environment: { DEBIAN_FRONTEND: "noninteractive" },
          steps: [
            { type: "run", command: "apt-get update -qq && apt-get install -y -qq git" },
            { type: "run", command: "git clone --branch \"$AGOLA_REPOSITORY_REF\" \"$AGOLA_REPOSITORY_URL\" . && git -c advice.detachedHead=false checkout \"$AGOLA_COMMIT_SHA\"" },
            { type: "run", command: "apt-get update -qq && apt-get install -y -qq cmake ninja-build clang build-essential" },
            { type: "run", command: "cmake --preset debug-headless && cmake --build --preset debug-headless" },
          ],
        },
        {
          name: "test-debug-headless",
          depends: ["build-debug-headless"],
          runtime: {
            arch: "amd64",
            containers: [{ image: "debian:bookworm" }],
          },
          environment: { DEBIAN_FRONTEND: "noninteractive" },
          steps: [
            { type: "run", command: "apt-get update -qq && apt-get install -y -qq git" },
            { type: "run", command: "git clone --branch \"$AGOLA_REPOSITORY_REF\" \"$AGOLA_REPOSITORY_URL\" . && git -c advice.detachedHead=false checkout \"$AGOLA_COMMIT_SHA\"" },
            { type: "run", command: "apt-get update -qq && apt-get install -y -qq cmake ninja-build clang build-essential" },
            { type: "run", command: "cmake --preset debug-headless && cmake --build --preset debug-headless && ctest --test-dir build/debug-headless --output-on-failure" },
          ],
        },
        {
          name: "build-frontend",
          runtime: {
            arch: "amd64",
            containers: [{ image: "node:22-bookworm" }],
          },
          steps: [
            { type: "run", command: "apt-get update -qq && apt-get install -y -qq git" },
            { type: "run", command: "git clone --branch \"$AGOLA_REPOSITORY_REF\" \"$AGOLA_REPOSITORY_URL\" . && git -c advice.detachedHead=false checkout \"$AGOLA_COMMIT_SHA\"" },
            { type: "run", command: "npm ci --prefix frontend" },
            { type: "run", command: "npm run build --prefix frontend" },
          ],
        },
        {
          name: "deploy-frontend",
          depends: ["build-frontend"],
          when: { branch: "main" },
          runtime: {
            arch: "amd64",
            containers: [{ image: "node:22-bookworm" }],
          },
          environment: {
            CLOUDFLARE_API_TOKEN: "",
            CLOUDFLARE_ACCOUNT_ID: "",
          },
          steps: [
            { type: "run", command: "apt-get update -qq && apt-get install -y -qq git" },
            { type: "run", command: "git clone --branch \"$AGOLA_REPOSITORY_REF\" \"$AGOLA_REPOSITORY_URL\" . && git -c advice.detachedHead=false checkout \"$AGOLA_COMMIT_SHA\"" },
            { type: "run", command: "bash deploy.sh" },
          ],
        },
      ],
    },
  ],
}
