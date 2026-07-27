#pragma once

#include "ahamkara/game/item_registry.h"
#include "ae/core/types.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <unordered_map>
#include <vector>

namespace ahamkara::game {

// ---------------------------------------------------------------------------
//  ItemInstance — a runtime owned item with its perk/mod rolls
// ---------------------------------------------------------------------------

/// A concrete instance of an item owned by a player.
///
/// Each `ItemInstance` corresponds to a specific piece of gear (weapon, armour,
/// etc.) that a player possesses.  The `definition_id` links back to the
/// static `ItemDefinition` in `ItemRegistry`.  Perks and mods are attached at
/// the instance level so that two copies of the same base item can have
/// different rolls.
struct ItemInstance {
    ae::u32 instance_id {0};
    ae::u32 definition_id {0};
    std::vector<ae::u32> perk_ids;  ///< Perk IDs rolled/attached to this item.
    std::vector<ae::u32> mod_ids;   ///< Mod IDs socketed into this item.
};

// ---------------------------------------------------------------------------
//  ProgressionState — player level, XP, and levelling hooks
// ---------------------------------------------------------------------------

/// Tracks the player's level and experience points.
///
/// The XP curve uses a 1.15× multiplier per level so that later levels
/// require progressively more XP.
struct ProgressionState {
    ae::u32 level {1};
    ae::u64 xp {0};
    ae::u64 xp_to_next_level {1000};

    /// Award XP.  Returns `true` if the player gained at least one level
    /// (caller may want to fire a level-up event or reward).
    bool add_xp(ae::u64 amount);

    /// Force-set the level (clamped >= 1).  Recalculates the XP curve and
    /// resets current XP to zero.
    void set_level(ae::u32 new_level);
};

// ---------------------------------------------------------------------------
//  CurrencyState — player wallet
// ---------------------------------------------------------------------------

/// Simple wallet holding one or more currency types.
///
/// Currently only tracks Glimmer (the primary soft currency).  Additional
/// currencies (Bright Dust, upgrade materials, etc.) can be added as new
/// fields when the game needs them.
struct CurrencyState {
    ae::u64 glimmer {0};

    /// Spend Glimmer.  Returns `false` if the balance is insufficient
    /// (the wallet is unchanged on failure).
    bool spend_glimmer(ae::u64 amount);

    /// Add Glimmer to the wallet.
    void earn_glimmer(ae::u64 amount);
};

// ---------------------------------------------------------------------------
//  Inventory — collection of owned item instances
// ---------------------------------------------------------------------------

/// Manages the set of items owned by a player.
///
/// Owns the mapping from `instance_id` → `ItemInstance` and the mapping from
/// `ItemSlot` → currently-equipped instance.  All mutations go through this
/// class so that invariants are enforced centrally:
///
///   • No duplicate instance IDs.
///   • Only existing items can be equipped.
///   • Only valid slots can be equipped into.
///   • Equipping an item into an occupied slot displaces the previous occupant
///     (it remains in the inventory — it is just unequipped).
///
/// The class does **not** depend on any rendering, networking, or AI code.
class Inventory {
  public:
    Inventory();

    // -- Item management -------------------------------------------------

    /// Add a new item instance derived from the given `definition_id`.
    /// Returns the newly-assigned instance ID, or 0 if the inventory is full.
    ae::u32 add_item(ae::u32 definition_id);

    /// Remove an item by instance ID.  Unequips it first if it is currently
    /// equipped.  Returns `true` on success.
    bool remove_item(ae::u32 instance_id);

    /// Remove all items and clear all equipment slots.
    void clear();

    // -- Query -----------------------------------------------------------

    /// Look up an item by instance ID.  Returns `nullptr` if not found.
    [[nodiscard]] const ItemInstance* get_item(ae::u32 instance_id) const;

    /// Number of items currently in the inventory.
    [[nodiscard]] std::size_t item_count() const { return items_.size(); }

    /// Whether the inventory has reached its capacity limit.
    [[nodiscard]] bool is_full() const { return items_.size() >= max_items_; }

