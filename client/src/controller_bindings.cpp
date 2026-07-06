#include "ahamkara/client/controller_bindings.h"

#include "ae/core/log.h"

#include <charconv>
#include <fstream>
#include <string>
#include <string_view>

namespace ahamkara::client {
namespace {

std::string_view trim(std::string_view value) {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) {
        value.remove_prefix(1);
    }
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) {
        value.remove_suffix(1);
    }
    return value;
}

bool parse_hex_code(std::string_view value, ae::GamepadInputCode& output) {
    unsigned int parsed = 0;
    const auto* begin = value.data();
    const auto* end = value.data() + value.size();

    if (value.size() > 2 && value[0] == '0' && (value[1] == 'x' || value[1] == 'X')) {
        begin += 2;
    }

    const auto [ptr, ec] = std::from_chars(begin, end, parsed, 16);
    if (ec != std::errc {} || ptr != end) {
        return false;
    }

    output = static_cast<ae::GamepadInputCode>(parsed);
    return true;
}

void assign_binding(std::string_view key, ae::GamepadInputCode code, ControllerBindings& bindings) {
    if (key == "jump") bindings.jump = code;
    else if (key == "crouch") bindings.crouch = code;
    else if (key == "slide") bindings.slide = code;
    else if (key == "reload") bindings.reload = code;
    else if (key == "sprint") bindings.sprint = code;
    else if (key == "ability") bindings.ability = code;
    else if (key == "interact") bindings.interact = code;
    else if (key == "metrics") bindings.metrics = code;
    else if (key == "menu") bindings.menu = code;
    else if (key == "toggle_perspective") bindings.toggle_perspective = code;
    else if (key == "fire") bindings.fire = code;
}

void write_binding(std::ofstream& file, const char* key, ae::GamepadInputCode code) {
    char buffer[32] {};
    std::snprintf(buffer, sizeof(buffer), "0x%08X", static_cast<unsigned int>(code));
    file << key << " = " << buffer << '\n';
}

}  // namespace

bool ControllerBindings::load_from_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        ae::log_info("No controller binding profile found, using defaults.");
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        const auto comment = line.find('#');
        std::string_view view = comment == std::string::npos ? std::string_view(line) : std::string_view(line).substr(0, comment);
        view = trim(view);
        if (view.empty()) {
            continue;
        }

        const auto delimiter = view.find('=');
        if (delimiter == std::string_view::npos) {
            continue;
        }

        const std::string_view key = trim(view.substr(0, delimiter));
        const std::string_view value = trim(view.substr(delimiter + 1));
        ae::GamepadInputCode code = ae::kInvalidGamepadInputCode;
        if (parse_hex_code(value, code)) {
            assign_binding(key, code, *this);
        }
    }

    ae::log_info("Controller bindings loaded from file.");
    return true;
}

bool ControllerBindings::save_to_file(const std::string& path) const {
    std::ofstream file(path, std::ios::trunc);
    if (!file.is_open()) {
        return false;
    }

    file << "# Ahamkara controller bindings\n";
    write_binding(file, "jump", jump);
    write_binding(file, "crouch", crouch);
    write_binding(file, "slide", slide);
    write_binding(file, "reload", reload);
    write_binding(file, "sprint", sprint);
    write_binding(file, "ability", ability);
    write_binding(file, "interact", interact);
    write_binding(file, "metrics", metrics);
    write_binding(file, "menu", menu);
    write_binding(file, "toggle_perspective", toggle_perspective);
    write_binding(file, "fire", fire);
    return true;
}

}  // namespace ahamkara::client
