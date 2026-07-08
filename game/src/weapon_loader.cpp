#include "ahamkara/game/weapon_loader.h"

#include "ae/core/log.h"

#include <cctype>
#include <cstring>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#define AE_LOG_CATEGORY "Game"

namespace ahamkara::game {
namespace {

class JsonParser {
public:
    explicit JsonParser(const std::string& src) : src_(src), pos_(0) {}

    void skip_whitespace() {
        while (pos_ < src_.size() && (std::isspace(static_cast<unsigned char>(src_[pos_])) || src_[pos_] == ','))
            ++pos_;
    }

    char peek() { skip_whitespace(); return pos_ < src_.size() ? src_[pos_] : '\0'; }

    char next() { skip_whitespace(); return pos_ < src_.size() ? src_[pos_++] : '\0'; }

    std::string read_string() {
        if (next() != '"') return {};
        std::string result;
        while (pos_ < src_.size() && src_[pos_] != '"') {
            if (src_[pos_] == '\\') { ++pos_; if (pos_ < src_.size()) result += src_[pos_++]; }
            else result += src_[pos_++];
        }
        ++pos_; // skip closing quote
        return result;
    }

    float read_number() {
        skip_whitespace();
        std::size_t start = pos_;
        if (pos_ < src_.size() && src_[pos_] == '-') ++pos_;
        while (pos_ < src_.size() && (std::isdigit(static_cast<unsigned char>(src_[pos_])) || src_[pos_] == '.'))
            ++pos_;
        return std::stof(std::string(src_.substr(start, pos_ - start)));
    }

    bool match(char expected) {
        skip_whitespace();
        if (pos_ < src_.size() && src_[pos_] == expected) { ++pos_; return true; }
        return false;
    }

    /// Advance past the current value (object, array, string, number, or literal).
    void skip_value() {
        skip_whitespace();
        if (pos_ >= src_.size()) return;
        char c = src_[pos_];
        if (c == '{') { ++pos_; int depth = 1; while (depth > 0 && pos_ < src_.size()) { if (src_[pos_]=='{') ++depth; else if (src_[pos_]=='}') --depth; ++pos_; } }
        else if (c == '[') { ++pos_; int depth = 1; while (depth > 0 && pos_ < src_.size()) { if (src_[pos_]=='[') ++depth; else if (src_[pos_]==']') --depth; ++pos_; } }
        else if (c == '"') { read_string(); }
        else { while (pos_ < src_.size() && !std::isspace(static_cast<unsigned char>(src_[pos_])) && src_[pos_] != ',' && src_[pos_] != '}' && src_[pos_] != ']') ++pos_; }
    }

