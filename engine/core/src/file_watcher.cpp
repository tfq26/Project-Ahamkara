#include "ae/core/file_watcher.h"
#include "ae/core/log.h"

#include <system_error>

#define AE_LOG_CATEGORY "FileWatcher"

namespace ae {

void FileWatcher::watch(std::string_view path, ChangeCallback on_change) {
    const std::string path_str(path);
    auto& entry = entries_[path_str];
    entry.callback = std::move(on_change);
    entry.has_initial_time = false;

    // Try to get initial write time.
    std::error_code ec;
    auto write_time = std::filesystem::last_write_time(path_str, ec);
    if (!ec) {
        entry.last_write_time = write_time;
        entry.has_initial_time = true;
        log_debug_cat(AE_LOG_CATEGORY, "Watching: " + path_str);
    } else {
        log_debug_cat(AE_LOG_CATEGORY, "Watching (pending): " + path_str + " (file not found yet)");
    }
}

void FileWatcher::unwatch(std::string_view path) {
    entries_.erase(std::string(path));
    log_debug_cat(AE_LOG_CATEGORY, "Unwatched: " + std::string(path));
}

int FileWatcher::poll() {
    int changed = 0;
    std::error_code ec;

    for (auto& [path, entry] : entries_) {
        const auto write_time = std::filesystem::last_write_time(path, ec);

        if (ec) {
            // File doesn't exist (yet) or is inaccessible.
            if (entry.has_initial_time) {
                // File was visible before; now gone — fire callback.
                entry.callback(path);
                ++changed;
                entry.has_initial_time = false;
            }
            continue;
        }

        if (!entry.has_initial_time) {
            // File just appeared.
            entry.last_write_time = write_time;
            entry.has_initial_time = true;
            entry.callback(path);
            ++changed;
            log_debug_cat(AE_LOG_CATEGORY, "File appeared: " + path);
            continue;
        }

        if (write_time > entry.last_write_time) {
            entry.last_write_time = write_time;
            entry.callback(path);
            ++changed;
            log_debug_cat(AE_LOG_CATEGORY, "File changed: " + path);
        }
    }

    return changed;
}

void FileWatcher::clear() {
    entries_.clear();
}

}  // namespace ae
