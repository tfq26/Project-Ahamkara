#pragma once

#include <string>
#include <vector>

namespace ae::ui {

// A settings row that is intentionally not backed by a working implementation
// in this build.  The settings UI renders each entry disabled with an explicit
// reason instead of silently accepting input that has no effect.
struct UnavailableSetting {
    std::string id;
    std::string label;
    std::string reason;
};

// Returns the catalog of settings rows presented as unavailable.  The catalog
// is kept separate from the ImGui render code so availability and navigation
// behavior can be unit-tested without a windowing/rendering context.
const std::vector<UnavailableSetting>& unavailable_settings();

} // namespace ae::ui