    std::string_view src_;
    std::size_t pos_;
};

bool parse_perks(JsonParser& p, std::unordered_map<std::string, WeaponPerk>& out) {
    if (p.next() != '[') return false;

    while (p.peek() != ']') {
        if (p.next() != '{') return false;
        WeaponPerk perk;

        while (p.peek() != '}') {
            std::string key = p.read_string();
            if (p.next() != ':') return false;

            if (key == "id" && p.peek() == '"')           perk.id = p.read_string();
            else if (key == "name" && p.peek() == '"')    perk.name = p.read_string();
            else if (key == "slot" && p.peek() == '"')    perk.slot = p.read_string();
            else if (key == "description" && p.peek() == '"') perk.description = p.read_string();
            else if (key == "behavior" && p.peek() == '"')  perk.behavior = p.read_string();
            else if (key == "stats" && p.peek() == '{') {
                if (p.next() != '{') return false;
                while (p.peek() != '}') {
                    std::string stat_key = p.read_string();
                    if (p.next() != ':') return false;
                    float val = p.read_number();
                    auto sk = stat_key_from_string(stat_key);
                    if (static_cast<int>(sk) < kStatCount) {
                        perk.stats[static_cast<int>(sk)] = val;
                    }
                }
                p.next(); // }
            } else {
                p.skip_value();
            }
        }
        p.next(); // }

        if (!perk.id.empty()) {
            out[perk.id] = std::move(perk);
        }
    }
    p.next(); // ]
    return true;
}

bool parse_archetypes(JsonParser& p, std::vector<WeaponArchetype>& out) {
    if (p.next() != '[') return false;

    while (p.peek() != ']') {
        if (p.next() != '{') return false;
        WeaponArchetype arch;

        while (p.peek() != '}') {
            std::string key = p.read_string();
            if (p.next() != ':') return false;

            if (key == "id" && p.peek() == '"')              arch.id = p.read_string();
            else if (key == "name" && p.peek() == '"')       arch.name = p.read_string();
            else if (key == "slot" && p.peek() == '"')       arch.slot = p.read_string();
            else if (key == "mesh" && p.peek() == '"')       arch.mesh = p.read_string();
            else if (key == "fire_mode" && p.peek() == '"')  arch.fire_mode = p.read_string();
            else if (key == "projectile_type" && p.peek() == '"') arch.projectile_type = p.read_string();
            else if (key == "headshot_multiplier" && p.peek() != '"') arch.headshot_multiplier = p.read_number();
            else if (key == "projectile_speed" && p.peek() != '"')  arch.projectile_speed = p.read_number();
            else if (key == "projectile_damage_radius" && p.peek() != '"') arch.projectile_damage_radius = p.read_number();
            else if (key == "base_stats" && p.peek() == '{') {
                if (p.next() != '{') return false;
                while (p.peek() != '}') {
                    std::string stat_key = p.read_string();
                    if (p.next() != ':') return false;
                    float val = p.read_number();
                    auto sk = stat_key_from_string(stat_key);
                    if (static_cast<int>(sk) < kStatCount) {
                        arch.base_stats[static_cast<int>(sk)] = val;
                    }
                }
                p.next(); // }
            }
            else if (key == "recoil_pattern" && p.peek() == '[') {
                p.next(); // [
                arch.recoil_shots = 0;
                while (p.peek() != ']') {
                    if (p.next() != '[') return false;
                    float yaw = p.read_number();
                    float pitch = p.read_number();
                    p.next(); // ]
                    if (arch.recoil_shots < 8) {
                        arch.recoil_pattern[arch.recoil_shots][0] = yaw;
                        arch.recoil_pattern[arch.recoil_shots][1] = pitch;
                        ++arch.recoil_shots;
                    }
                }
                p.next(); // ]
            }
            else if (key == "perk_slots" && p.peek() == '[') {
                p.next(); // [
                while (p.peek() != ']') {
                    std::string slot = p.read_string();
                    if (!slot.empty()) arch.perk_slots.push_back(slot);
                }
                p.next(); // ]
            }
            else {
                p.skip_value();
            }
        }
        p.next(); // }

        if (!arch.id.empty()) {
            out.push_back(std::move(arch));
        }
    }
    p.next(); // ]
    return true;
}

}  // namespace

bool WeaponDatabase::load_json(const std::string& archetypes_path, const std::string& perks_path) {
    // Load perks first (they're referenced by archetypes during instance building)
    {
        std::ifstream file(perks_path);
        if (!file.is_open()) {
            ae::log_error_cat(AE_LOG_CATEGORY, "Cannot open perks file: " + perks_path);
            return false;
        }
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        JsonParser p(content);

        // Find the "perks" key
        while (p.peek() != '\0') {
            if (p.peek() == '"') {
                std::string key = p.read_string();
                if (p.next() != ':') break;
                if (key == "perks") {
                    if (!parse_perks(p, perks_)) {
                        ae::log_error_cat(AE_LOG_CATEGORY, "Failed to parse perks from " + perks_path);
                        return false;
                    }
                    break;
                }
                p.skip_value();
            } else {
                p.next();
            }
        }
        ae::log_info_cat(AE_LOG_CATEGORY, "Loaded " + std::to_string(perks_.size()) + " perks from " + perks_path);
    }

    // Load archetypes
    {
        std::ifstream file(archetypes_path);
        if (!file.is_open()) {
            ae::log_error_cat(AE_LOG_CATEGORY, "Cannot open archetypes file: " + archetypes_path);
            return false;
        }
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        JsonParser p(content);

        while (p.peek() != '\0') {
            if (p.peek() == '"') {
                std::string key = p.read_string();
                if (p.next() != ':') break;
                if (key == "archetypes") {
                    if (!parse_archetypes(p, archetypes_)) {
                        ae::log_error_cat(AE_LOG_CATEGORY, "Failed to parse archetypes from " + archetypes_path);
                        return false;
                    }
                    break;
                }
                p.skip_value();
            } else {
                p.next();
            }
        }
        ae::log_info_cat(AE_LOG_CATEGORY, "Loaded " + std::to_string(archetypes_.size()) + " archetypes from " + archetypes_path);
    }

    for (std::size_t i = 0; i < archetypes_.size(); ++i) {
        archetype_index_[archetypes_[i].id] = i;
    }

    return true;
}

const WeaponArchetype* WeaponDatabase::find_archetype(const std::string& id) const {
    auto it = archetype_index_.find(id);
    if (it == archetype_index_.end()) return nullptr;
    return &archetypes_[it->second];
}

const WeaponPerk* WeaponDatabase::find_perk(const std::string& id) const {
    auto it = perks_.find(id);
    return (it != perks_.end()) ? &it->second : nullptr;
}

void WeaponDatabase::apply_perk(WeaponInstance& inst, const WeaponPerk& perk) const {
    for (int i = 0; i < kStatCount; ++i) {
        inst.stats[i] += perk.stats[i];
    }
}

void WeaponDatabase::compute_final_stats(WeaponInstance& inst) const {
    const auto* arch = find_archetype(inst.archetype_id);
    if (!arch) return;

    // Start with base stats
    for (int i = 0; i < kStatCount; ++i) {
        inst.stats[i] = arch->base_stats[i];
    }

    // Apply each perk's additive deltas
    for (const auto& perk_id : inst.perk_ids) {
        const auto* perk = find_perk(perk_id);
        if (perk) apply_perk(inst, *perk);
    }

    // Apply multiplicative modifiers LAST (so they compound on top of additive deltas)
    int base_size = static_cast<int>(arch->base_stats[static_cast<int>(StatKey::magazine_size)]);
    if (base_size <= 0) base_size = 1;

    float mag_mult = inst.stats[static_cast<int>(StatKey::magazine_size_mult)];
    if (mag_mult > 0.0F) {
        inst.stats[static_cast<int>(StatKey::magazine_size)] = std::max(1.0F, static_cast<float>(base_size) * mag_mult);
    }

    float dmg_mult = inst.stats[static_cast<int>(StatKey::damage_mult)];
    if (dmg_mult > 0.0F) {
        inst.stats[static_cast<int>(StatKey::damage)] *= dmg_mult;
    }

    float speed_mult = inst.stats[static_cast<int>(StatKey::projectile_speed_mult)];
    if (speed_mult > 0.0F) {
        // projectile_speed is a field on the archetype, not in stats[]
        // This would be handled at runtime by the projectile spawner
    }
}

WeaponInstance WeaponDatabase::build_instance(const std::string& archetype_id,
                                               const std::vector<std::string>& perk_ids) const {
    WeaponInstance inst;
    inst.archetype_id = archetype_id;
    inst.perk_ids = perk_ids;
    compute_final_stats(inst);
    return inst;
}

}  // namespace ahamkara::game
