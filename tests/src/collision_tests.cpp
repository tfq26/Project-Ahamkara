/**
 * @file collision_tests.cpp
 * @brief Tests for the ae::collision module: types, layers, hitbox resolution,
 *        and trace queries (Jolt-backed when available).
 */

#include "ae/collision/types.h"
#include "ae/collision/layers.h"
#include "ae/collision/trace.h"
#include "ae/collision/world.h"
#include "ae/collision/debug.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <string>

using namespace ae::collision;
using namespace ae;

// ============================================================
// AABB tests
// ============================================================

void test_aabb_contains() {
    AABB box {{0.0F, 0.0F, 0.0F}, {1.0F, 1.0F, 1.0F}};

    assert(box.contains({0.5F, 0.5F, 0.5F}));
    assert(box.contains({0.0F, 0.0F, 0.0F}));
    assert(box.contains({1.0F, 1.0F, 1.0F}));
    assert(!box.contains({-0.1F, 0.5F, 0.5F}));
    assert(!box.contains({1.1F, 0.5F, 0.5F}));
    assert(!box.contains({0.5F, 2.0F, 0.5F}));

    std::cout << "test_aabb_contains passed.\n";
}

void test_aabb_overlaps() {
    AABB a {{0.0F, 0.0F, 0.0F}, {2.0F, 2.0F, 2.0F}};
    AABB b {{1.0F, 1.0F, 1.0F}, {3.0F, 3.0F, 3.0F}};
    AABB c {{3.0F, 3.0F, 3.0F}, {4.0F, 4.0F, 4.0F}};

    assert(a.overlaps(b));
    assert(b.overlaps(a));
    assert(!a.overlaps(c));
    assert(!c.overlaps(a));

    // Touching at edge
    AABB d {{2.0F, 2.0F, 2.0F}, {3.0F, 3.0F, 3.0F}};
    assert(a.overlaps(d));  // shares edge at (2,2,2)

    std::cout << "test_aabb_overlaps passed.\n";
}

void test_aabb_center_extents() {
    AABB box {{-2.0F, -1.0F, 0.0F}, {2.0F, 3.0F, 4.0F}};

    Vec3 c = box.center();
    assert(std::fabs(c.x - 0.0F) < 0.001F);
    assert(std::fabs(c.y - 1.0F) < 0.001F);
    assert(std::fabs(c.z - 2.0F) < 0.001F);

    Vec3 e = box.extents();
    assert(std::fabs(e.x - 2.0F) < 0.001F);
    assert(std::fabs(e.y - 2.0F) < 0.001F);
    assert(std::fabs(e.z - 2.0F) < 0.001F);

    std::cout << "test_aabb_center_extents passed.\n";
}

void test_aabb_expand_empty() {
    AABB box = AABB::make_empty();

    box.expand({0.0F, 0.0F, 0.0F});
    assert(box.contains({0.0F, 0.0F, 0.0F}));

    box.expand({1.0F, -2.0F, 3.0F});
    assert(box.min.x == 0.0F);
    assert(box.min.y == -2.0F);
    assert(box.min.z == 0.0F);
    assert(box.max.x == 1.0F);
    assert(box.max.y == 0.0F);
    assert(box.max.z == 3.0F);

    std::cout << "test_aabb_expand_empty passed.\n";
}

// ============================================================
// CollisionMask tests
// ============================================================

void test_collision_mask_basic() {
    CollisionMask m{};
    m.bits = 0;  // empty mask

    assert(!m.test(GameLayers::PLAYER));

    m.set(GameLayers::PLAYER);
    assert(m.test(GameLayers::PLAYER));
    assert(!m.test(GameLayers::NPC));

    m.set(GameLayers::NPC);
    assert(m.test(GameLayers::PLAYER));
    assert(m.test(GameLayers::NPC));

    m.clear(GameLayers::PLAYER);
    assert(!m.test(GameLayers::PLAYER));
    assert(m.test(GameLayers::NPC));

    std::cout << "test_collision_mask_basic passed.\n";
}

