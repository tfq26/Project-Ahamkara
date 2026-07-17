#include "ae/core/error_types.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>
#include <random>

namespace ae {
namespace {

void copy_capped(char* dst, std::size_t cap, std::string_view src) {
    if (cap == 0) {
        return;
    }
    const std::size_t n = src.size() < (cap - 1) ? src.size() : (cap - 1);
    if (n > 0) {
        std::memcpy(dst, src.data(), n);
    }
    dst[n] = '\0';
}

char hex_digit(std::uint32_t v) {
    constexpr char kHex[] = "0123456789ABCDEF";
    return kHex[v & 0xFu];
}

} // namespace

IncidentId::IncidentId(std::uint64_t value) : value_(value) {
    // Format AAAA-BBBB from low 32 bits.
    const std::uint32_t hi = static_cast<std::uint32_t>((value_ >> 16) & 0xFFFFu);
    const std::uint32_t lo = static_cast<std::uint32_t>(value_ & 0xFFFFu);
    text_[0] = hex_digit(hi >> 12);
    text_[1] = hex_digit(hi >> 8);
    text_[2] = hex_digit(hi >> 4);
    text_[3] = hex_digit(hi);
    text_[4] = '-';
    text_[5] = hex_digit(lo >> 12);
    text_[6] = hex_digit(lo >> 8);
    text_[7] = hex_digit(lo >> 4);
    text_[8] = hex_digit(lo);
    text_[9] = '\0';
}

IncidentId IncidentId::generate() {
    static std::atomic<std::uint64_t> counter {1};
    const auto now = static_cast<std::uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const std::uint64_t mixed = now ^ (counter.fetch_add(1, std::memory_order_relaxed) << 32);
    return IncidentId(mixed == 0 ? 1 : mixed);
}

bool SmallContext::put(std::string_view key, std::string_view value) {
    if (key.empty() || size_ >= kMaxEntries) {
        return false;
    }
    // Update existing key if present.
    for (std::size_t i = 0; i < size_; ++i) {
        if (key == entries_[i].key) {
            copy_capped(entries_[i].value, sizeof(entries_[i].value), value);
            return true;
        }
    }
    copy_capped(entries_[size_].key, sizeof(entries_[size_].key), key);
    copy_capped(entries_[size_].value, sizeof(entries_[size_].value), value);
    ++size_;
    return true;
}

std::optional<std::string_view> SmallContext::get(std::string_view key) const {
    for (std::size_t i = 0; i < size_; ++i) {
        if (key == entries_[i].key) {
            return std::string_view(entries_[i].value);
        }
    }
    return std::nullopt;
}

Error::Error(ErrorCode code, std::source_location loc)
    : code_(code), incident_(IncidentId::generate()), source_(loc) {}

Error::Error(ErrorCode code, IncidentId incident, SmallContext context, std::source_location loc)
    : code_(code), incident_(incident), context_(std::move(context)), source_(loc) {
    if (incident_.empty()) {
        incident_ = IncidentId::generate();
    }
}

Error& Error::with_context(std::string_view key, std::string_view value) {
    context_.put(key, value);
    return *this;
}

Error& Error::caused_by(Error cause) {
    cause_ = std::make_shared<Error>(std::move(cause));
    return *this;
}

Error& Error::with_native(std::string_view native_domain, std::int64_t native_code) {
    copy_capped(native_domain_, sizeof(native_domain_), native_domain);
    native_code_ = native_code;
    return *this;
}

} // namespace ae
