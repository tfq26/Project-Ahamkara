#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ae {

/**
 * @brief Simple polling-based file watcher for live-reload hooks.
 *
 * Monitors a set of files by polling their last-write timestamps.
 * When a change is detected, the registered callback is invoked.
 *
 * This is intentionally simple — no inotify/kqueue/FSEvents — to keep
 * the engine portable and avoid platform-specific APIs.
 * Poll at a reasonable interval (e.g. once per frame or every 500ms).
 */
class FileWatcher {
public:
    using ChangeCallback = std::function<void(const std::string& path)>;

    FileWatcher() = default;

    /// Watch a file. When a modification is detected, `on_change` is called.
    /// If the file doesn't exist yet, it will be watched once it appears.
    void watch(std::string_view path, ChangeCallback on_change);

    /// Stop watching a specific file.
    void unwatch(std::string_view path);

    /// Poll all watched files for changes. Returns the number of files
    /// that changed since the last poll.
    int poll();

    /// Clear all watched files.
    void clear();

    [[nodiscard]] int watched_count() const { return static_cast<int>(entries_.size()); }

private:
    struct Entry {
        ChangeCallback callback;
        std::filesystem::file_time_type last_write_time;
        bool has_initial_time {false};
    };

    std::unordered_map<std::string, Entry> entries_;
};

}  // namespace ae
