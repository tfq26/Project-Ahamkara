#pragma once

#include <cstdint>
#include <fstream>
#include <limits>
#include <string>
#include <vector>
#include <string_view>

namespace ae::render {

// ============================================================
// Low-level binary I/O helpers for compiled asset formats.
//
// These are minimal wrappers around std::ifstream / std::ofstream
// that centralise error checking so every compiled-* loader and
// saver does not need to re-define them in an anonymous namespace.
// ============================================================

/// Write raw bytes to an output file stream.
inline bool write_bytes(std::ofstream& file, const void* data, std::size_t size) {
    file.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
    return static_cast<bool>(file);
}

/// Write a single value of type T.
template <typename T>
bool write_value(std::ofstream& file, const T& value) {
    return write_bytes(file, &value, sizeof(T));
}

/// Write a contiguous vector of values of type T (raw bytes).
template <typename T>
bool write_vector(std::ofstream& file, const std::vector<T>& values) {
    const auto byte_count = values.size() * sizeof(T);
    if (byte_count == 0) {
        return true;
    }
    return write_bytes(file, values.data(), byte_count);
}

/// Write a length-prefixed string (32-bit length + payload).
inline bool write_string(std::ofstream& file, const std::string& value) {
    const auto size = static_cast<std::uint32_t>(value.size());
    return write_value(file, size) && write_bytes(file, value.data(), value.size());
}

/// Read raw bytes from an input file stream.
inline bool read_bytes(std::ifstream& file, void* data, std::size_t size) {
    file.read(static_cast<char*>(data), static_cast<std::streamsize>(size));
    return static_cast<bool>(file);
}

/// Read a single value of type T.
template <typename T>
bool read_value(std::ifstream& file, T& value) {
    return read_bytes(file, &value, sizeof(T));
}

/// Validate that a count does not exceed a safe upper bound.
inline bool validate_count(std::uint32_t count, std::string_view label, std::string& error,
                           std::uint32_t max_elements = 100'000'000) {
    if (count > max_elements) {
        error = std::string(label) + " count is unreasonably large";
        return false;
    }
    return true;
}

/// Read a contiguous vector of values of type T.
template <typename T>
bool read_vector(std::ifstream& file, std::vector<T>& values, std::uint32_t count,
                 std::string_view label, std::string& error,
                 std::uint32_t max_elements = 100'000'000) {
    if (!validate_count(count, label, error, max_elements)) {
        return false;
    }

    values.resize(count);
    const auto byte_count = values.size() * sizeof(T);
    if (byte_count == 0) {
        return true;
    }

    if (!read_bytes(file, values.data(), byte_count)) {
        error = "Failed to read " + std::string(label) + " data";
        return false;
    }

    return true;
}

/// Read a length-prefixed string (32-bit length + payload).
inline bool read_string(std::ifstream& file, std::string& value, std::string& error,
                        std::uint32_t max_bytes = 1'048'576) {
    std::uint32_t size = 0;
    if (!read_value(file, size)) {
        error = "Failed to read string size";
        return false;
    }

    if (size > max_bytes) {
        error = "String size is unreasonably large";
        return false;
    }

    value.resize(size);
    if (size == 0) {
        return true;
    }

    if (!read_bytes(file, value.data(), size)) {
        error = "Failed to read string data";
        return false;
    }

    return true;
}

/// Safely narrow a std::size_t count to std::uint32_t.
/// Throws std::runtime_error if the value exceeds the representable range.
inline std::uint32_t checked_count(std::size_t value, std::string_view label) {
    if (value > std::numeric_limits<std::uint32_t>::max()) {
        throw std::runtime_error(std::string(label) + " count exceeds uint32 limit");
    }
    return static_cast<std::uint32_t>(value);
}

}  // namespace ae::render
