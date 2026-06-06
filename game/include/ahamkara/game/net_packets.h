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
constexpr ae::u16 kPacketVersion = 1;

enum class PacketType : ae::u16 {
    PlayerInput = 1,
    ServerSnapshot = 2
};

struct PacketHeader {
    ae::u32 magic {kPacketMagic};
    ae::u16 version {kPacketVersion};
    PacketType type {PacketType::PlayerInput};
};

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
        && version == kPacketVersion
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
        && writer.write_bool(command.ability_pressed);
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
        && reader.read_bool(command.ability_pressed);
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
    return writer.write(snapshot.server_tick)
        && writer.write(snapshot.last_processed_input)
        && write_player_state(writer, snapshot.local_player);
}

inline bool read_snapshot(ByteReader& reader, ServerSnapshot& snapshot) {
    return reader.read(snapshot.server_tick)
        && reader.read(snapshot.last_processed_input)
        && read_player_state(reader, snapshot.local_player);
}

}  // namespace detail

constexpr ae::usize player_input_packet_size() {
    return sizeof(ae::u32)
        + sizeof(ae::u16)
        + sizeof(ae::u16)
        + sizeof(ae::u32)
        + sizeof(ae::u32)
        + sizeof(float)
        + sizeof(float) * 2
        + sizeof(float) * 2
        + sizeof(ae::u8) * 7;
}

constexpr ae::usize server_snapshot_packet_size() {
    return sizeof(ae::u32)
        + sizeof(ae::u16)
        + sizeof(ae::u16)
        + sizeof(ae::u32)
        + sizeof(ae::u32)
        + sizeof(ae::u32)
        + sizeof(ae::u32)
        + sizeof(float) * 3
        + sizeof(float) * 3
        + sizeof(float)
        + sizeof(ae::u8)
        + sizeof(float)
        + sizeof(float);
}

inline bool serialize_player_input_packet(
    const PlayerInputCommand& command,
    std::span<std::byte, player_input_packet_size()> buffer) {
    detail::ByteWriter writer(buffer);
    return detail::write_header(writer, PacketType::PlayerInput)
        && detail::write_player_input(writer, command)
        && writer.bytes_written() == buffer.size();
}

inline bool deserialize_player_input_packet(
    std::span<const std::byte> buffer,
    PlayerInputCommand& command) {
    if (buffer.size() != player_input_packet_size()) {
        return false;
    }

    detail::ByteReader reader(buffer);
    return detail::read_header(reader, PacketType::PlayerInput)
        && detail::read_player_input(reader, command)
        && reader.is_complete();
}

inline bool serialize_server_snapshot_packet(
    const ServerSnapshot& snapshot,
    std::span<std::byte, server_snapshot_packet_size()> buffer) {
    detail::ByteWriter writer(buffer);
    return detail::write_header(writer, PacketType::ServerSnapshot)
        && detail::write_snapshot(writer, snapshot)
        && writer.bytes_written() == buffer.size();
}

inline bool deserialize_server_snapshot_packet(
    std::span<const std::byte> buffer,
    ServerSnapshot& snapshot) {
    if (buffer.size() != server_snapshot_packet_size()) {
        return false;
    }

    detail::ByteReader reader(buffer);
    return detail::read_header(reader, PacketType::ServerSnapshot)
        && detail::read_snapshot(reader, snapshot)
        && reader.is_complete();
}

using PlayerInputPacketBuffer = std::array<std::byte, player_input_packet_size()>;
using ServerSnapshotPacketBuffer = std::array<std::byte, server_snapshot_packet_size()>;

}  // namespace ahamkara::game
