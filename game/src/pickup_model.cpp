#include "ahamkara/game/pickup_model.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <sstream>
#include <string>

namespace ahamkara::game {

// ===========================================================================
// GrantId
// ===========================================================================

std::string GrantId::to_string() const {
    // Format: "XXXXXXXX-XXXXXXXX-XXXXXXXX"
    // (source_id as 16 hex, session_id as 8 hex, sequence as 8 hex)
    char buf[48];
    int n = std::snprintf(buf, sizeof(buf), "%016llx-%08x-%08x",
                          static_cast<unsigned long long>(source_id),
                          play_session_id, sequence);
    return std::string(buf, static_cast<std::size_t>(n));
}

GrantId GrantId::from_string(std::string_view sv) {
    GrantId id{};
    // Format: 16 hex + '-' + 8 hex + '-' + 8 hex = 34 chars
    if (sv.size() < 34) return id;

    // Quick validate format
    if (sv[16] != '-' || sv[25] != '-') return id;

    auto parse_hex16 = [&](std::size_t start) -> ae::u64 {
        ae::u64 val = 0;
        for (std::size_t i = 0; i < 16 && start + i < sv.size(); ++i) {
            char c = sv[start + i];
            val <<= 4;
            if (c >= '0' && c <= '9')       val |= static_cast<ae::u64>(c - '0');
            else if (c >= 'a' && c <= 'f')  val |= static_cast<ae::u64>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F')  val |= static_cast<ae::u64>(c - 'A' + 10);
            else return 0;
        }
        return val;
    };

    auto parse_hex8 = [&](std::size_t start) -> ae::u32 {
        ae::u32 val = 0;
        for (std::size_t i = 0; i < 8 && start + i < sv.size(); ++i) {
            char c = sv[start + i];
            val <<= 4;
            if (c >= '0' && c <= '9')       val |= static_cast<ae::u32>(c - '0');
            else if (c >= 'a' && c <= 'f')  val |= static_cast<ae::u32>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F')  val |= static_cast<ae::u32>(c - 'A' + 10);
            else return 0;
        }
        return val;
    };

    id.source_id = parse_hex16(0);
    id.play_session_id = parse_hex8(17);
    id.sequence = parse_hex8(26);
    return id;
}

// ===========================================================================
// RewardGrant
// ===========================================================================

bool RewardGrant::is_valid() const {
    return type < PickupType::Count &&
           quantity > 0.0F &&
           quantity < 1e9F;  // sanity cap
}

// ===========================================================================
// InventoryLimits
// ===========================================================================

ae::u32 InventoryLimits::max_for_type(PickupType type) {
    switch (type) {
        case PickupType::WeaponDrop:  return kMaxWeaponSlots;
        case PickupType::ArmorDrop:   return kMaxArmorSlots;
        case PickupType::PerkDrop:    return kMaxPerks;
        default:                      return 0; // no item-level cap for resources
    }
}

// ===========================================================================
// Inventory
// ===========================================================================

bool Inventory::has_weapon(ae::u32 definition_id) const {
    return std::any_of(weapons_.begin(), weapons_.end(),
        [definition_id](const InventoryEntry& e) {
            return e.item_definition_id == definition_id;
        });
}

bool Inventory::weapons_full() const {
    return weapons_.size() >= InventoryLimits::kMaxWeaponSlots;
}

bool Inventory::add_weapon(const InventoryEntry& entry) {
    if (weapons_.size() >= InventoryLimits::kMaxWeaponSlots) return false;
    weapons_.push_back(entry);
    return true;
}

bool Inventory::remove_weapon(ae::u64 instance_id) {
    auto it = std::remove_if(weapons_.begin(), weapons_.end(),
        [instance_id](const InventoryEntry& e) { return e.instance_id == instance_id; });
    if (it == weapons_.end()) return false;
    weapons_.erase(it, weapons_.end());
    return true;
}

bool Inventory::has_armor(ae::u32 definition_id) const {
    return std::any_of(armor_.begin(), armor_.end(),
        [definition_id](const InventoryEntry& e) {
            return e.item_definition_id == definition_id;
        });
}

bool Inventory::armor_full() const {
    return armor_.size() >= InventoryLimits::kMaxArmorSlots;
}

bool Inventory::add_armor(const InventoryEntry& entry) {
    if (armor_.size() >= InventoryLimits::kMaxArmorSlots) return false;
    armor_.push_back(entry);
    return true;
}

bool Inventory::remove_armor(ae::u64 instance_id) {
    auto it = std::remove_if(armor_.begin(), armor_.end(),
        [instance_id](const InventoryEntry& e) { return e.instance_id == instance_id; });
    if (it == armor_.end()) return false;
    armor_.erase(it, armor_.end());
    return true;
}

bool Inventory::has_perk(ae::u32 definition_id) const {
    return std::any_of(perks_.begin(), perks_.end(),
        [definition_id](const InventoryEntry& e) {
            return e.item_definition_id == definition_id;
        });
}

bool Inventory::perks_full() const {
    return perks_.size() >= InventoryLimits::kMaxPerks;
}

bool Inventory::add_perk(const InventoryEntry& entry) {
    if (perks_.size() >= InventoryLimits::kMaxPerks) return false;
    perks_.push_back(entry);
    return true;
}

bool Inventory::remove_perk(ae::u64 instance_id) {
    auto it = std::remove_if(perks_.begin(), perks_.end(),
        [instance_id](const InventoryEntry& e) { return e.instance_id == instance_id; });
    if (it == perks_.end()) return false;
    perks_.erase(it, perks_.end());
    return true;
}

void Inventory::reset() {
    weapons_.clear();
    armor_.clear();
    perks_.clear();
    current_health_ = 100.0F;
    max_health_ = 100.0F;
    current_shield_ = 50.0F;
    max_shield_ = 100.0F;
    ammo_primary_ = 0;
    max_ammo_primary_ = 300;
    ammo_secondary_ = 0;
    max_ammo_secondary_ = 200;
    ammo_heavy_ = 0;
    max_ammo_heavy_ = 50;
    ammo_special_ = 0;
    max_ammo_special_ = 100;
    grenade_count_ = 2;
    ultimate_charge_ = 0.0F;
    xp_ = 0;
    currency_ = 0;
}

// ===========================================================================
// PickupProcessor
// ===========================================================================

GrantResult PickupProcessor::process_grant(Inventory& inventory, const RewardGrant& grant) {
    if (!grant.is_valid()) return GrantResult::Invalid;

    // Idempotency check
    if (has_processed(grant.id)) return GrantResult::Duplicate;

    GrantResult result = GrantResult::Invalid;

    switch (grant.type) {
        case PickupType::PrimaryAmmo:
        case PickupType::SecondaryAmmo:
        case PickupType::HeavyAmmo:
        case PickupType::SpecialAmmo:
            result = process_ammo(grant.type, grant.quantity, inventory);
            break;

        case PickupType::HealthPack:
        case PickupType::LargeHealth:
            result = process_health(grant.quantity, inventory);
            break;

        case PickupType::ShieldCharge:
            result = process_shield(grant.quantity, inventory);
            break;

        case PickupType::WeaponDrop:
            result = process_weapon_drop(grant, inventory);
            break;

        case PickupType::ArmorDrop:
            result = process_armor_drop(grant, inventory);
            break;

        case PickupType::PerkDrop:
            result = process_perk_drop(grant, inventory);
            break;

        case PickupType::GrenadeCharge:
            result = process_grenade(grant.quantity, inventory);
            break;

        case PickupType::UltimateEnergy:
            result = process_ultimate(grant.quantity, inventory);
            break;

        case PickupType::XPOrb:
            result = process_xp(grant.quantity, inventory);
            break;

        case PickupType::Currency:
            result = process_currency(grant.quantity, inventory);
            break;

        default:
            return GrantResult::Invalid;
    }

    // Only record idempotency on successful grants.
    // AtCap is recorded too — we don't want repeated attempts for the same
    // health pack when the player is already full.
    if (result == GrantResult::Granted || result == GrantResult::AtCap || result == GrantResult::Duplicate) {
        if (processed_grants_.size() < InventoryLimits::kMaxProcessedGrantIds) {
            processed_grants_.push_back(grant.id);
        }
    }

    return result;
}

bool PickupProcessor::has_processed(const GrantId& id) const {
    return std::find(processed_grants_.begin(), processed_grants_.end(), id) != processed_grants_.end();
}

void PickupProcessor::reset_idempotency() {
    processed_grants_.clear();
}

void PickupProcessor::prune_grants_before_session(ae::u32 session_id) {
    auto it = std::remove_if(processed_grants_.begin(), processed_grants_.end(),
        [session_id](const GrantId& g) { return g.play_session_id < session_id; });
    processed_grants_.erase(it, processed_grants_.end());
}

GrantResult PickupProcessor::process_ammo(PickupType type, float quantity, Inventory& inv) {
    int qty = static_cast<int>(quantity);
    if (qty <= 0) return GrantResult::Invalid;

    int cur = 0;
    int cap = 0;
    switch (type) {
        case PickupType::PrimaryAmmo:  cur = inv.primary_ammo();  cap = inv.max_primary_ammo();  break;
        case PickupType::SecondaryAmmo: cur = inv.secondary_ammo(); cap = inv.max_secondary_ammo(); break;
        case PickupType::HeavyAmmo:    cur = inv.heavy_ammo();    cap = inv.max_heavy_ammo();    break;
        case PickupType::SpecialAmmo:  cur = inv.special_ammo();  cap = inv.max_special_ammo();  break;
        default: return GrantResult::Invalid;
    }

    if (cur >= cap) return GrantResult::AtCap;

    int new_val = std::min(cur + qty, cap);
    switch (type) {
        case PickupType::PrimaryAmmo:  inv.set_primary_ammo(new_val);  break;
        case PickupType::SecondaryAmmo: inv.set_secondary_ammo(new_val); break;
        case PickupType::HeavyAmmo:    inv.set_heavy_ammo(new_val);    break;
        case PickupType::SpecialAmmo:  inv.set_special_ammo(new_val);  break;
        default: break;
    }

    return GrantResult::Granted;
}

GrantResult PickupProcessor::process_health(float quantity, Inventory& inv) {
    if (quantity <= 0.0F) return GrantResult::Invalid;
    float cur = inv.current_health();
    float cap = inv.max_health();
    if (cur >= cap) return GrantResult::AtCap;
    inv.set_current_health(std::min(cur + quantity, cap));
    return GrantResult::Granted;
}

GrantResult PickupProcessor::process_shield(float quantity, Inventory& inv) {
    if (quantity <= 0.0F) return GrantResult::Invalid;
    float cur = inv.current_shield();
    float cap = inv.max_shield();
    if (cur >= cap) return GrantResult::AtCap;
    inv.set_current_shield(std::min(cur + quantity, cap));
    return GrantResult::Granted;
}

GrantResult PickupProcessor::process_weapon_drop(const RewardGrant& grant, Inventory& inv) {
    if (grant.item_def_id == 0) return GrantResult::Invalid;
    if (inv.has_weapon(grant.item_def_id)) return GrantResult::Duplicate;  // Already own this weapon

    InventoryEntry entry;
    entry.item_definition_id = grant.item_def_id;
    entry.stack_count = 1;
    entry.is_equipped = false;
    entry.instance_id = static_cast<ae::u64>(grant.id.source_id) ^
                        (static_cast<ae::u64>(grant.id.sequence) << 32);

    if (!inv.add_weapon(entry)) return GrantResult::InventoryFull;
    return GrantResult::Granted;
}

GrantResult PickupProcessor::process_armor_drop(const RewardGrant& grant, Inventory& inv) {
    if (grant.item_def_id == 0) return GrantResult::Invalid;
    if (inv.has_armor(grant.item_def_id)) return GrantResult::Duplicate;

    InventoryEntry entry;
    entry.item_definition_id = grant.item_def_id;
    entry.stack_count = 1;
    entry.is_equipped = false;
    entry.instance_id = static_cast<ae::u64>(grant.id.source_id) ^
                        (static_cast<ae::u64>(grant.id.sequence) << 32) ^ 0x5555555555555555ULL;

    if (!inv.add_armor(entry)) return GrantResult::InventoryFull;
    return GrantResult::Granted;
}

GrantResult PickupProcessor::process_perk_drop(const RewardGrant& grant, Inventory& inv) {
    if (grant.item_def_id == 0) return GrantResult::Invalid;
    if (inv.has_perk(grant.item_def_id)) return GrantResult::Duplicate;

    InventoryEntry entry;
    entry.item_definition_id = grant.item_def_id;
    entry.stack_count = 1;
    entry.is_equipped = false;
    entry.instance_id = static_cast<ae::u64>(grant.id.source_id) ^
                        (static_cast<ae::u64>(grant.id.sequence) << 32) ^ 0xAAAAAAAAAAAAAAAAULL;

    if (!inv.add_perk(entry)) return GrantResult::InventoryFull;
    return GrantResult::Granted;
}

GrantResult PickupProcessor::process_grenade(float quantity, Inventory& inv) {
    int qty = static_cast<int>(quantity);
    if (qty <= 0) return GrantResult::Invalid;
    int cur = inv.grenade_count();
    if (cur >= Inventory::kMaxGrenades) return GrantResult::AtCap;
    inv.set_grenade_count(std::min(cur + qty, Inventory::kMaxGrenades));
    return GrantResult::Granted;
}

GrantResult PickupProcessor::process_ultimate(float quantity, Inventory& inv) {
    if (quantity <= 0.0F) return GrantResult::Invalid;
    float cur = inv.ultimate_charge();
    if (cur >= 1.0F) return GrantResult::AtCap;
    inv.set_ultimate_charge(std::min(cur + quantity, 1.0F));
    return GrantResult::Granted;
}

GrantResult PickupProcessor::process_xp(float quantity, Inventory& inv) {
    ae::u64 qty = static_cast<ae::u64>(quantity);
    if (qty == 0) return GrantResult::Invalid;
    inv.set_xp(inv.xp() + qty);
    return GrantResult::Granted;
}

GrantResult PickupProcessor::process_currency(float quantity, Inventory& inv) {
    ae::u64 qty = static_cast<ae::u64>(quantity);
    if (qty == 0) return GrantResult::Invalid;
    // Use saturation arithmetic to avoid overflow
    ae::u64 cur = inv.currency();
    ae::u64 new_val = cur + qty;
    if (new_val < cur) new_val = std::numeric_limits<ae::u64>::max(); // saturate on overflow
    inv.set_currency(new_val);
    return GrantResult::Granted;
}

// ===========================================================================
// ProgressionSnapshot
// ===========================================================================

ProgressionSnapshot ProgressionSnapshot::create_default() {
    ProgressionSnapshot snap{};
    snap.version = kCurrentVersion;
    // All other fields already zero-initialized / at default values.
    return snap;
}

ProgressionSnapshot ProgressionSnapshot::migrate(ProgressionSnapshot old) {
    if (old.version == kCurrentVersion) return old;
    if (old.version < kMinSupportedVersion) return create_default();
    if (old.version > kCurrentVersion) return create_default();

    // Sequential migration: upgrade version by version.
    // Currently only v1 exists, so this is a no-op.
    // When v2 is added, add `if (old.version == 1) upgrade_1_to_2(old);`
    // then cascade.

    old.version = kCurrentVersion;
    return old;
}

bool ProgressionSnapshot::validate() const {
    if (version < kMinSupportedVersion || version > kCurrentVersion) return false;

    // Check for negative / overflow values
    if (xp == std::numeric_limits<ae::u64>::max() && xp != 0) return false; // saturated overflow marker — allow it
    if (currency == std::numeric_limits<ae::u64>::max() && currency != 0) return false;

    if (max_health <= 0.0F || max_health > 10000.0F) return false;
    if (max_shield <= 0.0F || max_shield > 10000.0F) return false;
    if (max_primary_ammo < 0 || max_primary_ammo > 10000) return false;
    if (max_secondary_ammo < 0 || max_secondary_ammo > 10000) return false;
    if (max_heavy_ammo < 0 || max_heavy_ammo > 10000) return false;
    if (max_special_ammo < 0 || max_special_ammo > 10000) return false;

    // Check collection sizes
    if (weapons.size() > InventoryLimits::kMaxWeaponSlots) return false;
    if (armor.size() > InventoryLimits::kMaxArmorSlots) return false;
    if (perks.size() > InventoryLimits::kMaxPerks) return false;
    if (processed_grants.size() > InventoryLimits::kMaxProcessedGrantIds) return false;

    // Check inventory entries for validity
    for (const auto& w : weapons) {
        if (w.item_definition_id == 0) return false;
    }
    for (const auto& a : armor) {
        if (a.item_definition_id == 0) return false;
    }
    for (const auto& p : perks) {
        if (p.item_definition_id == 0) return false;
    }

    return true;
}

ProgressionSnapshot ProgressionSnapshot::repair(ProgressionSnapshot corrupted) {
    // Fix version
    if (corrupted.version < kMinSupportedVersion || corrupted.version > kCurrentVersion) {
        corrupted = create_default();
        return corrupted;
    }

    // Clamp values
    if (corrupted.max_health <= 0.0F) corrupted.max_health = 100.0F;
    if (corrupted.max_health > 10000.0F) corrupted.max_health = 10000.0F;
    if (corrupted.max_shield <= 0.0F) corrupted.max_shield = 100.0F;
    if (corrupted.max_shield > 10000.0F) corrupted.max_shield = 10000.0F;

    auto clamp_ammo = [](int& val, int max) {
        if (val < 0) val = 0;
        if (val > max) val = max;
    };
    clamp_ammo(corrupted.max_primary_ammo, 10000);
    clamp_ammo(corrupted.max_secondary_ammo, 10000);
    clamp_ammo(corrupted.max_heavy_ammo, 10000);
    clamp_ammo(corrupted.max_special_ammo, 10000);

    // Truncate oversize collections
    if (corrupted.weapons.size() > InventoryLimits::kMaxWeaponSlots)
        corrupted.weapons.resize(InventoryLimits::kMaxWeaponSlots);
    if (corrupted.armor.size() > InventoryLimits::kMaxArmorSlots)
        corrupted.armor.resize(InventoryLimits::kMaxArmorSlots);
    if (corrupted.perks.size() > InventoryLimits::kMaxPerks)
        corrupted.perks.resize(InventoryLimits::kMaxPerks);
    if (corrupted.processed_grants.size() > InventoryLimits::kMaxProcessedGrantIds)
        corrupted.processed_grants.resize(InventoryLimits::kMaxProcessedGrantIds);

    // Remove entries with zero definition IDs
    auto clean_entries = [](std::vector<InventoryEntry>& entries) {
        auto it = std::remove_if(entries.begin(), entries.end(),
            [](const InventoryEntry& e) { return e.item_definition_id == 0; });
        entries.erase(it, entries.end());
    };
    clean_entries(corrupted.weapons);
    clean_entries(corrupted.armor);
    clean_entries(corrupted.perks);

    corrupted.version = kCurrentVersion;
    return corrupted;
}

// ===========================================================================
// Serialization (simple binary format)
// ===========================================================================

namespace {

template <typename T>
void append_le(std::vector<std::uint8_t>& buf, T value) {
    auto* bytes = reinterpret_cast<const std::uint8_t*>(&value);
    for (std::size_t i = 0; i < sizeof(T); ++i) {
        buf.push_back(bytes[i]);
    }
}

template <typename T>
T read_le(const std::uint8_t* data, std::size_t& offset) {
    // Use memcpy to avoid UB from unaligned access
    T value;
    std::memcpy(&value, data + offset, sizeof(T));
    offset += sizeof(T);
    return value;
}

void serialize_string_vector(
    std::vector<std::uint8_t>& buf,
    const std::vector<InventoryEntry>& entries) {

    ae::u32 count = static_cast<ae::u32>(entries.size());
    append_le(buf, count);
    for (const auto& e : entries) {
        append_le(buf, e.item_definition_id);
        append_le(buf, e.stack_count);
        append_le(buf, static_cast<ae::u32>(e.is_equipped ? 1 : 0));
        append_le(buf, e.instance_id);
    }
}

void serialize_grant_vector(
    std::vector<std::uint8_t>& buf,
    const std::vector<GrantId>& grants) {

    ae::u32 count = static_cast<ae::u32>(grants.size());
    append_le(buf, count);
    for (const auto& g : grants) {
        append_le(buf, g.source_id);
        append_le(buf, g.play_session_id);
        append_le(buf, g.sequence);
    }
}

std::vector<InventoryEntry> deserialize_entry_vector(
    const std::uint8_t* data, std::size_t& offset, std::size_t total_size) {

    if (offset + 4 > total_size) return {};
    ae::u32 count = read_le<ae::u32>(data, offset);

    std::vector<InventoryEntry> entries;
    entries.reserve(count);
    for (ae::u32 i = 0; i < count; ++i) {
        if (offset + 20 > total_size) break; // 4+4+4+8 = 20 bytes per entry
        InventoryEntry e;
        e.item_definition_id = read_le<ae::u32>(data, offset);
        e.stack_count = read_le<ae::u32>(data, offset);
        e.is_equipped = read_le<ae::u32>(data, offset) != 0;
        e.instance_id = read_le<ae::u64>(data, offset);
        entries.push_back(e);
    }
    return entries;
}

std::vector<GrantId> deserialize_grant_vector(
    const std::uint8_t* data, std::size_t& offset, std::size_t total_size) {

    if (offset + 4 > total_size) return {};
    ae::u32 count = read_le<ae::u32>(data, offset);

    std::vector<GrantId> grants;
    grants.reserve(count);
    for (ae::u32 i = 0; i < count; ++i) {
        if (offset + 16 > total_size) break; // 8+4+4 = 16 bytes per grant
        GrantId g;
        g.source_id = read_le<ae::u64>(data, offset);
        g.play_session_id = read_le<ae::u32>(data, offset);
        g.sequence = read_le<ae::u32>(data, offset);
        grants.push_back(g);
    }
    return grants;
}

} // anonymous namespace

std::vector<std::uint8_t> ProgressionSnapshot::serialize() const {
    std::vector<std::uint8_t> buf;
    buf.reserve(256);

    // Version header
    append_le(buf, version);
    append_le(buf, total_play_time_seconds);
    append_le(buf, last_session_id);
    append_le(buf, xp);
    append_le(buf, currency);

    // Inventory collections
    serialize_string_vector(buf, weapons);
    serialize_string_vector(buf, armor);
    serialize_string_vector(buf, perks);

    // Resource caps
    append_le(buf, max_health);
    append_le(buf, max_shield);
    append_le(buf, max_primary_ammo);
    append_le(buf, max_secondary_ammo);
    append_le(buf, max_heavy_ammo);
    append_le(buf, max_special_ammo);

    // Idempotency grants
    serialize_grant_vector(buf, processed_grants);

    return buf;
}

ProgressionSnapshot ProgressionSnapshot::deserialize(const std::uint8_t* data, std::size_t size) {
    if (!data || size < 4) return create_default();  // Need at least version

    ProgressionSnapshot snap;
    std::size_t offset = 0;

    snap.version = read_le<ae::u32>(data, offset);

    // Migrate from older versions
    if (snap.version < kCurrentVersion) {
        // For a real decoder we'd parse the old format here.
        // Since v1 is the only version for now, just migrate.
        return migrate(snap);
    }

    // Parse the rest
    if (offset + 32 > size) return repair(snap);  // Need at least fixed fields
    snap.total_play_time_seconds = read_le<ae::u64>(data, offset);
    snap.last_session_id = read_le<ae::u32>(data, offset);
    snap.xp = read_le<ae::u64>(data, offset);
    snap.currency = read_le<ae::u64>(data, offset);

    snap.weapons = deserialize_entry_vector(data, offset, size);
    snap.armor = deserialize_entry_vector(data, offset, size);
    snap.perks = deserialize_entry_vector(data, offset, size);

    if (offset + 24 > size) return repair(snap);
    snap.max_health = read_le<float>(data, offset);
    snap.max_shield = read_le<float>(data, offset);
    snap.max_primary_ammo = read_le<ae::i32>(data, offset);
    snap.max_secondary_ammo = read_le<ae::i32>(data, offset);
    snap.max_heavy_ammo = read_le<ae::i32>(data, offset);
    snap.max_special_ammo = read_le<ae::i32>(data, offset);

    snap.processed_grants = deserialize_grant_vector(data, offset, size);

    return repair(snap);
}

// ===========================================================================
// Utility strings
// ===========================================================================

const char* grant_result_string(GrantResult r) {
    switch (r) {
        case GrantResult::Granted:        return "Granted";
        case GrantResult::InventoryFull:  return "InventoryFull";
        case GrantResult::Duplicate:       return "Duplicate";
        case GrantResult::AtCap:           return "AtCap";
        case GrantResult::Blocked:         return "Blocked";
        case GrantResult::Invalid:         return "Invalid";
        default:                           return "Unknown";
    }
}

const char* pickup_type_string(PickupType t) {
    switch (t) {
        case PickupType::PrimaryAmmo:    return "PrimaryAmmo";
        case PickupType::SecondaryAmmo:  return "SecondaryAmmo";
        case PickupType::HeavyAmmo:      return "HeavyAmmo";
        case PickupType::SpecialAmmo:    return "SpecialAmmo";
        case PickupType::HealthPack:     return "HealthPack";
        case PickupType::LargeHealth:    return "LargeHealth";
        case PickupType::ShieldCharge:   return "ShieldCharge";
        case PickupType::WeaponDrop:     return "WeaponDrop";
        case PickupType::ArmorDrop:      return "ArmorDrop";
        case PickupType::PerkDrop:       return "PerkDrop";
        case PickupType::GrenadeCharge:  return "GrenadeCharge";
        case PickupType::UltimateEnergy: return "UltimateEnergy";
        case PickupType::XPOrb:          return "XPOrb";
        case PickupType::Currency:       return "Currency";
        default:                         return "Unknown";
    }
}

} // namespace ahamkara::game
