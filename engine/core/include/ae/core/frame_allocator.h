#pragma once

#include <cstddef>
#include <cstdint>
#include <new>

namespace ae {

/**
 * @brief A simple bump/arena allocator for per-frame temporary allocations.
 *
 * Allocates by bumping a pointer. O(1) allocation, O(1) reset.
 * Reset at the start of each frame to reclaim all memory at once.
 *
 * Thread-safe: NO. Use one per thread, or protect externally.
 *
 * Usage:
 * @code
 *   FrameAllocator alloc(4 * 1024 * 1024); // 4 MB arena
 *   void game_loop() {
 *       alloc.reset();
 *       // ... temp allocations this frame ...
 *       int* data = alloc.allocate_array<int>(100);
 *       // data is valid until next reset()
 *   }
 * @endcode
 */
class FrameAllocator {
public:
    explicit FrameAllocator(std::size_t arena_size_bytes);
    ~FrameAllocator();

    FrameAllocator(const FrameAllocator&) = delete;
    FrameAllocator& operator=(const FrameAllocator&) = delete;
    FrameAllocator(FrameAllocator&&) = delete;
    FrameAllocator& operator=(FrameAllocator&&) = delete;

    /**
     * @brief Allocate raw memory with given alignment.
     * @return Pointer to allocated memory, or nullptr if out of space.
     */
    [[nodiscard]] void* allocate(std::size_t size, std::size_t alignment = alignof(std::max_align_t));

    /**
     * @brief Allocate an array of T (uninitialized).
     */
    template <typename T>
    [[nodiscard]] T* allocate_array(std::size_t count) {
        return static_cast<T*>(allocate(count * sizeof(T), alignof(T)));
    }

    /**
     * @brief Allocate and default-construct a single T.
     */
    template <typename T>
    [[nodiscard]] T* allocate_object() {
        void* mem = allocate(sizeof(T), alignof(T));
        if (mem) return new (mem) T();
        return nullptr;
    }

    /** Reset the bump pointer to the start. Call once per frame. */
    void reset();

    /** Bytes currently used. */
    [[nodiscard]] std::size_t used() const { return offset_; }

    /** Peak bytes used since last reset. */
    [[nodiscard]] std::size_t peak_used() const { return peak_used_; }

    /** Total arena capacity in bytes. */
    [[nodiscard]] std::size_t capacity() const { return capacity_; }

private:
    std::uint8_t* memory_;
    std::size_t capacity_;
    std::size_t offset_{0};
    std::size_t peak_used_{0};
    bool oom_logged_{false};
};

}  // namespace ae
