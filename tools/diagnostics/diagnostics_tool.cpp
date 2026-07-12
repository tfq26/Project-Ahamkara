/**
 * @file diagnostics_tool.cpp
 * @brief CLI tool for collecting, viewing, and listing diagnostic bundles.
 *
 * Usage:
 *   ahamkara_diagnostics collect     — Collect a new diagnostic bundle
 *   ahamkara_diagnostics list        — List existing diagnostic bundles
 *   ahamkara_diagnostics view <idx>  — View a specific bundle (by index in list)
 *   ahamkara_diagnostics info        — Print system info only
 */

#include "ae/core/diagnostics.h"
#include "ae/core/log.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

void print_usage(const char* argv0) {
    std::cerr << "Usage:\n";
    std::cerr << "  " << argv0 << " collect       Collect a new diagnostic bundle\n";
    std::cerr << "  " << argv0 << " list          List existing diagnostic bundles\n";
    std::cerr << "  " << argv0 << " view <idx>    View contents of bundle at index\n";
    std::cerr << "  " << argv0 << " info          Print system information\n";
    std::cerr << "  " << argv0 << " help          Show this help\n";
}

int cmd_collect() {
    auto path = ae::write_diagnostic_bundle();
    if (path.empty()) {
        std::cerr << "Failed to collect diagnostic bundle.\n";
        return 1;
    }
    std::cout << "Diagnostic bundle written to: " << path << "\n";
    return 0;
}

int cmd_list() {
    auto bundles = ae::list_diagnostic_bundles();
    if (bundles.empty()) {
        std::cout << "No diagnostic bundles found.\n";
        return 0;
    }

    std::cout << "Found " << bundles.size() << " diagnostic bundle(s):\n";
    for (std::size_t i = 0; i < bundles.size(); ++i) {
        std::cout << "  [" << i << "] " << bundles[i].filename() << "\n";
    }
    return 0;
}

int cmd_view(int index) {
    auto bundles = ae::list_diagnostic_bundles();
    if (bundles.empty()) {
        std::cerr << "No diagnostic bundles found.\n";
        return 1;
    }

    if (index < 0 || static_cast<std::size_t>(index) >= bundles.size()) {
        std::cerr << "Index out of range. Use 'list' to see available bundles.\n";
        return 1;
    }

    const auto& dir = bundles[static_cast<std::size_t>(index)];
    std::cout << "Bundle: " << dir << "\n";
    std::cout << "---\n";

    // Print each file in the bundle
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (!entry.is_regular_file()) continue;
        std::cout << "\n=== " << entry.path().filename() << " ===\n";

        std::ifstream file(entry.path());
        if (file.is_open()) {
            std::string line;
            while (std::getline(file, line)) {
                std::cout << line << "\n";
            }
        }
        std::cout << "\n";
    }

    return 0;
}

int cmd_info() {
    auto info = ae::collect_system_info();
    std::cout << "=== System Information ===\n";
    std::cout << "OS:           " << info.os_name << "\n";
    std::cout << "CPU:          " << info.cpu_brand << "\n";
    std::cout << "Cores:        " << info.cpu_core_count << "\n";
    std::cout << "Total RAM:    " << (info.total_ram_bytes / (1024 * 1024)) << " MB\n";
    std::cout << "GPU:          " << info.gpu_renderer << "\n";
    std::cout << "Engine Ver:   " << info.engine_version << "\n";
    return 0;
}

}  // anonymous namespace

int main(int argc, char* argv[]) {
    // Initialize logging so the diagnostics module can log
    ae::init_file_logging("logs");
    ae::init_log_levels_from_env();

    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    std::string cmd = argv[1];

    if (cmd == "collect") {
        return cmd_collect();
    } else if (cmd == "list") {
        return cmd_list();
    } else if (cmd == "view") {
        if (argc < 3) {
            std::cerr << "Usage: " << argv[0] << " view <index>\n";
            return 1;
        }
        int index = std::atoi(argv[2]);
        return cmd_view(index);
    } else if (cmd == "info") {
        return cmd_info();
    } else {
        print_usage(argv[0]);
        return 1;
    }
}
