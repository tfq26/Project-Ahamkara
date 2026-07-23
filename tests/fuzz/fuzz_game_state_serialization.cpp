// Fuzz target: game state serialization and deserialization round-trips.
//
// Constructs semi-valid game state objects from fuzzer-supplied bytes and
// exercises the write-serialize-read-deserialize round-trip to detect crashes,
// buffer overflows, or logic errors in the serialization layer.
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
//     fuzz_game_state_serialization.cpp \
//     -o fuzz_game_state_serialization

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

// Copy up to `max_bytes` from the fuzzer input into a fixed-size buffer.
// Returns the number of bytes actually copied.
template <std::size_t N>
std::size_t fill_from_fuzz(std::byte (&dest)[N], const std::uint8_t* data,
                           std::size_t size) {
    const std::size_t copy = (size < N) ? size : N;
    if (copy > 0 && data != nullptr) {
        std::memcpy(dest, data, copy);
    }
    // Zero-initialise any remaining bytes so the structure is well-defined.
    if (copy < N) {
        std::memset(dest + copy, 0, N - copy);
    }
    return copy;
}

// ---- Fuzz serialisation round-trips -----------------------------------------

void fuzz_player_input_roundtrip(const std::uint8_t* data, std::size_t size) {
    PlayerInputCommand cmd{};
    // Fill command fields from fuzz data.
    if (size >= sizeof(ae::u32) * 2 + sizeof(float)) {
        cmd.sequence = *reinterpret_cast<const ae::u32*>(data);
        cmd.client_tick = *reinterpret_cast<const ae::u32*>(data + 4);
        cmd.client_time = *reinterpret_cast<const float*>(data + 8);
    }
    if (size >= sizeof(ae::u32) * 2 + sizeof(float) + sizeof(float) * 4) {
        cmd.move_axis.x = *reinterpret_cast<const float*>(data + 12);
        cmd.move_axis.y = *reinterpret_cast<const float*>(data + 16);
        cmd.look_delta.x = *reinterpret_cast<const float*>(data + 20);
        cmd.look_delta.y = *reinterpret_cast<const float*>(data + 24);
    }
    // Bool flags: derive from remaining bytes.
    std::size_t offset = sizeof(ae::u32) * 2 + sizeof(float) * 5;
    if (offset < size) {
        cmd.jump_pressed = (data[offset] & 0x01) != 0;
        cmd.crouch_held = (data[offset] & 0x02) != 0;
        cmd.sprint_held = (data[offset] & 0x04) != 0;
        cmd.slide_pressed = (data[offset] & 0x08) != 0;
        cmd.fire_held = (data[offset] & 0x10) != 0;
        cmd.reload_pressed = (data[offset] & 0x20) != 0;
        cmd.ability_pressed = (data[offset] & 0x40) != 0;
        cmd.interact_pressed = (data[offset] & 0x80) != 0;
    }

    PacketEnvelope env{};
    PlayerInputPacketBuffer buf{};
    PlayerInputCommand decoded{};

    if (serialize_player_input_packet(env, cmd, buf)) {
        deserialize_player_input_packet(buf, env, decoded);
    }
}

void fuzz_server_snapshot_roundtrip(const std::uint8_t* data,
                                   std::size_t size) {
    ServerSnapshot snap{};
    std::size_t offset = 0;

    auto read_u32 = [&](ae::u32& val) -> bool {
        if (offset + sizeof(ae::u32) > size) return false;
        val = *reinterpret_cast<const ae::u32*>(data + offset);
        offset += sizeof(ae::u32);
        return true;
    };
    auto read_float = [&](float& val) -> bool {
        if (offset + sizeof(float) > size) return false;
        val = *reinterpret_cast<const float*>(data + offset);
        offset += sizeof(float);
        return true;
    };
    auto read_u8 = [&](ae::u8& val) -> bool {
        if (offset + sizeof(ae::u8) > size) return false;
        val = data[offset];
        offset += sizeof(ae::u8);
        return true;
    };

    read_u32(snap.server_tick);
    read_u32(snap.last_processed_input);
    read_u32(snap.local_player.network_object_id);
    read_u32(snap.local_player.player_id);
    read_float(snap.local_player.position.x);
    read_float(snap.local_player.position.y);
    read_float(snap.local_player.position.z);
    read_float(snap.local_player.velocity.x);
    read_float(snap.local_player.velocity.y);
    read_float(snap.local_player.velocity.z);
    read_float(snap.local_player.yaw);
    read_float(snap.local_player.health);
    read_float(snap.local_player.shield);
    read_u8(snap.projectile_count);
    read_u8(snap.dummy_count);
    read_u8(snap.match_phase);
    read_float(snap.match_time);
    read_u8(snap.remote_player_count);

    // Clamp array counts to valid ranges.
    if (snap.projectile_count > 8) snap.projectile_count = 8;
    if (snap.dummy_count > 4) snap.dummy_count = 4;
    if (snap.remote_player_count > 4) snap.remote_player_count = 4;

    PacketEnvelope env{};
    ServerSnapshotPacketBuffer buf{};
    ServerSnapshot decoded{};

    if (serialize_server_snapshot_packet(env, snap, buf)) {
        deserialize_server_snapshot_packet(buf, env, decoded);
    }
}

