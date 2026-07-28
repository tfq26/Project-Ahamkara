#pragma once

#include "ahamkara/game/gameplay_types.h"
#include "ahamkara/game/net_types.h"

#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace ahamkara::game {

// ===========================================================================
// Pickup Types — game-owned taxonomy of pickups available in the world.
// ===========================================================================

/// The kind of pickup/reward that can be granted to a player.
/// Each value has deterministic rules for how it integrates with inventory,
/// loadout, and ability state.
enum class PickupType : std::uint8_t {
    // --- Ammo ---
    PrimaryAmmo = 0,     ///< Primary weapon ammo (e.g. auto rifle rounds)
    SecondaryAmmo = 1,   ///< Secondary weapon ammo (e.g. sidearm rounds)
    HeavyAmmo = 2,       ///< Heavy weapon ammo (e.g. rocket, sniper)
    SpecialAmmo = 3,     ///< Special weapon ammo (e.g. fusion, grenade launcher)

    // --- Health & Shield ---
    HealthPack = 4,      ///< Small health restore (e.g. +30 HP)
    LargeHealth = 5,     ///< Full health restore
    ShieldCharge = 6,    ///< Shield restoration (e.g. +50 shield)

    // --- Equipment ---
    WeaponDrop = 7,      ///< A new weapon item
    ArmorDrop = 8,       ///< A new armor piece
    PerkDrop = 9,        ///< A perk/mod item

    // --- Ability Charges ---
    GrenadeCharge = 10,  ///< Restores one grenade charge
    UltimateEnergy = 11, ///< Grants ultimate energy (0.0 – 1.0)

    // --- Progression Currency ---
    XPOrb = 12,          ///< Experience point award
    Currency = 13,       ///< In-game currency (glimmer, credits)

    // Sentinel — not a real pickup
    Count = 14
};

/// Result of attempting to grant a reward to a player.
enum class GrantResult : std::uint8_t {
    Granted = 0,          ///< Reward was successfully applied
    InventoryFull = 1,    ///< Inventory has no capacity for this item
    Duplicate = 2,        ///< This grant was already processed (idempotency)
    AtCap = 3,            ///< Resource is at its maximum (e.g. full health, max ammo)
    Blocked = 4,          ///< Game rules prevent granting this reward
    Invalid = 5,          ///< Grant parameters are invalid
};

// ===========================================================================
// GrantId — Stable idempotency identity for reward grants
// ===========================================================================

/// A stable, globally unique identity for every reward grant.
///
/// Design for persistence: the triple (source_id, play_session_id, sequence)
/// is unique across the entire game lifetime.  Every grant is recorded in the
/// progression snapshot so replays and reconnections never double-grant.
struct GrantId {
    ae::u64 source_id {0};         ///< Entity/event that generated this grant
    ae::u32 play_session_id {0};   ///< The play session that produced it
    ae::u32 sequence {0};          ///< Monotonic counter within the session

    /// Human-readable string suitable for storage keys.
    [[nodiscard]] std::string to_string() const;

    /// Parse from string produced by to_string().  Returns default (all-zero)
    /// GrantId on malformed input.
    [[nodiscard]] static GrantId from_string(std::string_view sv);

    /// Compare for equality.
    bool operator==(const GrantId& o) const {
        return source_id == o.source_id &&
               play_session_id == o.play_session_id &&
               sequence == o.sequence;
    }
    bool operator!=(const GrantId& o) const { return !(*this == o); }
};

// ===========================================================================
// RewardGrant — A single atomic unit of reward.
// ===========================================================================

struct RewardGrant {
    GrantId id {};                        ///< Stable identity for idempotency
    PickupType type {PickupType::Count};  ///< What kind of reward
    float quantity {0.0F};                ///< Amount (ammo count, health, XP, etc.)
    ae::u32 item_def_id {0};              ///< Item definition ID (for WeaponDrop/ArmorDrop/PerkDrop)

    /// Returns true if the grant has a valid type and positive quantity.
    [[nodiscard]] bool is_valid() const;
};

// ===========================================================================
// Inventory entry — a single item instance in the player's inventory
// ===========================================================================

