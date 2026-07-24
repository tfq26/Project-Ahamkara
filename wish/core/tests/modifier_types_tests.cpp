#include "wish/core/activity_loader.h"
#include "wish/core/activity_modifier.h"
#include "wish/core/live_content_hooks.h"
#include "wish/core/modifier_types.h"

#include <cstdio>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>

// ── Simple test framework ──────────────────────────────────────────────────

namespace {

int fail(const std::string& msg) {
    std::cerr << "modifier_tests FAILED: " << msg << '\n';
    return 1;
}

#define EXPECT(cond, msg)     \
    do {                      \
        if (!(cond))          \
            return fail(msg); \
    } while (0)

// ── Mock activity for LiveContentModifier tests ────────────────────────────

struct MockActivity : wish::core::IActivityBase {
    bool init_called {false};
    bool shutdown_called {false};
    bool admit_called {false};
    bool remove_called {false};
    bool tick_called {false};
    bool process_input_called {false};
    bool snapshot_called {false};
    wish::core::ActivityConfig stored_config {};
    wish::u32 player_count_ {0};

    bool initialize(const wish::core::ActivityConfig& cfg) override {
        init_called = true;
        stored_config = cfg;
        return true;
    }

    void shutdown() override { shutdown_called = true; }

    bool admit_player(const wish::core::SessionAdmissionRequest&) override {
        admit_called = true;
        player_count_ = 1;
        return true;
    }

    void remove_player(wish::session::SessionId) override { remove_called = true; }

    wish::u32 player_count() const override { return player_count_; }

    void tick(float) override { tick_called = true; }

    void process_input(wish::session::SessionId,
                       const wish::PacketEnvelope&,
                       wish::u32) override {
        process_input_called = true;
    }

    wish::usize build_snapshot_bytes(wish::session::SessionId,
                                      std::span<std::byte>) override {
        snapshot_called = true;
        return 0;
    }

    bool is_complete() const override { return false; }

    wish::core::ActivityId activity_id() const override { return stored_config.id; }
    wish::core::ActivityCategory category() const override { return stored_config.category; }
    std::string_view activity_name() const override { return stored_config.name; }

