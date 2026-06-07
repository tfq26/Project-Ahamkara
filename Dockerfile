FROM ubuntu:24.04 AS build

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        cmake \
        g++ \
        git \
        ninja-build \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN cmake -S . -B build -G Ninja \
        -DAHAMKARA_BUILD_CLIENT=OFF \
        -DAHAMKARA_BUILD_TESTS=OFF \
        -DAHAMKARA_BUILD_SAMPLES=OFF \
    && cmake --build build --target ahamkara_server -j"$(nproc)"

FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=build /src/build/server/ahamkara_server /app/ahamkara_server

EXPOSE 7777/udp
EXPOSE 7778/tcp

ENV WISH_SERVER_PORT=7777
ENV WISH_SERVER_ADMIN_PORT=7778
ENV WISH_SERVER_TICK_RATE=60
ENV WISH_SERVER_MAX_PLAYERS=8
ENV WISH_SERVER_DISCONNECT_TIMEOUT_SEC=10
ENV WISH_SERVER_MATCH_DURATION_SEC=600

CMD ["/app/ahamkara_server"]
