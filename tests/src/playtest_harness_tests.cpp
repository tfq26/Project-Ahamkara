#include "ahamkara/client/playtest_harness.h"

#include <cassert>
#include <iostream>

namespace {

void test_default_autoplay_scenario_passes() {
    auto scenario = ahamkara::client::make_default_autoplay_scenario();
    scenario.level_path.clear();
    const auto result = ahamkara::client::run_playtest_scenario(scenario);

    assert(result.passed);
    assert(result.interaction_successes >= 1);
    assert(result.ammo_spent > 0);

    std::cout << "test_default_autoplay_scenario_passes passed.\n";
}

}  // namespace

int main() {
    test_default_autoplay_scenario_passes();
    return 0;
}
