#pragma once

#include "ae/core/types.h"
#include "ahamkara/game/net_types.h"

#include <array>
#include <cstddef>
#include <cstring>
#include <span>
#include <type_traits>

namespace ahamkara::game {

constexpr ae::u32 kPacketMagic = 0x41484D4BU;
constexpr ae::u16 kProtocolVersion = 1;

enum class PacketType : ae::u16 {
    PlayerInput = 1,
    ServerSnapshot = 2,
    ClientHello = 3,
    ServerWelcome = 4,
    ServerReject = 5,
    Heartbeat = 6,
    ClientReconnect = 7
};

enum class HandshakeRejectReason : ae::u8 {
    VersionMismatch = 1,
    ServerBusy = 2
};

struct PacketHeader {
    ae::u32 magic {kPacketMagic};
    ae::u16 version {kProtocolVersion};
    PacketType type {PacketType::PlayerInput};
};

static constexpr ae::u16 kMaxAuthTokenLength = 512;
static constexpr ae::u16 kMaxPlayerIdLength = 64;

struct ClientHelloPacket {
    ae::u16 protocol_version {kProtocolVersion};
    ae::u16 auth_token_length {0};
    char auth_token[kMaxAuthTokenLength] {};
};

struct ServerWelcomePacket {
    ae::u16 protocol_version {kProtocolVersion};
    char player_id[kMaxPlayerIdLength] {};
};

struct ServerRejectPacket {
    ae::u16 protocol_version {kProtocolVersion};
    HandshakeRejectReason reason {HandshakeRejectReason::VersionMismatch};
};

struct HeartbeatPacket {
    ae::u32 server_tick {0};
    ae::u32 connected_players {0};
};

struct ClientReconnectPacket {
    ae::u64 session_id {0};
    ae::u16 protocol_version {kProtocolVersion};
};

[[nodiscard]] constexpr bool is_supported_protocol_version(const ae::u16 version) {
    return version == kProtocolVersion;
}

static_assert(std::is_trivially_copyable_v<ClientHelloPacket>);

namespace detail {

class ByteWriter {
public:
    explicit ByteWriter(std::span<std::byte> buffer)
        : buffer_(buffer) {
    }

    template <typename T>
    bool write(const T& value) {
        static_assert(std::is_trivially_copyable_v<T>);

        const auto bytes = sizeof(T);
        if (position_ + bytes > buffer_.size()) {
            return false;
        }

        std::memcpy(buffer_.data() + position_, &value, bytes);
        position_ += bytes;
        return true;
    }

    bool write_bool(bool value) {
        const ae::u8 encoded = value ? 1U : 0U;
        return write(encoded);
    }

    bool write_bytes(const char* data, ae::u16 length) {
        if (position_ + length > buffer_.size()) {
            return false;
        }
        std::memcpy(buffer_.data() + position_, data, length);
        position_ += length;
        return true;
    }

    [[nodiscard]] ae::usize bytes_written() const {
        return position_;
    }

private:
    std::span<std::byte> buffer_;
    ae::usize position_ {0};
};

class ByteReader {
public:
    explicit ByteReader(std::span<const std::byte> buffer)
        : buffer_(buffer) {
    }

    template <typename T>
    bool read(T& value) {
        static_assert(std::is_trivially_copyable_v<T>);

        const auto bytes = sizeof(T);
        if (position_ + bytes > buffer_.size()) {
            return false;
        }

        std::memcpy(&value, buffer_.data() + position_, bytes);
        position_ += bytes;
        return true;
    }

    bool read_bool(bool& value) {
        ae::u8 encoded = 0;
        if (!read(encoded) || encoded > 1U) {
            return false;
        }

        value = encoded != 0U;
        return true;
    }

    bool read_bytes(char* dest, ae::u16 length) {
        if (position_ + length > buffer_.size()) {
            return false;
        }
        std::memcpy(dest, buffer_.data() + position_, length);
        position_ += length;
        return true;
    }