void fuzz_client_hello_roundtrip(const std::uint8_t* data, std::size_t size) {
    ClientHelloPacket pkt{};
    if (size >= sizeof(ae::u16) * 2) {
        pkt.protocol_version =
            *reinterpret_cast<const ae::u16*>(data);
        pkt.auth_token_length =
            *reinterpret_cast<const ae::u16*>(data + 2);
    }
    if (pkt.auth_token_length > kMaxAuthTokenLength) {
        pkt.auth_token_length = 0;
    }
    const std::size_t token_offset = sizeof(ae::u16) * 2;
    if (token_offset < size) {
        const std::size_t copy_size = std::min<std::size_t>(
            size - token_offset, sizeof(pkt.auth_token) - 1);
        std::memcpy(pkt.auth_token, data + token_offset, copy_size);
        pkt.auth_token[copy_size] = '\0';  // ensure null-termination
    }
    if (pkt.auth_token_length > size - token_offset) {
        pkt.auth_token_length =
            static_cast<ae::u16>(size - token_offset);
    }

    PacketEnvelope env{};
    ClientHelloPacketBuffer buf{};
    ClientHelloPacket decoded{};
    ae::usize written = 0;

    if (serialize_client_hello_packet(env, pkt, buf, &written)) {
        deserialize_client_hello_packet(buf, env, decoded);
    }
}

void fuzz_byte_writer_overflow(const std::uint8_t* data, std::size_t size) {
    // Construct a ByteWriter with a tiny buffer and attempt to write many
    // different types to ensure bounds checking is correct.
    std::byte small_buf[4]{};
    auto span = std::span<std::byte>(small_buf);
    detail::ByteWriter writer(span);

    // These writes should all gracefully fail (return false) without crashing.
    ae::u32 val32 = 0;
    if (size >= sizeof(ae::u32)) {
        val32 = *reinterpret_cast<const ae::u32*>(data);
    }
    writer.write(val32);

    ae::u16 val16 = 0;
    if (size >= sizeof(ae::u32) + sizeof(ae::u16)) {
        val16 = *reinterpret_cast<const ae::u16*>(data + 4);
    }
    writer.write(val16);

    writer.write_bool(true);
    writer.write_bool(false);

    float fval = 0.0F;
    if (size >= sizeof(ae::u32) + sizeof(ae::u16) + sizeof(float)) {
        fval = *reinterpret_cast<const float*>(data + 6);
    }
    writer.write(fval);
}

