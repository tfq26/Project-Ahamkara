#include "ae/runtime/application.h"
#include "ahamkara/game/movement.h"
#include "ahamkara/game/net_packets.h"
#include "ahamkara/game/net_types.h"

#include <cassert>
#include <cstddef>

void run_camera_smoke_tests();
void run_local_play_tests();

namespace {

void test_application_lifecycle() {
    ae::Application application(ae::RuntimeMode::Tests);
    assert(!application.is_running());
    assert(application.mode() == ae::RuntimeMode::Tests);

    application.start();
    assert(application.is_running());

    application.shutdown();
    assert(!application.is_running());
}

void test_player_input_packet_round_trip() {
    ahamkara::game::PlayerInputCommand source {};
    source.sequence = 42;
    source.client_tick = 84;
    source.client_time = 1.5F;
    source.move_axis = {0.25F, 1.0F};
    source.look_delta = {1.0F, -0.5F};
    source.jump_pressed = true;
    source.crouch_held = false;
    source.sprint_held = true;
    source.slide_pressed = false;
    source.fire_held = true;
    source.reload_pressed = false;
    source.ability_pressed = true;

    ahamkara::game::PlayerInputPacketBuffer buffer {};
    const bool serialized = ahamkara::game::serialize_player_input_packet(source, buffer);
    assert(serialized);

    ahamkara::game::PlayerInputCommand decoded {};
    const bool deserialized = ahamkara::game::deserialize_player_input_packet(buffer, decoded);
    assert(deserialized);

    assert(decoded.sequence == source.sequence);
    assert(decoded.client_tick == source.client_tick);
    assert(decoded.client_time == source.client_time);
    assert(decoded.move_axis.x == source.move_axis.x);
    assert(decoded.move_axis.y == source.move_axis.y);
    assert(decoded.look_delta.x == source.look_delta.x);
    assert(decoded.look_delta.y == source.look_delta.y);
    assert(decoded.jump_pressed == source.jump_pressed);
    assert(decoded.crouch_held == source.crouch_held);
    assert(decoded.sprint_held == source.sprint_held);
    assert(decoded.slide_pressed == source.slide_pressed);
    assert(decoded.fire_held == source.fire_held);
    assert(decoded.reload_pressed == source.reload_pressed);
    assert(decoded.ability_pressed == source.ability_pressed);
}

void test_snapshot_packet_round_trip() {
    ahamkara::game::ServerSnapshot source {};
    source.server_tick = 7;
    source.last_processed_input = 99;
    source.local_player.network_object_id = 123;
    source.local_player.player_id = 456;
    source.local_player.position = {10.0F, 1.0F, -3.0F};
    source.local_player.velocity = {8.0F, 0.0F, 2.0F};
    source.local_player.yaw = 180.0F;
    source.local_player.movement_state = ahamkara::game::MovementState::Sprinting;
    source.local_player.health = 75.0F;
    source.local_player.shield = 25.0F;

    ahamkara::game::ServerSnapshotPacketBuffer buffer {};
    const bool serialized = ahamkara::game::serialize_server_snapshot_packet(source, buffer);
    assert(serialized);

    ahamkara::game::ServerSnapshot decoded {};
    const bool deserialized = ahamkara::game::deserialize_server_snapshot_packet(buffer, decoded);
    assert(deserialized);

    assert(decoded.server_tick == source.server_tick);
    assert(decoded.last_processed_input == source.last_processed_input);
    assert(decoded.local_player.network_object_id == source.local_player.network_object_id);
    assert(decoded.local_player.player_id == source.local_player.player_id);
    assert(decoded.local_player.position.x == source.local_player.position.x);
    assert(decoded.local_player.position.y == source.local_player.position.y);
    assert(decoded.local_player.position.z == source.local_player.position.z);
    assert(decoded.local_player.velocity.x == source.local_player.velocity.x);
    assert(decoded.local_player.velocity.y == source.local_player.velocity.y);
    assert(decoded.local_player.velocity.z == source.local_player.velocity.z);
    assert(decoded.local_player.yaw == source.local_player.yaw);
    assert(decoded.local_player.movement_state == source.local_player.movement_state);
    assert(decoded.local_player.health == source.local_player.health);
    assert(decoded.local_player.shield == source.local_player.shield);
}

void test_packet_validation_rejects_corruption() {
    ahamkara::game::PlayerInputCommand source {};
    ahamkara::game::PlayerInputPacketBuffer buffer {};
    assert(ahamkara::game::serialize_player_input_packet(source, buffer));

    buffer[0] = std::byte {0x00};

    ahamkara::game::PlayerInputCommand decoded {};
    const bool deserialized = ahamkara::game::deserialize_player_input_packet(buffer, decoded);
    assert(!deserialized);
}

void test_shared_movement_simulation() {
    ahamkara::game::ReplicatedPlayerState player_state {};
    ahamkara::game::PlayerInputCommand command {};
    command.move_axis.y = 1.0F;
    command.sprint_held = true;

    ahamkara::game::simulate_player_movement(player_state, command, 1.0F);

    assert(player_state.position.x == 0.0F);
    assert(player_state.position.z == 8.0F);
    assert(player_state.velocity.z == 8.0F);
    assert(player_state.movement_state == ahamkara::game::MovementState::Sprinting);
}

}  // namespace

int main() {
    test_application_lifecycle();
    test_player_input_packet_round_trip();
    test_snapshot_packet_round_trip();
    test_packet_validation_rejects_corruption();
    test_shared_movement_simulation();
    run_camera_smoke_tests();
    run_local_play_tests();
    return 0;
}