/// Limits for inventory sizing.
struct InventoryLimits {
    static constexpr ae::u32 kMaxWeaponSlots = 10;      ///< Max unique weapon items
    static constexpr ae::u32 kMaxArmorSlots = 5;        ///< One per armor slot
    static constexpr ae::u32 kMaxPerks = 20;             ///< Passive perks/mods
    static constexpr ae::u32 kMaxProcessedGrantIds = 1024; ///< Idempotency ring buffer size

    /// Max number of inventory entries for a given item type.
    static ae::u32 max_for_type(PickupType type);
};

struct InventoryEntry {
    ae::u32 item_definition_id {0};  ///< Item definition (from ItemRegistry)
    ae::u32 stack_count {1};         ///< Stack count (for consumables)
    bool is_equipped {false};        ///< Currently equipped in loadout
    ae::u64 instance_id {0};         ///< Unique instance identifier

    bool operator==(const InventoryEntry& o) const {
        return instance_id == o.instance_id;
    }
    bool operator!=(const InventoryEntry& o) const { return !(*this == o); }
};

// ===========================================================================
// Inventory — manages the player's collection of items
// ===========================================================================

class Inventory {
public:
    Inventory() = default;

    // --- Weapon inventory ---
    [[nodiscard]] const std::vector<InventoryEntry>& weapons() const { return weapons_; }
    [[nodiscard]] std::vector<InventoryEntry>& weapons() { return weapons_; }
    [[nodiscard]] bool has_weapon(ae::u32 definition_id) const;
    [[nodiscard]] bool weapons_full() const;
    bool add_weapon(const InventoryEntry& entry);
    bool remove_weapon(ae::u64 instance_id);

    // --- Armor inventory ---
    [[nodiscard]] const std::vector<InventoryEntry>& armor() const { return armor_; }
    [[nodiscard]] std::vector<InventoryEntry>& armor() { return armor_; }
    [[nodiscard]] bool has_armor(ae::u32 definition_id) const;
    [[nodiscard]] bool armor_full() const;
    bool add_armor(const InventoryEntry& entry);
    bool remove_armor(ae::u64 instance_id);

    // --- Perk inventory ---
    [[nodiscard]] const std::vector<InventoryEntry>& perks() const { return perks_; }
    [[nodiscard]] std::vector<InventoryEntry>& perks() { return perks_; }
    [[nodiscard]] bool has_perk(ae::u32 definition_id) const;
    [[nodiscard]] bool perks_full() const;
    bool add_perk(const InventoryEntry& entry);
    bool remove_perk(ae::u64 instance_id);

    // --- Per-resource caps ---
    [[nodiscard]] float current_health() const { return current_health_; }
    void set_current_health(float v) { current_health_ = v; }
    [[nodiscard]] float max_health() const { return max_health_; }
    void set_max_health(float v) { max_health_ = v; }

    [[nodiscard]] float current_shield() const { return current_shield_; }
    void set_current_shield(float v) { current_shield_ = v; }
    [[nodiscard]] float max_shield() const { return max_shield_; }
    void set_max_shield(float v) { max_shield_ = v; }

    [[nodiscard]] int primary_ammo() const { return ammo_primary_; }
    void set_primary_ammo(int v) { ammo_primary_ = std::max(0, v); }
    [[nodiscard]] int max_primary_ammo() const { return max_ammo_primary_; }
    void set_max_primary_ammo(int v) { max_ammo_primary_ = v; }

    [[nodiscard]] int secondary_ammo() const { return ammo_secondary_; }
    void set_secondary_ammo(int v) { ammo_secondary_ = std::max(0, v); }
    [[nodiscard]] int max_secondary_ammo() const { return max_ammo_secondary_; }
    void set_max_secondary_ammo(int v) { max_ammo_secondary_ = v; }

    [[nodiscard]] int heavy_ammo() const { return ammo_heavy_; }
    void set_heavy_ammo(int v) { ammo_heavy_ = std::max(0, v); }
    [[nodiscard]] int max_heavy_ammo() const { return max_ammo_heavy_; }
    void set_max_heavy_ammo(int v) { max_ammo_heavy_ = v; }

