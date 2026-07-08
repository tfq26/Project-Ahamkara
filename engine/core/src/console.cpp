#include "ae/core/console.h"
#include "ae/core/log.h"

#include <algorithm>
#include <cctype>
#include <sstream>

#define AE_LOG_CATEGORY "Console"

namespace ae {

Console::Console() {
    // Pre-fill log with a welcome message.
    print_tagged("Console", "Ahamkara developer console. Type 'help' for commands.");
}

// ============================================================
// Command registration
// ============================================================

void Console::register_command(std::string_view name, std::string_view help_text, CommandFn fn) {
    // Check for duplicate.
    auto it = std::find_if(commands_.begin(), commands_.end(),
        [name](const Command& c) { return c.name == name; });
    if (it != commands_.end()) {
        print_tagged("Console", "Warning: command '" + std::string(name) + "' already registered, overwriting.");
        it->help = std::string(help_text);
        it->fn = std::move(fn);
        return;
    }
    commands_.push_back({std::string(name), std::string(help_text), std::move(fn)});
    log_debug_cat(AE_LOG_CATEGORY, "Registered console command: " + std::string(name));
}

void Console::unregister_command(std::string_view name) {
    auto it = std::remove_if(commands_.begin(), commands_.end(),
        [name](const Command& c) { return c.name == name; });
    commands_.erase(it, commands_.end());
}

// ============================================================
// Execution
// ============================================================

void Console::execute(std::string_view line) {
    if (line.empty()) return;

    print_tagged(">", line);
    submit_input(line);

    auto tokens = tokenize(line);
    if (tokens.empty()) return;

    const std::string& cmd_name = tokens[0];

    // First, try registered commands.
    auto it = std::find_if(commands_.begin(), commands_.end(),
        [&cmd_name](const Command& c) { return c.name == cmd_name; });
    if (it != commands_.end()) {
        // Strip the command name from args.
        std::vector<std::string> args(tokens.begin() + 1, tokens.end());
        it->fn(args, *this);
        return;
    }

    // Second, try ConfigRegistry cvar lookup.
    if (config_registry_ != nullptr) {
        if (tokens.size() == 1) {
            // Single token: try to get the cvar value.
            auto val = config_registry_->get_value(cmd_name);
            if (!val.empty()) {
                print(cmd_name + " = " + val);
                return;
            }
            // Also try setting with an implicit default (if we somehow know it).
            print_tagged("Console", "Unknown command: '" + cmd_name + "'. "
                         "Type 'help' for commands, 'cvar_list' for variables.");
        } else {
            // Multiple tokens: treat as cvar set.
            std::string value = tokens[1];
            for (std::size_t i = 2; i < tokens.size(); ++i) {
                value += " " + tokens[i];
            }
            if (config_registry_->set_value(cmd_name, value)) {
                print_tagged("Console", "Set " + cmd_name + " = " + value);
            } else {
                print_tagged("Console", "Unknown cvar: '" + cmd_name + "'");
            }
        }
        return;
    }

    print_tagged("Console", "Unknown command: '" + cmd_name + "'. Type 'help' for available commands.");
}

void Console::print(std::string_view message) {
    LogLine& slot = log_buffer_[log_head_];
    slot.text = std::string(message);
    slot.type = LogLine::kOutput;
    log_head_ = (log_head_ + 1) % kMaxLogLines;
    if (log_count_ < kMaxLogLines) {
        ++log_count_;
    }
}

void Console::print_tagged(std::string_view tag, std::string_view message) {
    LogLine& slot = log_buffer_[log_head_];
    slot.text = "[" + std::string(tag) + "] " + std::string(message);
    slot.type = LogLine::kInfo;
    log_head_ = (log_head_ + 1) % kMaxLogLines;
    if (log_count_ < kMaxLogLines) {
        ++log_count_;
    }
}

// ============================================================
// Log access
// ============================================================

const Console::LogLine& Console::log_line(int index) const {
    // index 0 = oldest, index count-1 = newest
    const int offset = (log_head_ - log_count_ + index + kMaxLogLines) % kMaxLogLines;
    return log_buffer_[offset];
}

int Console::log_line_count() const {
    return log_count_;
}

// ============================================================
// Input history
// ============================================================

void Console::submit_input(std::string_view line) {
    history_[history_head_] = std::string(line);
    history_head_ = (history_head_ + 1) % kMaxHistory;
    if (history_count_ < kMaxHistory) {
        ++history_count_;
    }
}

const std::string& Console::history_line(int offset) const {
    // offset 0 = oldest, offset count-1 = newest
    const int idx = (history_head_ - history_count_ + offset + kMaxHistory) % kMaxHistory;
    return history_[idx];
}

int Console::history_count() const {
    return history_count_;
}

// ============================================================
// Built-in command registration
// ============================================================

void Console::set_config_registry(ConfigRegistry* registry) {
    config_registry_ = registry;
}

void Console::register_builtins() {
    // help
    register_command("help", "Show available commands and cvars",
        [this](const std::vector<std::string>&, Console& self) {
            self.print("=== Available Console Commands ===");
            for (const auto& cmd : commands_) {
                self.print("  " + cmd.name + " - " + cmd.help);
            }
            if (config_registry_ != nullptr) {
                self.print("  (ConfigRegistry linked — use 'cvar_list', 'cvar_get', 'cvar_set')");
            }
        });

    // echo
    register_command("echo", "Print the arguments back to the console",
        [](const std::vector<std::string>& args, Console& self) {
            std::string out;
            for (std::size_t i = 0; i < args.size(); ++i) {
                if (i > 0) out += " ";
                out += args[i];
            }
            self.print(out);
        });

    // clear
    register_command("clear", "Clear the console log",
        [this](const std::vector<std::string>&, Console& self) {
            log_head_ = 0;
            log_count_ = 0;
            self.print_tagged("Console", "Console cleared.");
        });

    // cvar_list
    register_command("cvar_list", "List all registered config variables",
        [this](const std::vector<std::string>&, Console& self) {
            if (config_registry_ == nullptr) {
                self.print_tagged("Console", "No ConfigRegistry linked.");
                return;
            }
            auto keys = config_registry_->all_keys();
            if (keys.empty()) {
                self.print_tagged("Console", "No config variables registered.");
                return;
            }
            self.print("=== Registered Config Variables (" + std::to_string(keys.size()) + ") ===");
            for (const auto& k : keys) {
                self.print("  " + k + " = " + config_registry_->get_value(k));
            }
        });

    // cvar_get
    register_command("cvar_get", "Read a config variable: cvar_get <name>",
        [this](const std::vector<std::string>& args, Console& self) {
            if (args.empty()) {
                self.print_tagged("Console", "Usage: cvar_get <name>");
                return;
            }
            if (config_registry_ == nullptr) {
                self.print_tagged("Console", "No ConfigRegistry linked.");
                return;
            }
            auto val = config_registry_->get_value(args[0]);
            if (val.empty()) {
                self.print_tagged("Console", "Unknown cvar: '" + args[0] + "'");
            } else {
                self.print(args[0] + " = " + val);
            }
        });

    // cvar_set
    register_command("cvar_set", "Set a config variable: cvar_set <name> <value>",
        [this](const std::vector<std::string>& args, Console& self) {
            if (args.size() < 2) {
                self.print_tagged("Console", "Usage: cvar_set <name> <value>");
                return;
            }
            // Reconstruct value string from remaining tokens.
            std::string value = args[1];
            for (std::size_t i = 2; i < args.size(); ++i) {
                value += " " + args[i];
            }
            if (config_registry_ == nullptr) {
                self.print_tagged("Console", "No ConfigRegistry linked. Cannot set cvars.");
                return;
            }
            if (config_registry_->set_value(args[0], value)) {
                self.print_tagged("Console", "Set " + args[0] + " = " + value);
            } else {
                self.print_tagged("Console", "Unknown cvar: '" + args[0] + "'");
            }
        });

    log_info_cat(AE_LOG_CATEGORY, "Built-in console commands registered.");
}

// ============================================================
// Tokenizer
// ============================================================

std::vector<std::string> Console::tokenize(std::string_view line) {
    std::vector<std::string> tokens;
    const char* s = line.data();
    const char* end = s + line.size();

    while (s < end) {
        // Skip whitespace.
        while (s < end && std::isspace(static_cast<unsigned char>(*s))) ++s;
        if (s >= end) break;

        if (*s == '"') {
            // Quoted string.
            ++s;
            const char* start = s;
            while (s < end && *s != '"') ++s;
            tokens.emplace_back(start, static_cast<std::size_t>(s - start));
            if (s < end) ++s;
        } else {
            // Unquoted token.
            const char* start = s;
            while (s < end && !std::isspace(static_cast<unsigned char>(*s))) ++s;
            tokens.emplace_back(start, static_cast<std::size_t>(s - start));
        }
    }
    return tokens;
}

}  // namespace ae
