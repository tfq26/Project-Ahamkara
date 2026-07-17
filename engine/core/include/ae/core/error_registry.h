#pragma once

#include "ae/core/error_types.h"

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace ae {

/// Compile/runtime registry of immutable error descriptors.
class ErrorRegistry {
  public:
    static ErrorRegistry& instance();

    /// Register descriptors. Returns false if any code is invalid/duplicate.
    bool register_descriptors(std::span<const ErrorDescriptor> descriptors);

    [[nodiscard]] const ErrorDescriptor* find(const ErrorCode& code) const;
    [[nodiscard]] const ErrorDescriptor* find(std::string_view code_text) const;
    [[nodiscard]] std::size_t size() const {
        return descriptors_.size();
    }
    [[nodiscard]] const std::vector<ErrorDescriptor>& all() const {
        return descriptors_;
    }

    /// Validate all registered codes are well-formed and unique.
    [[nodiscard]] bool validate() const;

  private:
    ErrorRegistry() = default;
    std::vector<ErrorDescriptor> descriptors_ {};
};

/// Built-in Ahamkara active codes (initial foundation set).
std::span<const ErrorDescriptor> ahamkara_active_error_descriptors();

// Convenience accessors for foundation codes.
const ErrorCode& ae_cfg_0001(); // configuration load/parse failure
const ErrorCode& ae_ast_0001(); // asset load/compile failure
const ErrorCode& ae_net_0001(); // socket init/bind/connect failure
const ErrorCode& ae_rnd_0001(); // renderer init failure
const ErrorCode& ae_aud_0001(); // audio init failure

} // namespace ae