void test_collision_mask_overlaps() {
    CollisionMask a{};
    a.bits = 0;
    a.set(GameLayers::PLAYER);
    a.set(GameLayers::WORLD_STATIC);

    CollisionMask b{};
    b.bits = 0;
    b.set(GameLayers::WORLD_STATIC);

    CollisionMask c{};
    c.bits = 0;
    c.set(GameLayers::NPC);

    assert(a.overlaps(b));
    assert(!b.overlaps(c));
    assert(!a.overlaps(c));

    std::cout << "test_collision_mask_overlaps passed.\n";
}

void test_game_layer_masks() {
    auto pm = GameLayers::player_mask();
    assert(pm.test(GameLayers::WORLD_STATIC));
    assert(pm.test(GameLayers::PLAYER));
    assert(pm.test(GameLayers::TRIGGER));
    assert(!pm.test(GameLayers::PROJECTILE));
    assert(!pm.test(GameLayers::DEBRIS));

    auto proj = GameLayers::projectile_mask();
    assert(proj.test(GameLayers::WORLD_STATIC));
    assert(proj.test(GameLayers::PLAYER));
    assert(!proj.test(GameLayers::TRIGGER));
    assert(!proj.test(GameLayers::PICKUP));

    std::cout << "test_game_layer_masks passed.\n";
}

// ============================================================
// Triangle tests
// ============================================================

void test_triangle_normal() {
    // CCW winding from above: (0,0,0)->(0,0,1)->(1,0,0) gives Y+ normal
    Triangle tri {
        {0.0F, 0.0F, 0.0F},
        {0.0F, 0.0F, 1.0F},
        {1.0F, 0.0F, 0.0F}
    };

    Vec3 n = tri.normal();
    // Normal should be pointing up (Y+)
    assert(std::fabs(n.x - 0.0F) < 0.01F);
    assert(std::fabs(n.y - 1.0F) < 0.01F);
    assert(std::fabs(n.z - 0.0F) < 0.01F);

    // Reverse winding gives opposite normal (Y-)
    Triangle tri_rev {
        {0.0F, 0.0F, 0.0F},
        {1.0F, 0.0F, 0.0F},
        {0.0F, 0.0F, 1.0F}
    };
    Vec3 n2 = tri_rev.normal();
    assert(std::fabs(n2.y + 1.0F) < 0.01F);  // pointing down

    std::cout << "test_triangle_normal passed.\n";
}

// ============================================================
// Hitbox/hurtbox resolution tests
// ============================================================

void test_hitbox_hurtbox_overlap_basic() {
    HitboxInstance hitbox {};
    hitbox.type = HitboxType::Hitbox;
    hitbox.owner_entity = 1;
    hitbox.active = true;
    hitbox.box = AABB{{-0.5F, -0.5F, -0.5F}, {0.5F, 0.5F, 0.5F}};

    HitboxInstance hurtbox {};
    hurtbox.type = HitboxType::Hurtbox;
    hurtbox.owner_entity = 2;  // different owner
    hurtbox.active = true;
    hurtbox.box = AABB{{0.0F, 0.0F, 0.0F}, {1.0F, 1.0F, 1.0F}};

    int hit_idx, hurt_idx;
    int count = resolve_hitbox_hurtbox_overlaps(
        &hitbox, 1, &hurtbox, 1,
        &hit_idx, &hurt_idx, 1
    );

    assert(count == 1);
    assert(hit_idx == 0);
    assert(hurt_idx == 0);

    std::cout << "test_hitbox_hurtbox_overlap_basic passed.\n";
}

void test_hitbox_hurtbox_no_self_damage() {
    HitboxInstance hitbox {};
    hitbox.type = HitboxType::Hitbox;
    hitbox.owner_entity = 42;
    hitbox.active = true;
    hitbox.box = AABB{{-1.0F, -1.0F, -1.0F}, {1.0F, 1.0F, 1.0F}};

    HitboxInstance hurtbox {};
    hurtbox.type = HitboxType::Hurtbox;
    hurtbox.owner_entity = 42;  // SAME owner
    hurtbox.active = true;
    hurtbox.box = AABB{{-0.5F, -0.5F, -0.5F}, {0.5F, 0.5F, 0.5F}};

    int hit_idx, hurt_idx;
    int count = resolve_hitbox_hurtbox_overlaps(
        &hitbox, 1, &hurtbox, 1,
        &hit_idx, &hurt_idx, 1
    );

    assert(count == 0);  // self-damage ignored

    std::cout << "test_hitbox_hurtbox_no_self_damage passed.\n";
}