    void for_each_connected_snapshot(
        void (*)(void*, wish::session::SessionId,
                 const std::byte*, wish::usize),
        void*) override {}
};

// ── Tests: ModifierType enum ───────────────────────────────────────────────

int test_modifier_type_to_string() {
    EXPECT(wish::core::modifier_type_name(wish::core::ModifierType::None) == "none",
           "None should map to 'none'");
    EXPECT(wish::core::modifier_type_name(wish::core::ModifierType::DamageMultiplier) == "damage_multiplier",
           "DamageMultiplier should map to 'damage_multiplier'");
    EXPECT(wish::core::modifier_type_name(wish::core::ModifierType::SpeedModifier) == "speed_modifier",
           "SpeedModifier should map to 'speed_modifier'");
    EXPECT(wish::core::modifier_type_name(wish::core::ModifierType::HealthRegen) == "health_regen",
           "HealthRegen should map to 'health_regen'");
    EXPECT(wish::core::modifier_type_name(wish::core::ModifierType::FriendlyFireToggle) == "friendly_fire_toggle",
           "FriendlyFireToggle should map to 'friendly_fire_toggle'");
    EXPECT(wish::core::modifier_type_name(static_cast<wish::core::ModifierType>(99)) == "unknown",
           "Unknown type should map to 'unknown'");
    std::cout << "test_modifier_type_to_string: ok\n";
    return 0;
}

int test_parse_modifier_type() {
    EXPECT(wish::core::parse_modifier_type("none") == wish::core::ModifierType::None,
           "'none' should parse to None");
    EXPECT(wish::core::parse_modifier_type("damage_multiplier") == wish::core::ModifierType::DamageMultiplier,
           "'damage_multiplier' should parse correctly");
    EXPECT(wish::core::parse_modifier_type("speed_modifier") == wish::core::ModifierType::SpeedModifier,
           "'speed_modifier' should parse correctly");
    EXPECT(wish::core::parse_modifier_type("friendly_fire_toggle") == wish::core::ModifierType::FriendlyFireToggle,
           "'friendly_fire_toggle' should parse correctly");
    EXPECT(wish::core::parse_modifier_type("unknown_type") == wish::core::ModifierType::None,
           "'unknown_type' should parse to None");
    std::cout << "test_parse_modifier_type: ok\n";
    return 0;
}

// ── Tests: ModifierConfig and ModifierParam ────────────────────────────────

int test_modifier_config_defaults() {
    wish::core::ModifierConfig cfg {};
    EXPECT(cfg.type == wish::core::ModifierType::None, "default type should be None");
    EXPECT(cfg.active, "default should be active");
    EXPECT(cfg.duration == 0.0F, "default duration should be 0 (permanent)");
    EXPECT(cfg.remaining_time == 0.0F, "default remaining_time should be 0");
    EXPECT(cfg.params.empty(), "default params should be empty");
    std::cout << "test_modifier_config_defaults: ok\n";
    return 0;
}

int test_modifier_param_lookup() {
    wish::core::ModifierConfig cfg {};
    cfg.type = wish::core::ModifierType::DamageMultiplier;
    cfg.name = "Double Damage";
    cfg.params.push_back({"multiplier", "2.0"});
    cfg.params.push_back({"headshot_only", "false"});

    auto mult = wish::core::find_modifier_param(cfg, "multiplier");
    EXPECT(mult == "2.0", "multiplier param should be '2.0'");

    auto hs = wish::core::find_modifier_param(cfg, "headshot_only");
    EXPECT(hs == "false", "headshot_only param should be 'false'");

    auto missing = wish::core::find_modifier_param(cfg, "nonexistent");
    EXPECT(missing.empty(), "nonexistent param should be empty");
    std::cout << "test_modifier_param_lookup: ok\n";
    return 0;
}

int test_is_modifier_active() {
    wish::core::ModifierConfig cfg {};

    // Default config should be active (active=true, duration=0)
    cfg.active = true;
    cfg.duration = 0.0F;
    cfg.remaining_time = 0.0F;
    EXPECT(wish::core::is_modifier_active(cfg), "permanent active modifier should be active");

    // Inactive config
    cfg.active = false;
    EXPECT(!wish::core::is_modifier_active(cfg), "inactive modifier should not be active");

    // Timed modifier with remaining time
    cfg.active = true;
    cfg.duration = 30.0F;
    cfg.remaining_time = 15.0F;
    EXPECT(wish::core::is_modifier_active(cfg), "timed modifier with remaining time should be active");

    // Expired timed modifier
    cfg.remaining_time = 0.0F;
    EXPECT(!wish::core::is_modifier_active(cfg), "expired timed modifier should not be active");
    std::cout << "test_is_modifier_active: ok\n";
    return 0;
}

// ── Tests: JSON parsing via ActivityLoader ─────────────────────────────────

int test_parse_activity_with_modifiers() {
    constexpr std::string_view json = R"({
        "id": 10,
        "name": "Test Activity",
        "category": "PvP",
        "max_players": 8,
        "modifiers": [
            {
                "type": "damage_multiplier",
                "name": "Double Damage",
                "params": {
                    "multiplier": "2.0",
                    "headshot_only": "false"
                },
                "active": true,
                "duration": 60.0
            },
            {
                "type": "speed_modifier",
                "name": "Speed Boost",
                "params": {
                    "speed_mult": "1.5"
                },
                "active": true,
                "duration": 30.0
            }
        ],
        "modifier_rotation_enabled": true
    })";

    wish::core::ActivityConfig cfg {};
    bool ok = wish::core::ActivityLoader::parse_one(json, cfg);
    EXPECT(ok, "should parse successfully");
    EXPECT(cfg.id == 10, "id should be 10");
    EXPECT(cfg.name == "Test Activity", "name should match");
    EXPECT(cfg.modifier_rotation_enabled, "modifier_rotation_enabled should be true");

    EXPECT(cfg.modifiers.size() == 2, "should have 2 modifiers");
    if (cfg.modifiers.size() >= 1) {
        EXPECT(cfg.modifiers[0].type == wish::core::ModifierType::DamageMultiplier,
               "first modifier type should be DamageMultiplier");
        EXPECT(cfg.modifiers[0].name == "Double Damage",
               "first modifier name should be 'Double Damage'");
        EXPECT(cfg.modifiers[0].active, "first modifier should be active");
        EXPECT(cfg.modifiers[0].duration == 60.0F, "first modifier duration should be 60.0");

        auto mult = wish::core::find_modifier_param(cfg.modifiers[0], "multiplier");
        EXPECT(mult == "2.0", "first modifier multiplier param should be '2.0'");
    }
    if (cfg.modifiers.size() >= 2) {
        EXPECT(cfg.modifiers[1].type == wish::core::ModifierType::SpeedModifier,
               "second modifier type should be SpeedModifier");
        EXPECT(cfg.modifiers[1].name == "Speed Boost",
               "second modifier name should be 'Speed Boost'");

        auto speed = wish::core::find_modifier_param(cfg.modifiers[1], "speed_mult");
        EXPECT(speed == "1.5", "second modifier speed_mult param should be '1.5'");
    }
    std::cout << "test_parse_activity_with_modifiers: ok\n";
    return 0;
}

