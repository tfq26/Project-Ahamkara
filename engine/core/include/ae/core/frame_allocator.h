#pragma once

#include <cstddef>
#include <cstdint>
#include <new>
#include <vector>

namespace ae {

/**
 * @brief Ring-buffer frame allocator for per-frame temporary allocations.
 *
 * Manages a ring of slots (default 3). Each frame advances to the next slot,
 * resetting its bump pointer. Allocations within a frame are linear bumps.
 * The ring-buffer design ensures data from the previous N-1 frames remains
 * valid while the current frame allocates, supporting double/triple-buffered
 * pipelines.
 *
 * Thread-safe: NO. Use one per thread, or protect externally.
 *
 * Usage:
 * @code
 *   // 12 MB total = 3 slots × 4 MB each
 *   FrameAllocator alloc(12 * 1024 * 1024, 3);
 *
 *   void game_loop() {
 *       alloc.end_frame();  // advance to next slot, reclaim oldest
 *
 *       int* data = alloc.allocate_array<int>(100);
 *       // data is valid until the slot is reclaimed after num_slots frames
 *   }
 * @endcode
 */
class FrameAllocator {
public:
    /**
     * @param arena_size_bytes  Total memory to reserve across all slots.
     * @param num_slots         Number of ring-buffer slots (default 3).
     *                          Each slot gets arena_size_bytes / num_slots.
     */
    explicit FrameAllocator(std::size_t arena_size_bytes, int num_slots = 3);
    ~FrameAllocator();

    FrameAllocator(const FrameAllocator&) = delete;
    FrameAllocator& operator=(const FrameAllocator&) = delete;
    FrameAllocator(FrameAllocator&&) = delete;
    FrameAllocator& operator=(FrameAllocator&&) = delete;

    /**
     * @brief Allocate raw memory from the current slot with given alignment.
     * @return Pointer to allocated memory, or nullptr if the current slot is full.
     */
    [[nodiscard]] void* allocate(std::size_t size, std::size_t alignment = alignof(std::max_align_t));

    /**
     * @brief Allocate an array of T (uninitialized) from the current slot.
     */
    template <typename T>
    [[nodiscard]] T* allocate_array(std::size_t count) {
        return static_cast<T*>(allocate(count * sizeof(T), alignof(T)));
    }

    /**
     * @brief Allocate and default-construct a single T in the current slot.
     */
    template <typename T>
    [[nodiscard]] T* allocate_object() {
        void* mem = allocate(sizeof(T), alignof(T));
        if (mem) return new (mem) T();
        return nullptr;
    }

    /**
     * @brief Advance to the next ring-buffer slot.
     * Call once per frame, before allocating for the new frame.
     */
    void end_frame();

    /**
     * @brief Reset all slots and start from slot 0.
     * Use at initialization or when flushing the entire ring.
     */
    void reset_all();

    /** Bytes used in the current slot. */
    [[nodiscard]] std::size_t used() const { return slot_offsets_[current_slot_]; }

    /** Peak bytes used in any single slot since last reset_all. */
    [[nodiscard]] std::size_t peak_used() const { return peak_used_; }

    /** Total backing memory capacity in bytes. */
    [[nodiscard]] std::size_t capacity() const { return capacity_; }

    /** Size of each individual slot in bytes. */
    [[nodiscard]] std::size_t slot_size() const { return slot_size_; }

    /** Number of ring-buffer slots. */
    [[nodiscard]] int num_slots() const { return num_slots_; }

    /** Current slot index [0, num_slots). */
    [[nodiscard]] int current_slot() const { return current_slot_; }

private:
    std::uint8_t* memory_;
    std::size_t capacity_;
    std::size_t slot_size_;
    int num_slots_;
    int current_slot_{0};
    std::vector<std::size_t> slot_offsets_;  ///< bump offset per slot
    std::size_t peak_used_{0};
    bool oom_logged_{false};
};

}  // namespace ae