void test_hitbox_hurtbox_multiple() {
    HitboxInstance hitboxes[2] {};
    hitboxes[0].type = HitboxType::Hitbox;
    hitboxes[0].owner_entity = 1;
    hitboxes[0].active = true;
    hitboxes[0].box = AABB{{-1.0F, -1.0F, -1.0F}, {1.0F, 1.0F, 1.0F}};

    hitboxes[1].type = HitboxType::Hitbox;
    hitboxes[1].owner_entity = 1;
    hitboxes[1].active = true;
    hitboxes[1].box = AABB{{10.0F, 10.0F, 10.0F}, {11.0F, 11.0F, 11.0F}}; // far away

    HitboxInstance hurtboxes[3] {};
    hurtboxes[0].type = HitboxType::Hurtbox;
    hurtboxes[0].owner_entity = 2;
    hurtboxes[0].active = true;
    hurtboxes[0].box = AABB{{0.0F, 0.0F, 0.0F}, {2.0F, 2.0F, 2.0F}};

    hurtboxes[1].type = HitboxType::Hurtbox;
    hurtboxes[1].owner_entity = 3;
    hurtboxes[1].active = false;  // inactive
    hurtboxes[1].box = AABB{{-2.0F, -2.0F, -2.0F}, {2.0F, 2.0F, 2.0F}};

    hurtboxes[2].type = HitboxType::Hurtbox;
    hurtboxes[2].owner_entity = 4;
    hurtboxes[2].active = true;
    hurtboxes[2].box = AABB{{10.0F, 10.0F, 10.0F}, {12.0F, 12.0F, 12.0F}};

    int hit_idx[4], hurt_idx[4];
    int count = resolve_hitbox_hurtbox_overlaps(
        hitboxes, 2, hurtboxes, 3,
        hit_idx, hurt_idx, 4
    );

    // hitbox[0] overlaps hurtbox[0]
    // hitbox[0] does NOT overlap hurtbox[1] (inactive)
    // hitbox[1] overlaps hurtbox[2]
    assert(count == 2);
    assert(hit_idx[0] == 0);
    assert(hurt_idx[0] == 0);
    assert(hit_idx[1] == 1);
    assert(hurt_idx[1] == 2);

    std::cout << "test_hitbox_hurtbox_multiple passed.\n";
}

void test_hitbox_hurtbox_damage_multiplier() {
    // Head hitbox with 2x multiplier
    HitboxInstance hurtbox_head {};
    hurtbox_head.type = HitboxType::Hurtbox;
    hurtbox_head.owner_entity = 2;
    hurtbox_head.active = true;
    hurtbox_head.box_index = 1;  // head
    hurtbox_head.damage_multiplier = 2.0F;
    hurtbox_head.box = AABB{{0.0F, 1.5F, 0.0F}, {0.5F, 2.0F, 0.5F}};

    // Body hurtbox with 1x multiplier
    HitboxInstance hurtbox_body {};
    hurtbox_body.type = HitboxType::Hurtbox;
    hurtbox_body.owner_entity = 2;
    hurtbox_body.active = true;
    hurtbox_body.box_index = 0;  // body
    hurtbox_body.damage_multiplier = 1.0F;
    hurtbox_body.box = AABB{{0.0F, 0.0F, 0.0F}, {0.5F, 1.5F, 0.5F}};

    HitboxInstance hitbox {};
    hitbox.type = HitboxType::Hitbox;
    hitbox.owner_entity = 1;
    hitbox.active = true;
    hitbox.box = AABB{{0.0F, 1.5F, 0.0F}, {0.5F, 2.0F, 0.5F}}; // hits head

    int hit_idx, hurt_idx;
    int count = resolve_hitbox_hurtbox_overlaps(
        &hitbox, 1, &hurtbox_head, 1,
        &hit_idx, &hurt_idx, 1
    );

    assert(count == 1);
    assert(hurt_idx == 0);
    // The hurtbox at index 0 has damage_multiplier = 2.0 (headshot)
    // Real game code would read this from hurtboxes[hurt_idx].damage_multiplier

    std::cout << "test_hitbox_hurtbox_damage_multiplier passed.\n";
}