int test_parse_activity_without_modifiers() {
    constexpr std::string_view json = R"({
        "id": 11,
        "name": "Plain Activity",
        "category": "PvE",
        "max_players": 4
    })";

    wish::core::ActivityConfig cfg {};
    bool ok = wish::core::ActivityLoader::parse_one(json, cfg);
    EXPECT(ok, "should parse successfully");
    EXPECT(cfg.modifiers.empty(), "should have no modifiers");
    EXPECT(!cfg.modifier_rotation_enabled, "modifier_rotation_enabled should default to false");
    std::cout << "test_parse_activity_without_modifiers: ok\n";
    return 0;
}

int test_parse_modifier_without_name() {
    constexpr std::string_view json = R"({
        "id": 12,
        "name": "Nameless Mods",
        "category": "PvP",
        "modifiers": [
            {
                "type": "health_regen",
                "params": {
                    "regen_per_second": "5.0"
                },
                "duration": 15.0
            }
        ]
    })";

    wish::core::ActivityConfig cfg {};
    bool ok = wish::core::ActivityLoader::parse_one(json, cfg);
    EXPECT(ok, "should parse successfully");
    EXPECT(cfg.modifiers.size() == 1, "should have 1 modifier");
    if (!cfg.modifiers.empty()) {
        EXPECT(cfg.modifiers[0].type == wish::core::ModifierType::HealthRegen,
               "modifier type should be HealthRegen");
        // Name should fall back to type name when not specified
        EXPECT(cfg.modifiers[0].name == "health_regen",
               "unnamed modifier should fall back to type name");
    }
    std::cout << "test_parse_modifier_without_name: ok\n";
    return 0;
}

int test_parse_invalid_modifier_type() {
    constexpr std::string_view json = R"({
        "id": 13,
        "name": "Bad Mod Type",
        "category": "Custom",
        "modifiers": [
            {
                "type": "nonexistent_modifier",
                "name": "Broken"
            }
        ]
    })";

    wish::core::ActivityConfig cfg {};
    bool ok = wish::core::ActivityLoader::parse_one(json, cfg);
    // The activity should still parse, but the invalid modifier should be skipped
    EXPECT(ok, "activity should parse even with invalid modifier");
    // Actually, looking at parse_modifier, it returns false for unknown types,
    // so the modifier should be silently skipped.
    EXPECT(cfg.modifiers.empty(), "invalid modifier should be skipped");
    std::cout << "test_parse_invalid_modifier_type: ok\n";
    return 0;
}

// ── Tests: LiveContentModifier ─────────────────────────────────────────────