void fuzz_snapshot_delta_roundtrip(const std::uint8_t* data,
                                   std::size_t size) {
    SnapshotDelta delta{};
    std::size_t offset = 0;

    auto read_u32 = [&](ae::u32& val) -> bool {
        if (offset + sizeof(ae::u32) > size) return false;
        val = *reinterpret_cast<const ae::u32*>(data + offset);
        offset += sizeof(ae::u32);
        return true;
    };
    auto read_u16 = [&](ae::u16& val) -> bool {
        if (offset + sizeof(ae::u16) > size) return false;
        val = *reinterpret_cast<const ae::u16*>(data + offset);
        offset += sizeof(ae::u16);
        return true;
    };
    auto read_float = [&](float& val) -> bool {
        if (offset + sizeof(float) > size) return false;
        val = *reinterpret_cast<const float*>(data + offset);
        offset += sizeof(float);
        return true;
    };

    read_u32(delta.server_tick);
    read_u32(delta.last_processed_input);
    read_u16(delta.dirty_mask);

    if (delta.dirty_mask & (1 << 0)) read_float(delta.pos_x);
    if (delta.dirty_mask & (1 << 1)) read_float(delta.pos_y);
    if (delta.dirty_mask & (1 << 2)) read_float(delta.pos_z);
    if (delta.dirty_mask & (1 << 3)) read_float(delta.vel_x);
    if (delta.dirty_mask & (1 << 4)) read_float(delta.vel_y);
    if (delta.dirty_mask & (1 << 5)) read_float(delta.vel_z);
    if (delta.dirty_mask & (1 << 6)) read_float(delta.yaw);
    if (delta.dirty_mask & (1 << 7) && offset < size) delta.movement = data[offset++];
    if (delta.dirty_mask & (1 << 8)) read_float(delta.health);
    if (delta.dirty_mask & (1 << 9)) read_float(delta.shield);

    // Round-trip: write then read back.
    ServerSnapshotPacketBuffer buf{};
    auto span = std::span<std::byte>(buf);
    detail::ByteWriter writer(span);
    if (write_snapshot_delta(writer, delta)) {
        detail::ByteReader reader(span);
        SnapshotDelta decoded{};
        [[maybe_unused]] auto result = read_snapshot_delta(reader, decoded);
    }
}

void fuzz_player_state_roundtrip(const std::uint8_t* data, std::size_t size) {
    ReplicatedPlayerState state{};
    std::size_t offset = 0;

    auto read_u32 = [&](ae::u32& val) -> bool {
        if (offset + sizeof(ae::u32) > size) return false;
        val = *reinterpret_cast<const ae::u32*>(data + offset);
        offset += sizeof(ae::u32);
        return true;
    };
    auto read_float = [&](float& val) -> bool {
        if (offset + sizeof(float) > size) return false;
        val = *reinterpret_cast<const float*>(data + offset);
        offset += sizeof(float);
        return true;
    };

    read_u32(state.network_object_id);
    read_u32(state.player_id);
    read_float(state.position.x);
    read_float(state.position.y);
    read_float(state.position.z);
    read_float(state.velocity.x);
    read_float(state.velocity.y);
    read_float(state.velocity.z);
    read_float(state.yaw);
    if (offset < size) {
        state.movement_state =
            static_cast<MovementState>(data[offset] % 8);
        ++offset;
    }
    read_float(state.health);
    read_float(state.shield);

    // Round-trip through ByteWriter/ByteReader.
    ServerSnapshotPacketBuffer buf{};
    auto span = std::span<std::byte>(buf);
    detail::ByteWriter writer(span);
    if (write_player_state(writer, state)) {
        detail::ByteReader reader(span);
        ReplicatedPlayerState decoded{};
        [[maybe_unused]] auto result = read_player_state(reader, decoded);
    }
}

}  // anonymous namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data,
                                      std::size_t size) {
    if (size < 1) {
        return 0;
    }

    const std::uint8_t selector = data[0];
    const auto* payload = data + 1;
    const std::size_t payload_size = size - 1;

    switch (selector % 7) {
        case 0:
            fuzz_player_input_roundtrip(payload, payload_size);
            break;
        case 1:
            fuzz_server_snapshot_roundtrip(payload, payload_size);
            break;
        case 2:
            fuzz_client_hello_roundtrip(payload, payload_size);
            break;
        case 3:
            fuzz_byte_writer_overflow(payload, payload_size);
            break;
        case 4:
            fuzz_snapshot_delta_roundtrip(payload, payload_size);
            break;
        case 5:
            fuzz_player_state_roundtrip(payload, payload_size);
            break;
        case 6:
            // Combined: fuzz the ByteReader API directly with raw data.
            {
                auto buf = std::span<const std::byte>(
                    reinterpret_cast<const std::byte*>(payload),
                    payload_size);
                detail::ByteReader reader(buf);
                ae::u32 a{};
                ae::u16 b{};
                ae::u8 c{};
                float d{};
                reader.read(a);
                reader.read(b);
                reader.read(c);
                reader.read(d);
            }
            break;
    }

    return 0;
}