    [[nodiscard]] int special_ammo() const { return ammo_special_; }
    void set_special_ammo(int v) { ammo_special_ = std::max(0, v); }
    [[nodiscard]] int max_special_ammo() const { return max_ammo_special_; }
    void set_max_special_ammo(int v) { max_ammo_special_ = v; }

    [[nodiscard]] int grenade_count() const { return grenade_count_; }
    void set_grenade_count(int v) { grenade_count_ = std::max(0, v); }
    static constexpr int kMaxGrenades = 2;

    [[nodiscard]] float ultimate_charge() const { return ultimate_charge_; }
    void set_ultimate_charge(float v) { ultimate_charge_ = std::clamp(v, 0.0F, 1.0F); }

    [[nodiscard]] ae::u64 xp() const { return xp_; }
    void set_xp(ae::u64 v) { xp_ = v; }
    [[nodiscard]] ae::u64 currency() const { return currency_; }
    void set_currency(ae::u64 v) { currency_ = v; }

    // --- Clear everything ---
    void reset();

private:
    // Item collections
    std::vector<InventoryEntry> weapons_;
    std::vector<InventoryEntry> armor_;
    std::vector<InventoryEntry> perks_;

    // Health & Shield
    float current_health_ {100.0F};
    float max_health_ {100.0F};
    float current_shield_ {50.0F};
    float max_shield_ {100.0F};

    // Ammo pools
    int ammo_primary_ {0};
    int max_ammo_primary_ {300};
    int ammo_secondary_ {0};
    int max_ammo_secondary_ {200};
    int ammo_heavy_ {0};
    int max_ammo_heavy_ {50};
    int ammo_special_ {0};
    int max_ammo_special_ {100};

    // Ability charges
    int grenade_count_ {2};
    float ultimate_charge_ {0.0F};

    // Progression resources
    ae::u64 xp_ {0};
    ae::u64 currency_ {0};
};

// ===========================================================================
// PickupProcessor — Deterministic rules for granting rewards
// ===========================================================================

/// Processes reward grants against the player's inventory/state, enforcing
/// idempotency, capacity limits, and game rules.
///
/// This is the sole entry point for applying rewards.  External code calls
/// `process_grant()` and receives a GrantResult.  The processor never
/// silently discards or duplicates rewards.
class PickupProcessor {
public:
    PickupProcessor() = default;

    /// Process a single reward grant against the given inventory.
    /// Returns the result and mutates the inventory only on success.
    ///
    /// Idempotency: if `grant.id` was already processed, returns Duplicate.
    /// Capacity: if the target resource is at cap, returns AtCap.
    /// Inventory: if there is no room for an item, returns InventoryFull.
    [[nodiscard]] GrantResult process_grant(Inventory& inventory, const RewardGrant& grant);

    /// Returns true if this processor has seen the given grant ID before.
    [[nodiscard]] bool has_processed(const GrantId& id) const;

    /// Forget all processed grant IDs (e.g. on new match).
    void reset_idempotency();

    /// Forget processed grants older than the given session ID.
    /// Used when a new play session begins within the same match runtime.
    void prune_grants_before_session(ae::u32 session_id);

    /// Return a const reference to the processed grants set (for snapshot).
    [[nodiscard]] const std::vector<GrantId>& processed_grants() const {
        return processed_grants_;
    }

private:
    // Ring buffer of processed grant IDs (for idempotency).
    // Implemented as a flat vector for simplicity; we linear-scan on insert.
    // This is bounded by InventoryLimits::kMaxProcessedGrantIds.
    std::vector<GrantId> processed_grants_;

    // Internal helpers for each pickup type.
    GrantResult process_ammo(PickupType type, float quantity, Inventory& inv);
    GrantResult process_health(float quantity, Inventory& inv);
    GrantResult process_shield(float quantity, Inventory& inv);
    GrantResult process_weapon_drop(const RewardGrant& grant, Inventory& inv);
    GrantResult process_armor_drop(const RewardGrant& grant, Inventory& inv);
    GrantResult process_perk_drop(const RewardGrant& grant, Inventory& inv);
    GrantResult process_grenade(float quantity, Inventory& inv);
    GrantResult process_ultimate(float quantity, Inventory& inv);
    GrantResult process_xp(float quantity, Inventory& inv);
    GrantResult process_currency(float quantity, Inventory& inv);
};