    /// Set / get the maximum number of items the inventory can hold.
    void set_max_items(std::size_t max) { max_items_ = max; }
    [[nodiscard]] std::size_t max_items() const { return max_items_; }

    /// Return all items whose definition matches the given `ItemType`.
    /// Performs a lookup through `ItemRegistry::instance()`.
    [[nodiscard]] std::vector<const ItemInstance*> get_items_by_type(ItemType type) const;

    /// Convenience: return all items that belong to a particular `ItemSlot`
    /// category (weapon primary, armour helmet, etc.).
    [[nodiscard]] std::vector<const ItemInstance*> get_items_by_slot(ItemSlot slot) const;

    // -- Equipment -------------------------------------------------------

    /// Equip an item into the given slot.  If the slot already held an item
    /// that item is unequipped (but stays in the inventory).  Returns `false`
    /// if the instance does not exist or the slot is invalid.
    bool equip_item(ae::u32 instance_id, ItemSlot slot);

    /// Unequip whatever is in `slot`.  The item remains in the inventory.
    bool unequip_item(ItemSlot slot);

    /// Return the item currently equipped in `slot`, or `nullptr` if empty.
    [[nodiscard]] const ItemInstance* get_equipped(ItemSlot slot) const;

    /// Return the instance ID equipped in `slot`, or 0 if empty.
    [[nodiscard]] ae::u32 get_equipped_id(ItemSlot slot) const;

    // -- Perks & Mods ----------------------------------------------------

    /// Attach a perk to an item.  No-op if the perk is already attached.
    bool attach_perk(ae::u32 instance_id, ae::u32 perk_id);

    /// Remove a perk from an item.  No-op if the perk is not present.
    bool remove_perk(ae::u32 instance_id, ae::u32 perk_id);

    /// Attach a mod to an item.  No-op if the mod is already attached.
    bool attach_mod(ae::u32 instance_id, ae::u32 mod_id);

    /// Remove a mod from an item.  No-op if the mod is not present.
    bool remove_mod(ae::u32 instance_id, ae::u32 mod_id);

    // -- Iteration (read-only) -------------------------------------------

    using const_iterator = std::unordered_map<ae::u32, ItemInstance>::const_iterator;

    [[nodiscard]] const_iterator begin() const { return items_.begin(); }
    [[nodiscard]] const_iterator end() const { return items_.end(); }

    // -- Constants -------------------------------------------------------

    /// Sentinel for "no instance" / empty slot.
    static constexpr ae::u32 kInvalidInstanceId = 0;

  private:
    std::unordered_map<ae::u32, ItemInstance> items_;
    ae::u32 equipped_slots_[static_cast<int>(ItemSlot::None)];  // indexed by ItemSlot
    ae::u32 next_instance_id_ {1};
    std::size_t max_items_ {50};

    [[nodiscard]] static bool is_valid_slot(ItemSlot slot) {
        const int idx = static_cast<int>(slot);
        return idx >= 0 && idx < static_cast<int>(ItemSlot::None);
    }
};

// ===========================================================================
//  Inline implementations
// ===========================================================================

inline bool ProgressionState::add_xp(ae::u64 amount) {
    xp += amount;
    bool leveled = false;
    while (xp >= xp_to_next_level) {
        xp -= xp_to_next_level;
        ++level;
        // Curve: each level costs ~15 % more than the previous.
        xp_to_next_level = static_cast<ae::u64>(
            1000.0 * std::pow(1.15, static_cast<double>(level - 1)));
        leveled = true;
    }
    return leveled;
}

inline void ProgressionState::set_level(ae::u32 new_level) {
    level = (new_level >= 1) ? new_level : ae::u32(1);
    xp = 0;
    xp_to_next_level = static_cast<ae::u64>(
        1000.0 * std::pow(1.15, static_cast<double>(level - 1)));
}

inline bool CurrencyState::spend_glimmer(ae::u64 amount) {
    if (glimmer < amount) return false;
    glimmer -= amount;
    return true;
}

inline void CurrencyState::earn_glimmer(ae::u64 amount) {
    glimmer += amount;
}

}  // namespace ahamkara::game