    [[nodiscard]] bool is_complete() const {
        return position_ == buffer_.size();
    }

private:
    std::span<const std::byte> buffer_;
    ae::usize position_ {0};
};

inline bool write_header(ByteWriter& writer, PacketType type) {
    PacketHeader header {};
    header.type = type;
    return writer.write(header.magic)
        && writer.write(header.version)
        && writer.write(static_cast<ae::u16>(header.type));
}

inline bool read_header(ByteReader& reader, PacketType expected_type) {
    ae::u32 magic = 0;
    ae::u16 version = 0;
    ae::u16 type = 0;

    if (!reader.read(magic) || !reader.read(version) || !reader.read(type)) {
        return false;
    }

    return magic == kPacketMagic
        && version == kProtocolVersion
        && type == static_cast<ae::u16>(expected_type);
}

inline bool write_vec2(ByteWriter& writer, const Vec2& value) {
    return writer.write(value.x) && writer.write(value.y);
}

inline bool read_vec2(ByteReader& reader, Vec2& value) {
    return reader.read(value.x) && reader.read(value.y);
}

inline bool write_vec3(ByteWriter& writer, const Vec3& value) {
    return writer.write(value.x) && writer.write(value.y) && writer.write(value.z);
}

inline bool read_vec3(ByteReader& reader, Vec3& value) {
    return reader.read(value.x) && reader.read(value.y) && reader.read(value.z);
}

inline bool write_player_input(ByteWriter& writer, const PlayerInputCommand& command) {
    return writer.write(command.sequence)
        && writer.write(command.client_tick)
        && writer.write(command.client_time)
        && write_vec2(writer, command.move_axis)
        && write_vec2(writer, command.look_delta)
        && writer.write_bool(command.jump_pressed)
        && writer.write_bool(command.crouch_held)
        && writer.write_bool(command.sprint_held)
        && writer.write_bool(command.slide_pressed)
        && writer.write_bool(command.fire_held)
        && writer.write_bool(command.reload_pressed)
        && writer.write_bool(command.ability_pressed)
        && writer.write_bool(command.interact_pressed);
}

inline bool read_player_input(ByteReader& reader, PlayerInputCommand& command) {
    return reader.read(command.sequence)
        && reader.read(command.client_tick)
        && reader.read(command.client_time)
        && read_vec2(reader, command.move_axis)
        && read_vec2(reader, command.look_delta)
        && reader.read_bool(command.jump_pressed)
        && reader.read_bool(command.crouch_held)
        && reader.read_bool(command.sprint_held)
        && reader.read_bool(command.slide_pressed)
        && reader.read_bool(command.fire_held)
        && reader.read_bool(command.reload_pressed)
        && reader.read_bool(command.ability_pressed)
        && reader.read_bool(command.interact_pressed);
}

inline bool write_player_state(ByteWriter& writer, const ReplicatedPlayerState& state) {
    return writer.write(state.network_object_id)
        && writer.write(state.player_id)
        && write_vec3(writer, state.position)
        && write_vec3(writer, state.velocity)
        && writer.write(state.yaw)
        && writer.write(static_cast<ae::u8>(state.movement_state))
        && writer.write(state.health)
        && writer.write(state.shield);
}

inline bool read_player_state(ByteReader& reader, ReplicatedPlayerState& state) {
    ae::u8 movement_state_value = 0;

    if (!reader.read(state.network_object_id)
        || !reader.read(state.player_id)
        || !read_vec3(reader, state.position)
        || !read_vec3(reader, state.velocity)
        || !reader.read(state.yaw)
        || !reader.read(movement_state_value)
        || !reader.read(state.health)
        || !reader.read(state.shield)) {
        return false;
    }

    state.movement_state = static_cast<MovementState>(movement_state_value);
    return true;
}

inline bool write_snapshot(ByteWriter& writer, const ServerSnapshot& snapshot) {
    if (!writer.write(snapshot.server_tick)
        || !writer.write(snapshot.last_processed_input)
        || !write_player_state(writer, snapshot.local_player)
        || !writer.write(snapshot.projectile_count))
        return false;
    for (ae::u8 i = 0; i < snapshot.projectile_count; ++i) {
        const auto& p = snapshot.projectiles[i];
        if (!write_vec3(writer, p.position) || !write_vec3(writer, p.velocity)
            || !writer.write(p.lifetime_seconds) || !writer.write_bool(p.alive)
            || !writer.write(p.client_tick))
            return false;
    }
    if (!writer.write(snapshot.dummy_count)) return false;
    for (ae::u8 i = 0; i < snapshot.dummy_count; ++i) {
        const auto& d = snapshot.dummies[i];
        if (!writer.write(d.dummy_id) || !write_vec3(writer, d.position)
            || !writer.write(d.yaw) || !writer.write(d.health)
            || !writer.write_bool(d.alive))
            return false;
    }
    if (!writer.write(snapshot.match_phase) || !writer.write(snapshot.match_time)
        || !writer.write(snapshot.team_score_red) || !writer.write(snapshot.team_score_blue)
        || !writer.write(snapshot.individual_score) || !writer.write(snapshot.remote_player_count))
        return false;
    for (ae::u8 i = 0; i < snapshot.remote_player_count; ++i) {
        const auto& rp = snapshot.remote_players[i];
        if (!writer.write(rp.player_id) || !write_vec3(writer, rp.position)
            || !writer.write(rp.yaw) || !writer.write(rp.health))
            return false;
    }
    return true;
}

inline bool read_snapshot(ByteReader& reader, ServerSnapshot& snapshot) {
    if (!reader.read(snapshot.server_tick)
        || !reader.read(snapshot.last_processed_input)
        || !read_player_state(reader, snapshot.local_player)
        || !reader.read(snapshot.projectile_count))
        return false;
    if (snapshot.projectile_count > 8) return false;
    for (ae::u8 i = 0; i < snapshot.projectile_count; ++i) {
        auto& p = snapshot.projectiles[i];
        if (!read_vec3(reader, p.position) || !read_vec3(reader, p.velocity)
            || !reader.read(p.lifetime_seconds) || !reader.read_bool(p.alive)
            || !reader.read(p.client_tick))
            return false;
    }
    if (!reader.read(snapshot.dummy_count) || snapshot.dummy_count > 4) return false;
    for (ae::u8 i = 0; i < snapshot.dummy_count; ++i) {
        auto& d = snapshot.dummies[i];
        if (!reader.read(d.dummy_id) || !read_vec3(reader, d.position)
            || !reader.read(d.yaw) || !reader.read(d.health)
            || !reader.read_bool(d.alive))
            return false;
    }
    if (!reader.read(snapshot.match_phase) || !reader.read(snapshot.match_time)
        || !reader.read(snapshot.team_score_red) || !reader.read(snapshot.team_score_blue)
        || !reader.read(snapshot.individual_score) || !reader.read(snapshot.remote_player_count))
        return false;
    if (snapshot.remote_player_count > 4) return false;
    for (ae::u8 i = 0; i < snapshot.remote_player_count; ++i) {
        auto& rp = snapshot.remote_players[i];
        if (!reader.read(rp.player_id) || !read_vec3(reader, rp.position)
            || !reader.read(rp.yaw) || !reader.read(rp.health))
            return false;
    }
    return true;
}

inline bool write_client_hello(ByteWriter& writer, const ClientHelloPacket& packet) {
    return writer.write(packet.protocol_version)
        && writer.write(packet.auth_token_length)
        && writer.write_bytes(packet.auth_token, packet.auth_token_length);
}

inline bool read_client_hello(ByteReader& reader, ClientHelloPacket& packet) {
    if (!reader.read(packet.protocol_version)
        || !reader.read(packet.auth_token_length))
        return false;
    if (packet.auth_token_length > kMaxAuthTokenLength) return false;
    return reader.read_bytes(packet.auth_token, packet.auth_token_length);
}

inline bool write_server_welcome(ByteWriter& writer, const ServerWelcomePacket& packet) {
    return writer.write(packet.protocol_version)
        && writer.write_bytes(packet.player_id, kMaxPlayerIdLength);
}

inline bool read_server_welcome(ByteReader& reader, ServerWelcomePacket& packet) {
    return reader.read(packet.protocol_version)
        && reader.read_bytes(packet.player_id, kMaxPlayerIdLength);
}

inline bool write_server_reject(ByteWriter& writer, const ServerRejectPacket& packet) {
    return writer.write(packet.protocol_version)
        && writer.write(static_cast<ae::u8>(packet.reason));
}

inline bool read_server_reject(ByteReader& reader, ServerRejectPacket& packet) {
    ae::u8 reason_value = 0;
    if (!reader.read(packet.protocol_version)
        || !reader.read(reason_value)
        || reason_value == 0U
        || reason_value > static_cast<ae::u8>(HandshakeRejectReason::ServerBusy)) {
        return false;
    }

    packet.reason = static_cast<HandshakeRejectReason>(reason_value);
    return true;
}

inline bool write_envelope(ByteWriter& writer, const PacketEnvelope& envelope) {
    return writer.write(envelope.sequence)
        && writer.write(envelope.ack_sequence)
        && writer.write(envelope.ack_bitfield);
}

inline bool read_envelope(ByteReader& reader, PacketEnvelope& envelope) {
    return reader.read(envelope.sequence)
        && reader.read(envelope.ack_sequence)
        && reader.read(envelope.ack_bitfield);
}

inline bool write_heartbeat(ByteWriter& writer, const HeartbeatPacket& packet) {
    return writer.write(packet.server_tick)
        && writer.write(packet.connected_players);
}

inline bool read_heartbeat(ByteReader& reader, HeartbeatPacket& packet) {
    return reader.read(packet.server_tick)
        && reader.read(packet.connected_players);
}

inline bool write_client_reconnect(ByteWriter& writer, const ClientReconnectPacket& packet) {
    return writer.write(packet.session_id)
        && writer.write(packet.protocol_version);
}

inline bool read_client_reconnect(ByteReader& reader, ClientReconnectPacket& packet) {
    return reader.read(packet.session_id)
        && reader.read(packet.protocol_version);
}

}  // namespace detail

// Wire size constants (at ahamkara::game scope, not detail)
constexpr ae::usize kProjectileStateWireSize = sizeof(float)*3 + sizeof(float)*3 + sizeof(float) + sizeof(ae::u8) + sizeof(ae::u32);
constexpr ae::usize kDummyStateWireSize = sizeof(ae::u32) + sizeof(float)*3 + sizeof(float) + sizeof(float) + sizeof(ae::u8);
constexpr ae::usize kMaxSnapshotWireSize = sizeof(ae::u32)*2 + sizeof(ReplicatedPlayerState)
    + sizeof(ae::u8) + kProjectileStateWireSize * 8
    + sizeof(ae::u8) + kDummyStateWireSize * 4
    + sizeof(ae::u8) + sizeof(float) + sizeof(ae::u32)*2 + sizeof(ae::u16)
    + sizeof(ae::u8) + (sizeof(ae::u32) + sizeof(float)*3 + sizeof(float) + sizeof(float)) * 4;

// Delta compression functions
inline bool write_snapshot_delta(detail::ByteWriter& writer, const SnapshotDelta& d) {
    if (!writer.write(d.server_tick) || !writer.write(d.last_processed_input) || !writer.write(d.dirty_mask))
        return false;
    if (d.dirty_mask & (1 << static_cast<ae::u16>(PlayerDeltaBit::PosX))) writer.write(d.pos_x);
    if (d.dirty_mask & (1 << static_cast<ae::u16>(PlayerDeltaBit::PosY))) writer.write(d.pos_y);
    if (d.dirty_mask & (1 << static_cast<ae::u16>(PlayerDeltaBit::PosZ))) writer.write(d.pos_z);
    if (d.dirty_mask & (1 << static_cast<ae::u16>(PlayerDeltaBit::VelX))) writer.write(d.vel_x);
    if (d.dirty_mask & (1 << static_cast<ae::u16>(PlayerDeltaBit::VelY))) writer.write(d.vel_y);
    if (d.dirty_mask & (1 << static_cast<ae::u16>(PlayerDeltaBit::VelZ))) writer.write(d.vel_z);
    if (d.dirty_mask & (1 << static_cast<ae::u16>(PlayerDeltaBit::Yaw))) writer.write(d.yaw);
    if (d.dirty_mask & (1 << static_cast<ae::u16>(PlayerDeltaBit::Movement))) writer.write(d.movement);
    if (d.dirty_mask & (1 << static_cast<ae::u16>(PlayerDeltaBit::Health))) writer.write(d.health);
    if (d.dirty_mask & (1 << static_cast<ae::u16>(PlayerDeltaBit::Shield))) writer.write(d.shield);
    return true;
}

inline bool read_snapshot_delta(detail::ByteReader& reader, SnapshotDelta& d) {
    if (!reader.read(d.server_tick) || !reader.read(d.last_processed_input) || !reader.read(d.dirty_mask))
        return false;
    if (d.dirty_mask & (1 << static_cast<ae::u16>(PlayerDeltaBit::PosX)) && !reader.read(d.pos_x)) return false;
    if (d.dirty_mask & (1 << static_cast<ae::u16>(PlayerDeltaBit::PosY)) && !reader.read(d.pos_y)) return false;
    if (d.dirty_mask & (1 << static_cast<ae::u16>(PlayerDeltaBit::PosZ)) && !reader.read(d.pos_z)) return false;
    if (d.dirty_mask & (1 << static_cast<ae::u16>(PlayerDeltaBit::VelX)) && !reader.read(d.vel_x)) return false;
    if (d.dirty_mask & (1 << static_cast<ae::u16>(PlayerDeltaBit::VelY)) && !reader.read(d.vel_y)) return false;
    if (d.dirty_mask & (1 << static_cast<ae::u16>(PlayerDeltaBit::VelZ)) && !reader.read(d.vel_z)) return false;
    if (d.dirty_mask & (1 << static_cast<ae::u16>(PlayerDeltaBit::Yaw)) && !reader.read(d.yaw)) return false;
    if (d.dirty_mask & (1 << static_cast<ae::u16>(PlayerDeltaBit::Movement)) && !reader.read(d.movement)) return false;
    if (d.dirty_mask & (1 << static_cast<ae::u16>(PlayerDeltaBit::Health)) && !reader.read(d.health)) return false;
    if (d.dirty_mask & (1 << static_cast<ae::u16>(PlayerDeltaBit::Shield)) && !reader.read(d.shield)) return false;
    return true;
}

/// Envelope wire size: seq(2B) + ack_seq(2B) + ack_bitfield(4B)
constexpr ae::usize kEnvelopeWireSize = sizeof(ae::u16) * 2 + sizeof(ae::u32);

// Minimum hello packet wire size: magic(4)+version(2)+type(2)+envelope(10)+proto_version(2)+token_length(2)
static constexpr ae::usize kMinClientHelloWireSize = sizeof(ae::u32) + sizeof(ae::u16) + sizeof(ae::u16)
    + kEnvelopeWireSize + sizeof(ae::u16) + sizeof(ae::u16);

constexpr ae::usize player_input_packet_size() {
    return sizeof(ae::u32)    // magic
        + sizeof(ae::u16)     // version
        + sizeof(ae::u16)     // type
        + kEnvelopeWireSize   // envelope
        + sizeof(ae::u32)     // sequence
        + sizeof(ae::u32)     // client_tick
        + sizeof(float)       // client_time
        + sizeof(float) * 2   // move_axis
        + sizeof(float) * 2   // look_delta
        + sizeof(ae::u8) * 8; // bools
}

constexpr ae::usize server_snapshot_packet_size() {
    return kEnvelopeWireSize
        + sizeof(ae::u32) + sizeof(ae::u32) + sizeof(ReplicatedPlayerState)
        + sizeof(ae::u8) + kProjectileStateWireSize * 8
        + sizeof(ae::u8) + kDummyStateWireSize * 4
        + sizeof(ae::u8) + sizeof(float) + sizeof(ae::u32)*2 + sizeof(ae::u16)
        + sizeof(ae::u8) + (sizeof(ae::u32) + sizeof(float)*3 + sizeof(float) + sizeof(float)) * 4
        + sizeof(ae::u32) + sizeof(ae::u16)*2 + sizeof(ae::u16); // header
}

inline bool serialize_player_input_packet(
    const PacketEnvelope& envelope,
    const PlayerInputCommand& command,
    std::span<std::byte, player_input_packet_size()> buffer) {
    detail::ByteWriter writer(buffer);
    return detail::write_header(writer, PacketType::PlayerInput)
        && detail::write_envelope(writer, envelope)
        && detail::write_player_input(writer, command)
        && writer.bytes_written() == buffer.size();
}

constexpr ae::usize client_hello_packet_size() {
    return sizeof(ae::u32)    // magic
        + sizeof(ae::u16)     // version
        + sizeof(ae::u16)     // type
        + kEnvelopeWireSize   // envelope
        + sizeof(ae::u16)     // protocol_version
        + sizeof(ae::u16)     // auth_token_length
        + kMaxAuthTokenLength; // auth_token (max)
}

constexpr ae::usize server_welcome_packet_size() {
    return sizeof(ae::u32)    // magic
        + sizeof(ae::u16)     // version
        + sizeof(ae::u16)     // type
        + kEnvelopeWireSize   // envelope
        + sizeof(ae::u16)     // protocol_version
        + kMaxPlayerIdLength; // player_id
}

constexpr ae::usize server_reject_packet_size() {
    return sizeof(ae::u32)    // magic
        + sizeof(ae::u16)     // version
        + sizeof(ae::u16)     // type
        + kEnvelopeWireSize   // envelope
        + sizeof(ae::u16)     // protocol_version
        + sizeof(ae::u8);     // reason
}

inline bool deserialize_player_input_packet(
    std::span<const std::byte> buffer,
    PacketEnvelope& envelope,
    PlayerInputCommand& command) {
    if (buffer.size() != player_input_packet_size()) {
        return false;
    }

    detail::ByteReader reader(buffer);
    return detail::read_header(reader, PacketType::PlayerInput)
        && detail::read_envelope(reader, envelope)
        && detail::read_player_input(reader, command)
        && reader.is_complete();
}

inline bool serialize_server_snapshot_packet(
    const PacketEnvelope& envelope,
    const ServerSnapshot& snapshot,
    std::span<std::byte, server_snapshot_packet_size()> buffer,
    ae::usize* out_bytes_written = nullptr) {
    detail::ByteWriter writer(std::span<std::byte>(buffer.data(), buffer.size()));
    bool ok = detail::write_header(writer, PacketType::ServerSnapshot)
        && detail::write_envelope(writer, envelope)
        && detail::write_snapshot(writer, snapshot);
    if (ok && out_bytes_written) *out_bytes_written = writer.bytes_written();
    return ok;
}

inline bool deserialize_server_snapshot_packet(
    std::span<const std::byte> buffer,
    PacketEnvelope& envelope,
    ServerSnapshot& snapshot) {
    detail::ByteReader reader(buffer);
    return detail::read_header(reader, PacketType::ServerSnapshot)
        && detail::read_envelope(reader, envelope)
        && detail::read_snapshot(reader, snapshot);
}

inline bool serialize_client_hello_packet(
    const PacketEnvelope& envelope,
    const ClientHelloPacket& packet,
    std::span<std::byte, client_hello_packet_size()> buffer,
    ae::usize* out_bytes_written = nullptr) {
    detail::ByteWriter writer(std::span<std::byte>(buffer.data(), buffer.size()));
    bool ok = detail::write_header(writer, PacketType::ClientHello)
        && detail::write_envelope(writer, envelope)
        && detail::write_client_hello(writer, packet);
    if (ok && out_bytes_written) *out_bytes_written = writer.bytes_written();
    return ok;
}

inline bool deserialize_client_hello_packet(
    std::span<const std::byte> buffer,
    PacketEnvelope& envelope,
    ClientHelloPacket& packet) {
    if (buffer.size() < kMinClientHelloWireSize) {
        return false;
    }

    detail::ByteReader reader(buffer);
    return detail::read_header(reader, PacketType::ClientHello)
        && detail::read_envelope(reader, envelope)
        && detail::read_client_hello(reader, packet);
}

inline bool serialize_server_welcome_packet(
    const PacketEnvelope& envelope,
    const ServerWelcomePacket& packet,
    std::span<std::byte, server_welcome_packet_size()> buffer) {
    detail::ByteWriter writer(buffer);
    return detail::write_header(writer, PacketType::ServerWelcome)
        && detail::write_envelope(writer, envelope)
        && detail::write_server_welcome(writer, packet)
        && writer.bytes_written() == buffer.size();
}

inline bool deserialize_server_welcome_packet(
    std::span<const std::byte> buffer,
    PacketEnvelope& envelope,
    ServerWelcomePacket& packet) {
    if (buffer.size() != server_welcome_packet_size()) {
        return false;
    }

    detail::ByteReader reader(buffer);
    return detail::read_header(reader, PacketType::ServerWelcome)
        && detail::read_envelope(reader, envelope)
        && detail::read_server_welcome(reader, packet)
        && reader.is_complete();
}

inline bool serialize_server_reject_packet(
    const PacketEnvelope& envelope,
    const ServerRejectPacket& packet,
    std::span<std::byte, server_reject_packet_size()> buffer) {
    detail::ByteWriter writer(buffer);
    return detail::write_header(writer, PacketType::ServerReject)
        && detail::write_envelope(writer, envelope)
        && detail::write_server_reject(writer, packet)
        && writer.bytes_written() == buffer.size();
}

inline bool deserialize_server_reject_packet(
    std::span<const std::byte> buffer,
    PacketEnvelope& envelope,
    ServerRejectPacket& packet) {
    if (buffer.size() != server_reject_packet_size()) {
        return false;
    }

    detail::ByteReader reader(buffer);
    return detail::read_header(reader, PacketType::ServerReject)
        && detail::read_envelope(reader, envelope)
        && detail::read_server_reject(reader, packet)
        && reader.is_complete();
}

using PlayerInputPacketBuffer = std::array<std::byte, player_input_packet_size()>;
using ServerSnapshotPacketBuffer = std::array<std::byte, server_snapshot_packet_size()>;
using ClientHelloPacketBuffer = std::array<std::byte, client_hello_packet_size()>;
using ServerWelcomePacketBuffer = std::array<std::byte, server_welcome_packet_size()>;
using ServerRejectPacketBuffer = std::array<std::byte, server_reject_packet_size()>;

constexpr ae::usize heartbeat_packet_size() {
    return sizeof(ae::u32)    // magic
        + sizeof(ae::u16)     // version
        + sizeof(ae::u16)     // type
        + kEnvelopeWireSize   // envelope
        + sizeof(ae::u32)     // server_tick
        + sizeof(ae::u32);    // connected_players
}

inline bool serialize_heartbeat_packet(
    const PacketEnvelope& envelope,
    const HeartbeatPacket& packet,
    std::span<std::byte, heartbeat_packet_size()> buffer) {
    detail::ByteWriter writer(buffer);
    return detail::write_header(writer, PacketType::Heartbeat)
        && detail::write_envelope(writer, envelope)
        && detail::write_heartbeat(writer, packet)
        && writer.bytes_written() == buffer.size();
}

inline bool deserialize_heartbeat_packet(
    std::span<const std::byte> buffer,
    PacketEnvelope& envelope,
    HeartbeatPacket& packet) {
    if (buffer.size() != heartbeat_packet_size()) return false;
    detail::ByteReader reader(buffer);
    return detail::read_header(reader, PacketType::Heartbeat)
        && detail::read_envelope(reader, envelope)
        && detail::read_heartbeat(reader, packet)
        && reader.is_complete();
}

constexpr ae::usize client_reconnect_packet_size() {
    return sizeof(ae::u32)    // magic
        + sizeof(ae::u16)     // version
        + sizeof(ae::u16)     // type
        + kEnvelopeWireSize   // envelope
        + sizeof(ae::u64)     // session_id
        + sizeof(ae::u16);    // protocol_version
}

inline bool serialize_client_reconnect_packet(
    const PacketEnvelope& envelope,
    const ClientReconnectPacket& packet,
    std::span<std::byte, client_reconnect_packet_size()> buffer) {
    detail::ByteWriter writer(buffer);
    return detail::write_header(writer, PacketType::ClientReconnect)
        && detail::write_envelope(writer, envelope)
        && detail::write_client_reconnect(writer, packet)
        && writer.bytes_written() == buffer.size();
}

inline bool deserialize_client_reconnect_packet(
    std::span<const std::byte> buffer,
    PacketEnvelope& envelope,
    ClientReconnectPacket& packet) {
    if (buffer.size() != client_reconnect_packet_size()) return false;
    detail::ByteReader reader(buffer);
    return detail::read_header(reader, PacketType::ClientReconnect)
        && detail::read_envelope(reader, envelope)
        && detail::read_client_reconnect(reader, packet)
        && reader.is_complete();
}

using HeartbeatPacketBuffer = std::array<std::byte, heartbeat_packet_size()>;
using ClientReconnectPacketBuffer = std::array<std::byte, client_reconnect_packet_size()>;

}  // namespace ahamkara::game
