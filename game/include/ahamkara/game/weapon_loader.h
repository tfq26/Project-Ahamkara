#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ahamkara::game {

// --- Perk definition (matches perks.json) ------------------------------------

struct WeaponPerk {
    std::string id;
    std::string name;
    std::string slot;
    std::string description;
    std::string behavior;   // optional gameplay behavior tag
    float stats[16]{};      // key → value mapping, indexed by StatKey enum
};

// --- Archetype definition (matches archetypes.json) ---------------------------

struct WeaponArchetype {
    std::string id;
    std::string name;
    std::string slot;       // primary / secondary / heavy
    std::string mesh;       // asset path, e.g. "viewmodel_ar15"
    std::string fire_mode;  // automatic / semi_automatic / hitscan / projectile
    std::string projectile_type; // hitscan / projectile

    float base_stats[16]{}; // indexed by StatKey

    float recoil_pattern[8][2]{}; // (yaw, pitch) pairs
    int   recoil_shots{0};

    float headshot_multiplier{2.0F};
    float projectile_speed{0.0F};
    float projectile_damage_radius{0.0F};

    std::vector<std::string> perk_slots;
};

// --- Runtime weapon instance -------------------------------------------------

struct WeaponInstance {
    std::string archetype_id;
    std::vector<std::string> perk_ids;  // selected perks

    // Computed stats (archetype base + all perk deltas applied)
    float stats[16]{};
};

// --- Stat key enum (index into the stats[] array) ----------------------------

enum class StatKey : int {
    damage = 0,
    headshot_multiplier,
    pellets,
    rpm,
    magazine_size,
    reload_time_s,
    aim_down_sight_time_s,
    range,
    stability,
    handling,
    aim_assist,
    magazine_size_mult,
    damage_mult,
    projectile_speed_mult,
    _count
};

constexpr int kStatCount = static_cast<int>(StatKey::_count);

inline StatKey stat_key_from_string(std::string_view key) {
    using namespace std::string_view_literals;
    if (key == "damage"sv)                    return StatKey::damage;
    if (key == "headshot_multiplier"sv)        return StatKey::headshot_multiplier;
    if (key == "pellets"sv)                    return StatKey::pellets;
    if (key == "rpm"sv)                        return StatKey::rpm;
    if (key == "magazine_size"sv)              return StatKey::magazine_size;
    if (key == "reload_time_s"sv)              return StatKey::reload_time_s;
    if (key == "aim_down_sight_time_s"sv)      return StatKey::aim_down_sight_time_s;
    if (key == "range"sv)                      return StatKey::range;
    if (key == "stability"sv)                  return StatKey::stability;
    if (key == "handling"sv)                   return StatKey::handling;
    if (key == "aim_assist"sv)                 return StatKey::aim_assist;
    if (key == "magazine_size_mult"sv)         return StatKey::magazine_size_mult;
    if (key == "damage_mult"sv)                return StatKey::damage_mult;
    if (key == "projectile_speed_mult"sv)      return StatKey::projectile_speed_mult;
    return StatKey::_count;
}

// --- Weapon database (loaded once at startup) ---------------------------------

class WeaponDatabase {
public:
    bool load_json(const std::string& archetypes_path, const std::string& perks_path);

    [[nodiscard]] const WeaponArchetype* find_archetype(const std::string& id) const;
    [[nodiscard]] const WeaponPerk* find_perk(const std::string& id) const;

    /// Build a weapon instance from an archetype + perk list.
    /// Computes final stats by applying perk deltas to base stats.
    [[nodiscard]] WeaponInstance build_instance(const std::string& archetype_id,
                                                 const std::vector<std::string>& perk_ids) const;

    [[nodiscard]] const auto& archetypes() const { return archetypes_; }
    [[nodiscard]] const auto& perks() const { return perks_; }

private:
    void apply_perk(WeaponInstance& inst, const WeaponPerk& perk) const;
    void compute_final_stats(WeaponInstance& inst) const;

    std::vector<WeaponArchetype> archetypes_;
    std::unordered_map<std::string, std::size_t> archetype_index_;
    std::unordered_map<std::string, WeaponPerk> perks_;
};

}  // namespace ahamkara::game