// ============================================================
// CollisionWorld tests (Jolt-backed integration test)
// ============================================================

void test_collision_world_create() {
    CollisionWorld world;
    CollisionStats stats = world.get_stats();
    assert(stats.body_count == 0);

    std::cout << "test_collision_world_create passed.\n";
}

void test_collision_world_add_static_body() {
    CollisionWorld world;

    BodyDef def {};
    def.type = BodyType::Static;
    def.collider.shape = ColliderShape::Box;
    def.collider.half_extents = {1.0F, 1.0F, 1.0F};
    def.position = {0.0F, 0.0F, 0.0F};
    def.layer = GameLayers::WORLD_STATIC;
    def.user_data = 42;

    BodyHandle handle = world.add_body(def);
    assert(handle != kInvalidBodyHandle);

    CollisionStats stats = world.get_stats();
    assert(stats.body_count == 1);

    Vec3 pos = world.get_body_position(handle);
    assert(std::fabs(pos.x - 0.0F) < 0.001F);
    assert(std::fabs(pos.y - 0.0F) < 0.001F);
    assert(std::fabs(pos.z - 0.0F) < 0.001F);

    u64 ud = world.get_body_user_data(handle);
    assert(ud == 42);

    AABB bounds = world.get_body_aabb(handle);
    assert(std::fabs(bounds.min.x + 1.0F) < 0.01F);
    assert(std::fabs(bounds.max.x - 1.0F) < 0.01F);

    // Cleanup
    world.remove_body(handle);
    assert(world.get_stats().body_count == 0);

    std::cout << "test_collision_world_add_static_body passed.\n";
}

void test_collision_world_sphere_body() {
    CollisionWorld world;

    BodyDef def {};
    def.type = BodyType::Static;
    def.collider.shape = ColliderShape::Sphere;
    def.collider.radius = 0.5F;
    def.position = {0.0F, 1.0F, 0.0F};
    def.layer = GameLayers::PLAYER;

    BodyHandle handle = world.add_body(def);
    assert(handle != kInvalidBodyHandle);

    AABB bounds = world.get_body_aabb(handle);
    assert(std::fabs(bounds.min.x + 0.5F) < 0.01F);
    assert(std::fabs(bounds.max.x - 0.5F) < 0.01F);
    assert(std::fabs(bounds.min.y - 0.5F) < 0.01F);
    assert(std::fabs(bounds.max.y - 1.5F) < 0.01F);

    world.remove_body(handle);

    std::cout << "test_collision_world_sphere_body passed.\n";
}

void test_collision_world_capsule_body() {
    CollisionWorld world;

    BodyDef def {};
    def.type = BodyType::Static;
    def.collider.shape = ColliderShape::Capsule;
    def.collider.radius = 0.5F;
    def.collider.capsule_half_height = 1.0F;
    def.position = {0.0F, 1.0F, 0.0F};
    def.layer = GameLayers::NPC;

    BodyHandle handle = world.add_body(def);
    assert(handle != kInvalidBodyHandle);

    AABB bounds = world.get_body_aabb(handle);
    // Capsule half_height=1.0, radius=0.5 => extends from -1.5 to +1.5 in Y relative to center
    assert(std::fabs(bounds.min.y - (-0.5F)) < 0.01F); // 1.0 - 1.5 = -0.5
    assert(std::fabs(bounds.max.y - 2.5F) < 0.01F);    // 1.0 + 1.5 = 2.5

    world.remove_body(handle);

    std::cout << "test_collision_world_capsule_body passed.\n";
}

void test_collision_world_sensor_body() {
    CollisionWorld world;

    BodyDef def {};
    def.type = BodyType::Static;
    def.collider.shape = ColliderShape::Box;
    def.collider.half_extents = {2.0F, 2.0F, 2.0F};
    def.position = {0.0F, 0.0F, 0.0F};
    def.layer = GameLayers::TRIGGER;
    def.is_sensor = true;

    BodyHandle handle = world.add_body(def);
    assert(handle != kInvalidBodyHandle);

    world.step(0.016F);
    CollisionStats stats = world.get_stats();
    assert(stats.body_count == 1);

    world.remove_body(handle);

    std::cout << "test_collision_world_sensor_body passed.\n";
}