int test_live_content_modifier_query() {
    auto mock = std::make_unique<MockActivity>();
    auto* mock_ptr = mock.get();

    using LiveMock = wish::core::LiveContentModifier<MockActivity>;
    auto modifier = std::make_unique<LiveMock>(std::move(mock));

    wish::core::ActivityConfig cfg {};
    cfg.id = 100;
    cfg.name = "Live Test";
    cfg.modifiers.push_back({
        .type = wish::core::ModifierType::DamageMultiplier,
        .name = "Test Damage Mod",
        .params = {{"multiplier", "2.5"}},
        .active = true,
        .duration = 0.0F, // permanent
        .remaining_time = 0.0F,
        .rotation_order = 0
    });

    bool init_ok = modifier->initialize(cfg);
    EXPECT(init_ok, "LiveContentModifier should initialize successfully");
    EXPECT(mock_ptr->init_called, "base initialize should be called");

    // Query API
    EXPECT(modifier->has_active_modifier(wish::core::ModifierType::DamageMultiplier),
           "should detect active DamageMultiplier modifier");
    EXPECT(!modifier->has_active_modifier(wish::core::ModifierType::SpeedModifier),
           "should not detect non-existent modifier type");

    // Parameter query
    auto mult = modifier->get_param(wish::core::ModifierType::DamageMultiplier, "multiplier");
    EXPECT(mult == "2.5", "multiplier param should be '2.5'");

    // Name query
    EXPECT(modifier->has_active_modifier("Test Damage Mod"),
           "should detect modifier by name");

    std::cout << "test_live_content_modifier_query: ok\n";
    return 0;
}

int test_live_content_modifier_timer_decay() {
    auto mock = std::make_unique<MockActivity>();
    auto* mock_ptr = mock.get();

    using LiveMock = wish::core::LiveContentModifier<MockActivity>;
    auto modifier = std::make_unique<LiveMock>(std::move(mock));

    wish::core::ActivityConfig cfg {};
    cfg.id = 101;
    cfg.name = "Timer Test";
    cfg.modifiers.push_back({
        .type = wish::core::ModifierType::DamageMultiplier,
        .name = "Timed Mod",
        .params = {},
        .active = true,
        .duration = 1.0F, // 1 second duration
        .remaining_time = 1.0F,
        .rotation_order = 0
    });

    modifier->initialize(cfg);
    EXPECT(modifier->has_active_modifier(wish::core::ModifierType::DamageMultiplier),
           "modifier should be active after init");

    // Tick for 0.6 seconds — still active
    modifier->tick(0.6F);
    EXPECT(mock_ptr->tick_called, "base tick should be called");
    EXPECT(modifier->has_active_modifier(wish::core::ModifierType::DamageMultiplier),
           "modifier should still be active after 0.6s");

    // Tick for 0.5 more seconds — should expire
    modifier->tick(0.5F);
    EXPECT(!modifier->has_active_modifier(wish::core::ModifierType::DamageMultiplier),
           "modifier should be expired after 1.1s");

    std::cout << "test_live_content_modifier_timer_decay: ok\n";
    return 0;
}

int test_live_content_modifier_rotation() {
    auto mock = std::make_unique<MockActivity>();

    using LiveMock = wish::core::LiveContentModifier<MockActivity>;
    auto modifier = std::make_unique<LiveMock>(std::move(mock));

    wish::core::ActivityConfig cfg {};
    cfg.id = 102;
    cfg.name = "Rotation Test";
    cfg.modifier_rotation_enabled = true;
    cfg.modifiers.push_back({
        .type = wish::core::ModifierType::DamageMultiplier,
        .name = "Mod A",
        .params = {},
        .active = true,
        .duration = 0.0F,
        .rotation_order = 0
    });
    cfg.modifiers.push_back({
        .type = wish::core::ModifierType::SpeedModifier,
        .name = "Mod B",
        .params = {},
        .active = false,
        .duration = 0.0F,
        .rotation_order = 0
    });

    modifier->initialize(cfg);
    modifier->set_rotation_interval(0.5F);

    // Initially Mod A should be active
    EXPECT(modifier->has_active_modifier(wish::core::ModifierType::DamageMultiplier),
           "Mod A (DamageMultiplier) should be active initially");
    EXPECT(!modifier->has_active_modifier(wish::core::ModifierType::SpeedModifier),
           "Mod B (SpeedModifier) should not be active initially");

    // Tick past rotation interval
    modifier->tick(0.6F);

    // Now Mod B should be active
    EXPECT(!modifier->has_active_modifier(wish::core::ModifierType::DamageMultiplier),
           "Mod A should be inactive after rotation");
    EXPECT(modifier->has_active_modifier(wish::core::ModifierType::SpeedModifier),
           "Mod B should be active after rotation");

    // Manual rotation
    modifier->rotate_next();
    EXPECT(modifier->has_active_modifier(wish::core::ModifierType::DamageMultiplier),
           "Mod A should be active after manual rotate_next");

    // Reset rotation
    modifier->rotate_reset();
    EXPECT(modifier->has_active_modifier(wish::core::ModifierType::DamageMultiplier),
           "Mod A should be active after reset");

    std::cout << "test_live_content_modifier_rotation: ok\n";
    return 0;
}

