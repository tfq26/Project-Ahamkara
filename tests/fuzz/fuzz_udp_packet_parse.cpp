// Fuzz target: UDP packet parsing/deserialization.
//
// Exercises all packet deserialization entry points with arbitrary byte
// sequences to detect crashes, memory errors, or logic bugs in the network
// packet parsing layer.
//
// Build with Clang + libFuzzer:
//   clang++-21 -std=c++20 -fsanitize=fuzzer,address \
//     -I../../engine/core/include \
//     -I../../engine/network/include \
//     -I../../game/include \
//     -I../../wish/include \
//     -I../../client/include \
//     -I../../build/debug/_deps/glm-src/glm \
//     -I../../build/debug/_deps/entt-src/src \
//     fuzz_udp_packet_parse.cpp \
//     -o fuzz_udp_packet_parse

#include "ae/core/types.h"
#include "ahamkara/game/net_packets.h"
#include "ahamkara/game/net_types.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

namespace {

using namespace ahamkara::game;

// ---- Fuzz helpers -----------------------------------------------------------

// Safely construct a span from fuzzer data.  Returns an empty span when
// `size` exceeds the available data to avoid UB in production paths.
[[nodiscard]] std::span<const std::byte> safe_span(const std::uint8_t* data,
                                                   std::size_t size) {
    if (data == nullptr || size == 0) {
        return {};
    }
    return {reinterpret_cast<const std::byte*>(data), size};
}

// ---- Fuzz one deserialization entry point -----------------------------------

void fuzz_deserialize_player_input(const std::uint8_t* data,
                                   std::size_t size) {
    auto buf = safe_span(data, size);
    PacketEnvelope env{};
    PlayerInputCommand cmd{};
    [[maybe_unused]] auto result =
        deserialize_player_input_packet(buf, env, cmd);
}

void fuzz_deserialize_server_snapshot(const std::uint8_t* data,
                                     std::size_t size) {
    auto buf = safe_span(data, size);
    PacketEnvelope env{};
    ServerSnapshot snap{};
    [[maybe_unused]] auto result =
        deserialize_server_snapshot_packet(buf, env, snap);
}

void fuzz_deserialize_client_hello(const std::uint8_t* data,
                                  std::size_t size) {
    auto buf = safe_span(data, size);
    PacketEnvelope env{};
    ClientHelloPacket pkt{};
    [[maybe_unused]] auto result =
        deserialize_client_hello_packet(buf, env, pkt);
}

void fuzz_deserialize_server_welcome(const std::uint8_t* data,
                                    std::size_t size) {
    auto buf = safe_span(data, size);
    PacketEnvelope env{};
    ServerWelcomePacket pkt{};
    [[maybe_unused]] auto result =
        deserialize_server_welcome_packet(buf, env, pkt);
}

void fuzz_deserialize_server_reject(const std::uint8_t* data,
                                   std::size_t size) {
    auto buf = safe_span(data, size);
    PacketEnvelope env{};
    ServerRejectPacket pkt{};
    [[maybe_unused]] auto result =
        deserialize_server_reject_packet(buf, env, pkt);
}

void fuzz_deserialize_heartbeat(const std::uint8_t* data, std::size_t size) {
    auto buf = safe_span(data, size);
    PacketEnvelope env{};
    HeartbeatPacket pkt{};
    [[maybe_unused]] auto result = deserialize_heartbeat_packet(buf, env, pkt);
}

void fuzz_deserialize_client_reconnect(const std::uint8_t* data,
                                      std::size_t size) {
    auto buf = safe_span(data, size);
    PacketEnvelope env{};
    ClientReconnectPacket pkt{};
    [[maybe_unused]] auto result =
        deserialize_client_reconnect_packet(buf, env, pkt);
}

void fuzz_byte_reader_trivially_copyable(const std::uint8_t* data,
                                        std::size_t size) {
    // Feed arbitrary bytes into the ByteReader and attempt to read various
    // trivially-copyable types.  This exercises bounds checking inside the
    // reader.
    auto buf = safe_span(data, size);
    detail::ByteReader reader(buf);

    ae::u32 a{}, b{}, c{};
    ae::u16 d{}, e{};
    ae::u8 f{};
    float x{}, y{}, z{};

    // Try reading in a mixed pattern; each read that fails simply returns
    // false and leaves the output parameter untouched.
    reader.read(a);
    reader.read(b);
    reader.read(d);
    reader.read(x);
    reader.read(c);
    reader.read(e);
    reader.read(y);
    reader.read(f);
    reader.read(z);
}

void fuzz_byte_reader_bools(const std::uint8_t* data, std::size_t size) {
    auto buf = safe_span(data, size);
    detail::ByteReader reader(buf);

    bool val{};
    for (std::size_t i = 0; i < 32; ++i) {
        if (!reader.read_bool(val)) {
            break;
        }
    }
}

void fuzz_read_snapshot_delta(const std::uint8_t* data, std::size_t size) {
    auto buf = safe_span(data, size);
    detail::ByteReader reader(buf);
    SnapshotDelta delta{};
    [[maybe_unused]] auto result = read_snapshot_delta(reader, delta);
}

// ---- Top-level fuzzer -------------------------------------------------------
// libFuzzer entry point.  Distributes fuzz data across all deserialization
// paths.  Each path is self-contained: it reads from the supplied span and
// returns false on malformed input without crashing.

}  // anonymous namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data,
                                      std::size_t size) {
    // Ensure we have enough data to exercise at least one path meaningfully.
    if (size < 1) {
        return 0;
    }

    // Use the first byte to select a fuzz target.
    const std::uint8_t selector = data[0];
    const auto* payload = data + 1;
    const std::size_t payload_size = size - 1;

    switch (selector % 10) {
        case 0:
            fuzz_deserialize_player_input(payload, payload_size);
            break;
        case 1:
            fuzz_deserialize_server_snapshot(payload, payload_size);
            break;
        case 2:
            fuzz_deserialize_client_hello(payload, payload_size);
            break;
        case 3:
            fuzz_deserialize_server_welcome(payload, payload_size);
            break;
        case 4:
            fuzz_deserialize_server_reject(payload, payload_size);
            break;
        case 5:
            fuzz_deserialize_heartbeat(payload, payload_size);
            break;
        case 6:
            fuzz_deserialize_client_reconnect(payload, payload_size);
            break;
        case 7:
            fuzz_byte_reader_trivially_copyable(payload, payload_size);
            break;
        case 8:
            fuzz_byte_reader_bools(payload, payload_size);
            break;
        case 9:
            fuzz_read_snapshot_delta(payload, payload_size);
            break;
    }

    return 0;  // Non-zero tells libFuzzer the input is undesirable.
}