void test_collision_world_triangle_mesh() {
    CollisionWorld world;

    Triangle tris[2] = {
        {{0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 1.0F}},
        {{1.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 1.0F}, {0.0F, 0.0F, 1.0F}},
    };

    BodyDef def {};
    def.type = BodyType::Static;
    def.collider.shape = ColliderShape::TriangleMesh;
    def.collider.triangles = tris;
    def.collider.triangle_count = 2;
    def.position = {0.0F, 0.0F, 0.0F};
    def.layer = GameLayers::WORLD_STATIC;

    BodyHandle handle = world.add_body(def);
    assert(handle != kInvalidBodyHandle);

    AABB bounds = world.get_body_aabb(handle);
    assert(std::fabs(bounds.min.y - 0.0F) < 0.1F);
    assert(std::fabs(bounds.max.x - 1.0F) < 0.1F);

    world.remove_body(handle);

    std::cout << "test_collision_world_triangle_mesh passed.\n";
}

// ============================================================
// Trace tests (Jolt-backed)
// ============================================================

void test_ray_trace_hit() {
    CollisionWorld world;

    // Add a ground plane (large box at y=-0.5)
    BodyDef def {};
    def.type = BodyType::Static;
    def.collider.shape = ColliderShape::Box;
    def.collider.half_extents = {10.0F, 0.5F, 10.0F};
    def.position = {0.0F, -0.5F, 0.0F};
    def.layer = GameLayers::WORLD_STATIC;
    (void)world.add_body(def);

    // Ray cast straight down
    Ray ray {{0.0F, 5.0F, 0.0F}, {0.0F, -1.0F, 0.0F}};
    TraceParams params;
    params.layer_mask = GameLayers::camera_mask();

    TraceResult result = ray_trace(world, ray, params);
    assert(result.hit);
    assert(result.distance > 4.0F && result.distance < 6.0F);
    // Hit normal should point up
    assert(result.normal.y > 0.9F);
    // Hit position should be on the top face
    assert(std::fabs(result.position.y - 0.0F) < 0.01F);

    std::cout << "test_ray_trace_hit passed (dist=" << result.distance << ").\n";
}

void test_ray_trace_miss() {
    CollisionWorld world;

    BodyDef def {};
    def.type = BodyType::Static;
    def.collider.shape = ColliderShape::Box;
    def.collider.half_extents = {1.0F, 1.0F, 1.0F};
    def.position = {10.0F, 0.0F, 10.0F};
    def.layer = GameLayers::WORLD_STATIC;
    (void)world.add_body(def);

    // Ray from origin going away from the box
    Ray ray {{0.0F, 0.0F, 0.0F}, {0.0F, 1.0F, 0.0F}};
    TraceParams params;
    params.layer_mask = GameLayers::camera_mask();

    TraceResult result = ray_trace(world, ray, params);
    assert(!result.hit);

    std::cout << "test_ray_trace_miss passed.\n";
}

void test_ray_trace_layer_mask() {
    CollisionWorld world;

    // Add a static world box at origin
    BodyDef world_box {};
    world_box.type = BodyType::Static;
    world_box.collider.shape = ColliderShape::Box;
    world_box.collider.half_extents = {1.0F, 1.0F, 1.0F};
    world_box.position = {0.0F, 0.0F, 0.0F};
    world_box.layer = GameLayers::WORLD_STATIC;
    (void)world.add_body(world_box);

    // Add a player box at origin (overlapping)
    BodyDef player_box {};
    player_box.type = BodyType::Static;
    player_box.collider.shape = ColliderShape::Box;
    player_box.collider.half_extents = {0.5F, 0.5F, 0.5F};
    player_box.position = {0.0F, 0.0F, 0.0F};
    player_box.layer = GameLayers::PLAYER;
    (void)world.add_body(player_box);

    // Ray with ONLY world_static mask - should hit the world box
    Ray ray {{2.0F, 0.0F, 0.0F}, {-1.0F, 0.0F, 0.0F}};

    TraceParams params;
    params.layer_mask = GameLayers::camera_mask();  // only WORLD_STATIC + DYNAMIC

    TraceResult result = ray_trace(world, ray, params);
    assert(result.hit);
    // The hit distance should be about 1.0 (touching the world box face)
    assert(result.distance < 1.5F);

    // Now trace with only PLAYER mask
    TraceParams player_params;
    player_params.layer_mask = CollisionMask{};
    player_params.layer_mask.bits = 0;
    player_params.layer_mask.set(GameLayers::PLAYER);

    TraceResult result2 = ray_trace(world, ray, player_params);
    assert(result2.hit);
    // Hit the player box, which is smaller (0.5 extent)
    assert(result2.distance > 1.0F); // hits the inner player box

    std::cout << "test_ray_trace_layer_mask passed.\n";
}