int test_live_content_modifier_active_list() {
    auto mock = std::make_unique<MockActivity>();

    using LiveMock = wish::core::LiveContentModifier<MockActivity>;
    auto modifier = std::make_unique<LiveMock>(std::move(mock));

    wish::core::ActivityConfig cfg {};
    cfg.id = 103;
    cfg.name = "Active List Test";
    cfg.modifiers.push_back({
        .type = wish::core::ModifierType::DamageMultiplier,
        .name = "Active Mod",
        .params = {},
        .active = true,
        .duration = 0.0F,
        .rotation_order = 0
    });
    cfg.modifiers.push_back({
        .type = wish::core::ModifierType::HealthRegen,
        .name = "Inactive Mod",
        .params = {},
        .active = false,
        .duration = 0.0F,
        .rotation_order = 0
    });

    modifier->initialize(cfg);

    auto all = modifier->all_modifiers();
    EXPECT(all.size() == 2, "all_modifiers should return 2 configs");

    auto active = modifier->active_modifiers();
    EXPECT(active.size() == 1, "active_modifiers should return 1 config");
    EXPECT(active[0].type == wish::core::ModifierType::DamageMultiplier,
           "active modifier should be DamageMultiplier");

    std::cout << "test_live_content_modifier_active_list: ok\n";
    return 0;
}

// ── Tests: Helper functions from live_content_hooks.cpp ────────────────────

int test_helper_functions() {
    wish::core::ModifierConfig cfg {};
    cfg.type = wish::core::ModifierType::DamageMultiplier;
    cfg.params.push_back({"multiplier", "2.5"});
    cfg.params.push_back({"headshot_only", "true"});
    cfg.params.push_back({"max_stacks", "3"});

    float mult = wish::core::get_modifier_float_param(cfg, "multiplier");
    EXPECT(mult == 2.5F, "float param 'multiplier' should be 2.5");

    bool hs = wish::core::get_modifier_bool_param(cfg, "headshot_only");
    EXPECT(hs, "bool param 'headshot_only' should be true");

    int stacks = wish::core::get_modifier_int_param(cfg, "max_stacks");
    EXPECT(stacks == 3, "int param 'max_stacks' should be 3");

    // Missing key returns default
    float missing = wish::core::get_modifier_float_param(cfg, "nonexistent", 1.0F);
    EXPECT(missing == 1.0F, "missing float param should return default");

    std::cout << "test_helper_functions: ok\n";
    return 0;
}

int test_live_content_hooks_version() {
    std::string version = wish::core::live_content_hooks_version();
    EXPECT(!version.empty(), "version string should not be empty");
    EXPECT(version == "1.0.0", "version should be '1.0.0'");
    std::cout << "test_live_content_hooks_version: ok\n";
    return 0;
}

} // anonymous namespace

int main() {
    int failures = 0;

    // ModifierType enum tests
    failures += test_modifier_type_to_string();
    failures += test_parse_modifier_type();

    // ModifierConfig tests
    failures += test_modifier_config_defaults();
    failures += test_modifier_param_lookup();
    failures += test_is_modifier_active();

    // JSON parsing tests
    failures += test_parse_activity_with_modifiers();
    failures += test_parse_activity_without_modifiers();
    failures += test_parse_modifier_without_name();
    failures += test_parse_invalid_modifier_type();

    // LiveContentModifier tests
    failures += test_live_content_modifier_query();
    failures += test_live_content_modifier_timer_decay();
    failures += test_live_content_modifier_rotation();
    failures += test_live_content_modifier_active_list();

    // Helper function tests
    failures += test_helper_functions();
    failures += test_live_content_hooks_version();

    if (failures > 0) {
        std::cerr << failures << " modifier test(s) FAILED.\n";
        return 1;
    }

    std::cout << "All modifier tests passed.\n";
    return 0;
}