// ===========================================================================
// ProgressionSnapshot — Versioned, serializable game-owned state
// ===========================================================================

/// A versioned snapshot of the player's progression and inventory state.
///
/// This is the canonical data model for Flashback's save/load system.
/// Wish storage adapters read/write this snapshot; Flashback owns its
/// schema, default construction, migration, and corruption repair.
///
/// ## Versioning
/// The `version` field determines the on-disk schema.  `migrate()` upgrades
/// older snapshots in place.  Unknown versions are treated as corrupt.
///
/// ## Guarantees
/// A default-constructed snapshot is valid and equivalent to a new player.
/// After `repair()`, all invariants hold (no dangling pointers, no
/// negative counts, no version mismatches).
struct ProgressionSnapshot {
    /// The current snapshot version.  Increment on every schema change.
    static constexpr ae::u32 kCurrentVersion = 1;
    /// The oldest version we can migrate from.
    static constexpr ae::u32 kMinSupportedVersion = 1;

    ae::u32 version {kCurrentVersion};  ///< Schema version for migration.

    // --- Timestamps ---
    ae::u64 total_play_time_seconds {0};  ///< Cumulative play time.
    ae::u32 last_session_id {0};          ///< The most recent play session.

    // --- Progression resources ---
    ae::u64 xp {0};
    ae::u64 currency {0};

    // --- Inventory ---
    std::vector<InventoryEntry> weapons;
    std::vector<InventoryEntry> armor;
    std::vector<InventoryEntry> perks;

    // --- Per-resource caps (persisted so loadout is consistent) ---
    float max_health {100.0F};
    float max_shield {100.0F};
    int max_primary_ammo {300};
    int max_secondary_ammo {200};
    int max_heavy_ammo {50};
    int max_special_ammo {100};

    // --- Idempotency ---
    std::vector<GrantId> processed_grants;

    // ------------------------------------------------------------------
    // Lifecycle & validation
    // ------------------------------------------------------------------

    /// Create a default (new-player) snapshot.
    [[nodiscard]] static ProgressionSnapshot create_default();

    /// Migrate an older-version snapshot to the current version.
    /// Returns the migrated snapshot.  Input is not modified.
    /// If the version is kMinSupportedVersion or higher and not current,
    /// migration steps are applied sequentially.  Unknown versions produce
    /// a default snapshot.
    [[nodiscard]] static ProgressionSnapshot migrate(ProgressionSnapshot old);

    /// Validate the snapshot's invariants.
    /// Returns false if any invariant is violated (negative values, missing
    /// version, dangling references, overflow).
    [[nodiscard]] bool validate() const;

    /// Attempt to repair a corrupted snapshot.
    /// Clamps negative values to zero, resets unknown versions to default,
    /// truncates over-capacity collections, and removes dangling refs.
    /// Returns a guaranteed-valid (but potentially data-losing) snapshot.
    [[nodiscard]] static ProgressionSnapshot repair(ProgressionSnapshot corrupted);

    // ------------------------------------------------------------------
    // Serialization (testable; actual persistence goes through Wish storage)
    // ------------------------------------------------------------------

    /// Serialize to a byte buffer.
    /// Format: [version:4][play_time:8][session_id:4][xp:8][currency:8]
    ///         [weapons_len:4][weapons_data...][armor_len:4][armor_data...]
    ///         [perks_len:4][perks_data...][health:4][shield:4]
    ///         [ammo caps:4 each][grants_len:4][grants_data...]
    /// All multi-byte values are little-endian.
    [[nodiscard]] std::vector<std::uint8_t> serialize() const;

    /// Deserialize from a byte buffer.  Handles version migration internally.
    /// Returns a valid snapshot (via repair()) on any error.
    [[nodiscard]] static ProgressionSnapshot deserialize(const std::uint8_t* data, std::size_t size);
};

// ===========================================================================
// Free utility functions
// ===========================================================================

/// Format a GrantResult as a human-readable string.
[[nodiscard]] const char* grant_result_string(GrantResult r);

/// Format a PickupType as a human-readable string.
[[nodiscard]] const char* pickup_type_string(PickupType t);

} // namespace ahamkara::game