void test_sphere_trace() {
    CollisionWorld world;

    // Ground at y=0
    BodyDef ground {};
    ground.type = BodyType::Static;
    ground.collider.shape = ColliderShape::Box;
    ground.collider.half_extents = {10.0F, 0.5F, 10.0F};
    ground.position = {0.0F, -0.5F, 0.0F};
    ground.layer = GameLayers::WORLD_STATIC;
    (void)world.add_body(ground);

    // Sweep a sphere from above down toward the ground
    Vec3 start {0.0F, 5.0F, 0.0F};
    Vec3 dir {0.0F, -1.0F, 0.0F};
    TraceParams params;
    params.layer_mask = GameLayers::camera_mask();

    TraceResult result = sphere_trace(world, start, dir, 10.0F, 1.0F, params);
    assert(result.hit);
    // Sphere radius=1.0, ground top at y=0 => hit at y≈1.0
    assert(std::fabs(result.position.y - 1.0F) < 0.1F);

    std::cout << "test_sphere_trace passed (hit at y=" << result.position.y << ").\n";
}

void test_capsule_trace() {
    CollisionWorld world;

    // Wall at x=5
    BodyDef wall {};
    wall.type = BodyType::Static;
    wall.collider.shape = ColliderShape::Box;
    wall.collider.half_extents = {0.5F, 5.0F, 5.0F};
    wall.position = {5.0F, 0.0F, 0.0F};
    wall.layer = GameLayers::WORLD_STATIC;
    (void)world.add_body(wall);

    // Sweep a capsule toward the wall
    Vec3 start {0.0F, 0.0F, 0.0F};
    Vec3 dir {1.0F, 0.0F, 0.0F};
    TraceParams params;
    params.layer_mask = GameLayers::camera_mask();

    TraceResult result = capsule_trace(world, start, dir, 20.0F, 1.0F, 0.5F, params);
    assert(result.hit);
    // Capsule radius=0.5, wall left face at x=4.5 => hit at x≈4.0
    assert(result.position.x > 3.5F && result.position.x < 4.5F);

    std::cout << "test_capsule_trace passed (hit at x=" << result.position.x << ").\n";
}

void test_sphere_overlap() {
    CollisionWorld world;

    BodyDef box {};
    box.type = BodyType::Static;
    box.collider.shape = ColliderShape::Box;
    box.collider.half_extents = {1.0F, 1.0F, 1.0F};
    box.position = {0.0F, 0.0F, 0.0F};
    box.layer = GameLayers::WORLD_STATIC;
    (void)world.add_body(box);

    TraceParams params;
    params.layer_mask = GameLayers::camera_mask();

    // Sphere overlapping the box
    assert(sphere_overlap(world, {0.0F, 0.0F, 0.0F}, 2.0F, params));

    // Sphere far away
    assert(!sphere_overlap(world, {10.0F, 10.0F, 10.0F}, 0.5F, params));

    std::cout << "test_sphere_overlap passed.\n";
}

