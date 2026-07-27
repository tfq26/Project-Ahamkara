#include "ahamkara/game/inventory.h"

#include <algorithm>
#include <cstring>

namespace ahamkara::game {

// ===========================================================================
//  Inventory
// ===========================================================================

Inventory::Inventory() {
    std::memset(equipped_slots_, 0, sizeof(equipped_slots_));
}

// -- Item management ---------------------------------------------------------

ae::u32 Inventory::add_item(ae::u32 definition_id) {
    if (is_full()) {
        return 0;
    }
    const ae::u32 id = next_instance_id_++;
    items_.emplace(id, ItemInstance{id, definition_id, {}, {}});
    return id;
}

bool Inventory::remove_item(ae::u32 instance_id) {
    const auto it = items_.find(instance_id);
    if (it == items_.end()) {
        return false;
    }
    // Unequip first if this item is currently equipped in any slot.
    for (int i = 0; i < static_cast<int>(ItemSlot::None); ++i) {
        if (equipped_slots_[i] == instance_id) {
            equipped_slots_[i] = kInvalidInstanceId;
            break;
        }
    }
    items_.erase(it);
    return true;
}

void Inventory::clear() {
    items_.clear();
    std::memset(equipped_slots_, 0, sizeof(equipped_slots_));
    next_instance_id_ = 1;
}

// -- Query -------------------------------------------------------------------

const ItemInstance* Inventory::get_item(ae::u32 instance_id) const {
    const auto it = items_.find(instance_id);
    return (it != items_.end()) ? &it->second : nullptr;
}

std::vector<const ItemInstance*> Inventory::get_items_by_type(ItemType type) const {
    std::vector<const ItemInstance*> result;
    const auto& registry = ItemRegistry::instance();
    for (const auto& [id, inst] : items_) {
        (void)id;
        const ItemDefinition* def = registry.get_item(inst.definition_id);
        if (def != nullptr && def->type == type) {
            result.push_back(&inst);
        }
    }
    return result;
}

std::vector<const ItemInstance*> Inventory::get_items_by_slot(ItemSlot slot) const {
    std::vector<const ItemInstance*> result;
    const auto& registry = ItemRegistry::instance();
    for (const auto& [id, inst] : items_) {
        (void)id;
        const ItemDefinition* def = registry.get_item(inst.definition_id);
        if (def != nullptr && def->slot == slot) {
            result.push_back(&inst);
        }
    }
    return result;
}

// -- Equipment ---------------------------------------------------------------

bool Inventory::equip_item(ae::u32 instance_id, ItemSlot slot) {
    if (!is_valid_slot(slot)) {
        return false;
    }
    if (items_.find(instance_id) == items_.end()) {
        return false;
    }
    equipped_slots_[static_cast<int>(slot)] = instance_id;
    return true;
}

bool Inventory::unequip_item(ItemSlot slot) {
    if (!is_valid_slot(slot)) {
        return false;
    }
    equipped_slots_[static_cast<int>(slot)] = kInvalidInstanceId;
    return true;
}

const ItemInstance* Inventory::get_equipped(ItemSlot slot) const {
    if (!is_valid_slot(slot)) {
        return nullptr;
    }
    const ae::u32 id = equipped_slots_[static_cast<int>(slot)];
    return (id != kInvalidInstanceId) ? get_item(id) : nullptr;
}

ae::u32 Inventory::get_equipped_id(ItemSlot slot) const {
    if (!is_valid_slot(slot)) {
        return kInvalidInstanceId;
    }
    return equipped_slots_[static_cast<int>(slot)];
}

// -- Perks & Mods ------------------------------------------------------------

bool Inventory::attach_perk(ae::u32 instance_id, ae::u32 perk_id) {
    const auto it = items_.find(instance_id);
    if (it == items_.end()) {
        return false;
    }
    std::vector<ae::u32>& perks = it->second.perk_ids;
    if (std::find(perks.begin(), perks.end(), perk_id) != perks.end()) {
        return false;  // already attached
    }
    perks.push_back(perk_id);
    return true;
}

bool Inventory::remove_perk(ae::u32 instance_id, ae::u32 perk_id) {
    const auto it = items_.find(instance_id);
    if (it == items_.end()) {
        return false;
    }
    std::vector<ae::u32>& perks = it->second.perk_ids;
    const auto pos = std::find(perks.begin(), perks.end(), perk_id);
    if (pos == perks.end()) {
        return false;
    }
    perks.erase(pos);
    return true;
}

bool Inventory::attach_mod(ae::u32 instance_id, ae::u32 mod_id) {
    const auto it = items_.find(instance_id);
    if (it == items_.end()) {
        return false;
    }
    std::vector<ae::u32>& mods = it->second.mod_ids;
    if (std::find(mods.begin(), mods.end(), mod_id) != mods.end()) {
        return false;  // already attached
    }
    mods.push_back(mod_id);
    return true;
}

bool Inventory::remove_mod(ae::u32 instance_id, ae::u32 mod_id) {
    const auto it = items_.find(instance_id);
    if (it == items_.end()) {
        return false;
    }
    std::vector<ae::u32>& mods = it->second.mod_ids;
    const auto pos = std::find(mods.begin(), mods.end(), mod_id);
    if (pos == mods.end()) {
        return false;
    }
    mods.erase(pos);
    return true;
}

}  // namespace ahamkara::game
