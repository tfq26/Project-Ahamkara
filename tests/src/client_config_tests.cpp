#include "ahamkara/client/client_config.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

int main() {
    namespace fs = std::filesystem;
    using ahamkara::client::ClientConfig;
    using ahamkara::client::clamp_character_color;

    static_assert(clamp_character_color(-0.2F) == 0.0F);
    static_assert(clamp_character_color(0.5F) == 0.5F);
    static_assert(clamp_character_color(1.4F) == 1.0F);

    const fs::path path = fs::temp_directory_path() / "ahamkara-character-config-test.cfg";
    {
        std::ofstream file(path);
        file << "suit_color_r=0.14\n"
             << "suit_color_g=0.42\n"
             << "suit_color_b=0.77\n";
    }
    ClientConfig loaded;
    assert(loaded.load_from_file(path.string()));
    assert(loaded.character.suit_color_r == 0.14F);
    assert(loaded.character.suit_color_g == 0.42F);
    assert(loaded.character.suit_color_b == 0.77F);

    const fs::path saved = fs::temp_directory_path() / "ahamkara-character-config-saved.cfg";
    assert(loaded.save_to_file(saved.string()));
    ClientConfig round_trip;
    assert(round_trip.load_from_file(saved.string()));
    assert(round_trip.character.suit_color_r == loaded.character.suit_color_r);
    assert(round_trip.character.suit_color_g == loaded.character.suit_color_g);
    assert(round_trip.character.suit_color_b == loaded.character.suit_color_b);

    fs::remove(path);
    fs::remove(saved);
    return 0;
}