void test_aabb_query() {
    CollisionWorld world;

    for (int i = 0; i < 4; ++i) {
        BodyDef def {};
        def.type = BodyType::Static;
        def.collider.shape = ColliderShape::Box;
        def.collider.half_extents = {0.5F, 0.5F, 0.5F};
        def.position = {static_cast<float>(i * 2), 0.0F, 0.0F};
        def.layer = GameLayers::WORLD_STATIC;
        (void)world.add_body(def);
    }

    BodyHandle handles[16];
    AABB query_box {{0.0F, -1.0F, -1.0F}, {4.0F, 1.0F, 1.0F}};
    CollisionMask mask;
    int count = world.query_aabb(query_box, mask, handles, 16);
    assert(count == 3); // boxes at x=0, 2, 4 (x=6 is outside)

    std::cout << "test_aabb_query passed (found " << count << " bodies).\n";
}

// ============================================================
// Debug overlay tests
// ============================================================

void test_debug_overlay_clear() {
    DebugOverlay overlay;
    overlay.box_count = 5;
    overlay.line_count = 3;
    overlay.sphere_count = 2;

    overlay.clear();

    assert(overlay.box_count == 0);
    assert(overlay.line_count == 0);
    assert(overlay.sphere_count == 0);
    assert(overlay.stats_text == nullptr);

    std::cout << "test_debug_overlay_clear passed.\n";
}

void test_debug_overlay_populate() {
    CollisionWorld world;

    BodyDef def {};
    def.type = BodyType::Static;
    def.collider.shape = ColliderShape::Box;
    def.collider.half_extents = {1.0F, 1.0F, 1.0F};
    def.position = {0.0F, 0.0F, 0.0F};
    def.layer = GameLayers::WORLD_STATIC;
    (void)world.add_body(def);

    DebugOverlay overlay;
    populate_debug_overlay(world, overlay);

    assert(overlay.box_count > 0);
    assert(overlay.stats_text != nullptr);

    // Test stats text
    std::string stats(overlay.stats_text);
    assert(!stats.empty());

    std::cout << "test_debug_overlay_populate passed (" << overlay.stats_text << ").\n";
}

void test_debug_hitboxes() {
    HitboxInstance hitboxes[2];
    hitboxes[0].type = HitboxType::Hitbox;
    hitboxes[0].active = true;
    hitboxes[0].box = AABB{{0.0F, 0.0F, 0.0F}, {1.0F, 1.0F, 1.0F}};

    hitboxes[1].type = HitboxType::Hurtbox;
    hitboxes[1].active = false;  // inactive, should be skipped
    hitboxes[1].box = AABB{{-1.0F, -1.0F, -1.0F}, {0.0F, 0.0F, 0.0F}};

    DebugOverlay overlay;
    populate_debug_hitboxes(hitboxes, 2, overlay);

    assert(overlay.box_count == 1);  // only active hitbox

    std::cout << "test_debug_hitboxes passed.\n";
}

// ============================================================
// Runner
// ============================================================

int main() {
    std::cout << "\n=== Collision AABB Tests ===\n";
    test_aabb_contains();
    test_aabb_overlaps();
    test_aabb_center_extents();
    test_aabb_expand_empty();

    std::cout << "\n=== CollisionMask Tests ===\n";
    test_collision_mask_basic();
    test_collision_mask_overlaps();
    test_game_layer_masks();

    std::cout << "\n=== Triangle Tests ===\n";
    test_triangle_normal();

    std::cout << "\n=== Hitbox/Hurtbox Tests ===\n";
    test_hitbox_hurtbox_overlap_basic();
    test_hitbox_hurtbox_no_self_damage();
    test_hitbox_hurtbox_multiple();
    test_hitbox_hurtbox_damage_multiplier();

    std::cout << "\n=== CollisionWorld Tests ===\n";
    test_collision_world_create();
    test_collision_world_add_static_body();
    test_collision_world_sphere_body();
    test_collision_world_capsule_body();
    test_collision_world_sensor_body();
    test_collision_world_triangle_mesh();

    std::cout << "\n=== Trace Tests ===\n";
    test_ray_trace_hit();
    test_ray_trace_miss();
    test_ray_trace_layer_mask();
    test_sphere_trace();
    test_capsule_trace();
    test_sphere_overlap();
    test_aabb_query();

    std::cout << "\n=== Debug Overlay Tests ===\n";
    test_debug_overlay_clear();
    test_debug_overlay_populate();
    test_debug_hitboxes();

    std::cout << "\nAll collision tests passed.\n";
    return 0;
}
