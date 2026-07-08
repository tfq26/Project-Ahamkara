#pragma once

#include "ae/core/config.h"

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace ae {

/**
 * @brief Runtime developer console for executing commands and modifying cvars.
 *
 * The console maintains a ring buffer of log lines and a command history.
 * It integrates with ConfigRegistry to provide get/set for registered cvars.
 *
 * Usage:
 * @code
 *   Console console;
 *   console.register_command("help", "Show available commands",
 *       [](const std::vector<std::string>&, Console& c) { c.print("Available commands: ..."); });
 *   console.execute("player_speed 10.0");
 * @endcode
 */
class Console {
public:
    static constexpr int kMaxLogLines = 256;
    static constexpr int kMaxHistory  = 64;

    Console();

    // --- Command registration ---

    using CommandFn = std::function<void(const std::vector<std::string>& args, Console& self)>;

    /// Register a named console command that the user can invoke.
    void register_command(std::string_view name, std::string_view help_text, CommandFn fn);

    /// Remove a previously registered command.
    void unregister_command(std::string_view name);

    // --- Execution ---

    /// Parse and execute a single line of console input.
    /// Lines starting with '/' are treated as commands; otherwise cvars.
    void execute(std::string_view line);

    /// Print a message to the console log.
    void print(std::string_view message);

    /// Print a formatted message (tagged with a category).
    void print_tagged(std::string_view tag, std::string_view message);

    // --- Log access ---

    struct LogLine {
        std::string text;
        // Simple classification — could extend to severity colours later.
        enum Type { kInfo, kOutput, kError } type {kInfo};
    };

    [[nodiscard]] const LogLine& log_line(int index) const;
    [[nodiscard]] int log_line_count() const;

    // --- Input history ---

    /// Submit a completed input line (history tracking).
    void submit_input(std::string_view line);

    [[nodiscard]] const std::string& history_line(int offset) const;
    [[nodiscard]] int history_count() const;

    // --- Built-in commands ---

    /// Register the standard set of built-in commands (help, echo, cvar list/get/set, clear).
    void register_builtins();

    /// Link to the global ConfigRegistry for cvar get/set commands.
    void set_config_registry(ConfigRegistry* registry);

private:
    // Parse a command line into tokens (space-separated, respects double-quoted strings).
    [[nodiscard]] static std::vector<std::string> tokenize(std::string_view line);

    // Ring buffer for log lines.
    LogLine log_buffer_[kMaxLogLines] {};
    int log_head_ {0};
    int log_count_ {0};

    // Command definitions.
    struct Command {
        std::string name;
        std::string help;
        CommandFn fn;
    };
    std::vector<Command> commands_;

    // Input history ring buffer.
    std::string history_[kMaxHistory] {};
    int history_head_ {0};
    int history_count_ {0};

    // Back-reference to config registry (optional, for cvar commands).
    ConfigRegistry* config_registry_ {nullptr};
};

}  // namespace ae
