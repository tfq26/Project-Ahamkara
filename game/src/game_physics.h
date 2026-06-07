#pragma once

// ============================================================================
// MIGRATION SEAM: Game-owned Jolt Physics
// ============================================================================
//
// This header is the managed boundary between game code and direct Jolt usage.
// It exists so that every Jolt dependency in the game layer is routed through
// a single, documented header — not scattered across source files.
//
// WHY GAME OWNS JOLT DIRECTLY (current state):
//   The game uses JPH::CharacterVirtual (kinematic character controller / KCC)
//   which must share a PhysicsSystem with the bodies it interacts with (map
//   geometry, NPCs).  ae::collision::CollisionWorld does not (yet) abstract
//   character controllers, so the game layer owns its own PhysicsSystem.
//
// OWNERSHIP RULE:
//   Only game/src/ files may include Jolt headers directly.  No other layer
//   may depend on Jolt types.  All engine-side collision code goes through
//   ae::collision, which encapsulates Jolt behind its private Impl (see
//   engine/collision/src/jolt_backend.h).
//
// TWO PHYSICS WORLDS (current state):
//   ┌─────────────────────────────────────────────────────────────────┐
//   │ ae::collision::CollisionWorld  (in engine/collision/)          │
//   │   • Fully-featured Jolt wrapper (body CRUD, traces, layers)    │
//   │   • Used by: collision tests only (not wired to game yet)      │
//   │   • 10+ collision layers, 64-bit masks, backend-agnostic API   │
//   └─────────────────────────────────────────────────────────────────┘
//   ┌─────────────────────────────────────────────────────────────────┐
//   │ ahamkara::game::GamePhysics  (defined in this file)             │
//   │   • Minimal Jolt wrapper (2 layers: NON_MOVING/MOVING)         │
//   │   • Owns CharacterVirtual KCC                                  │
//   │   • Used by: world.cpp (movement), world_projectile.cpp (shots) │
//   │   • Owning the PhysicsSystem is MANDATORY while KCC is in game │
//   └─────────────────────────────────────────────────────────────────┘
//
// MIGRATION PATH (toward CollisionWorld unification):
//   1. Small step (safe now):
//      Add accessor methods to GamePhysics that mirror CollisionWorld's
//      API (set_body_position, cast_ray, etc.).  This makes future
//      migration a search-and-replace.
//   2. Medium step (requires CollisionWorld change):
//      Add a CharacterController abstraction to ae::collision (or a
//      companion ae::collision::Character).  Once that exists, the game
//      can drop its PhysicsSystem and delegate body management to
//      CollisionWorld.
//   3. Final step (major):
//      Remove the game-owned PhysicsSystem entirely.  All bodies live in
//      CollisionWorld; the character controller (now owned by ae_collision)
//      references that world.  game_physics.h becomes a thin adapter that
//      composes CollisionWorld + ae::collision::Character.
//
// CURRENT BARRIERS TO MIGRATION:
//   a) JPH::CharacterVirtual is not abstracted by CollisionWorld.
//   b) The game uses a flat 2-layer system (NON_MOVING/MOVING) while
//      CollisionWorld uses a 10+ layer system; bodies would need
//      re-layering.
//   c) Projectile rollback temporarily repositions dummy bodies for
//      raycasts; CollisionWorld would need a "scratch transform" or
//      query-time override to support this pattern efficiently.
//
// WHEN TO ADD NEW JOLT USAGE:
//   Prefer adding new bodies / traces through CollisionWorld if the
//   feature does NOT require interacting with the character controller's
//   PhysicsSystem.  For anything that must share the KCC's physics world,
//   add it here and mark it with a // MIGRATION SEAM comment.
// ============================================================================

// All game code that needs Jolt should include this header instead of
// including Jolt headers directly.  This keeps the migration surface
// narrow and documented.

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>

#include "world_jolt_bridge.h"
