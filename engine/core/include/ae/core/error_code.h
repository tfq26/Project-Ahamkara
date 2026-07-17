#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace ae {

/// Stable, human-searchable error identity: PRODUCT-DOMAIN-NUMBER
/// Example: AE-NET-1004
class ErrorCode {
public:
    static constexpr std::size_t kMaxText = 31;

    constexpr ErrorCode() = default;
    explicit constexpr ErrorCode(std::string_view text) { assign(text); }

    [[nodiscard]] constexpr bool valid() const { return length_ > 0; }
    [[nodiscard]] constexpr std::string_view text() const {
        return std::string_view(text_.data(), length_);
    }
    [[nodiscard]] std::string str() const { return std::string(text()); }

    [[nodiscard]] constexpr std::string_view product() const { return part(0); }
    [[nodiscard]] constexpr std::string_view domain() const { return part(1); }
    [[nodiscard]] constexpr std::uint16_t number() const {
        const auto n = part(2);
        std::uint16_t value = 0;
        for (char c : n) {
            if (c < '0' || c > '9') {
                return 0;
            }
            value = static_cast<std::uint16_t>(value * 10 + (c - '0'));
        }
        return value;
    }

    [[nodiscard]] static constexpr bool is_well_formed(std::string_view text) {
        // PRODUCT-DOMAIN-NUMBER where PRODUCT/DOMAIN are A-Z and NUMBER is 1-4 digits.
        if (text.empty() || text.size() > kMaxText) {
            return false;
        }
        std::size_t dash1 = text.find('-');
        if (dash1 == std::string_view::npos || dash1 == 0) {
            return false;
        }
        std::size_t dash2 = text.find('-', dash1 + 1);
        if (dash2 == std::string_view::npos || dash2 <= dash1 + 1 || dash2 + 1 >= text.size()) {
            return false;
        }
        auto product = text.substr(0, dash1);
        auto domain = text.substr(dash1 + 1, dash2 - dash1 - 1);
        auto number = text.substr(dash2 + 1);
        if (product.empty() || domain.empty() || number.empty() || number.size() > 4) {
            return false;
        }
        for (char c : product) {
            if (c < 'A' || c > 'Z') return false;
        }
        for (char c : domain) {
            if (c < 'A' || c > 'Z') return false;
        }
        for (char c : number) {
            if (c < '0' || c > '9') return false;
        }
        return true;
    }

    [[nodiscard]] friend constexpr bool operator==(const ErrorCode& a, const ErrorCode& b) {
        return a.text() == b.text();
    }
    [[nodiscard]] friend constexpr bool operator!=(const ErrorCode& a, const ErrorCode& b) {
        return !(a == b);
    }

private:
    constexpr void assign(std::string_view text) {
        length_ = 0;
        if (!is_well_formed(text)) {
            return;
        }
        length_ = text.size();
        for (std::size_t i = 0; i < length_; ++i) {
            text_[i] = text[i];
        }
    }

    [[nodiscard]] constexpr std::string_view part(std::size_t index) const {
        if (!valid()) {
            return {};
        }
        std::string_view all = text();
        std::size_t start = 0;
        std::size_t count = 0;
        for (std::size_t i = 0; i <= all.size(); ++i) {
            if (i == all.size() || all[i] == '-') {
                if (count == index) {
                    return all.substr(start, i - start);
                }
                start = i + 1;
                ++count;
            }
        }
        return {};
    }

    std::array<char, kMaxText> text_{};
    std::size_t length_{0};
};

} // namespace ae
