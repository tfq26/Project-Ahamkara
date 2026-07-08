#include "ae/core/frame_allocator.h"
#include "ae/core/log.h"

#if defined(_MSC_VER)
#include <malloc.h>
#endif

#include <cstdlib>

#define AE_LOG_CATEGORY "Core"

namespace ae {

FrameAllocator::FrameAllocator(std::size_t arena_size_bytes, int num_slots)
    : capacity_(arena_size_bytes)
    , num_slots_(num_slots)
    , slot_offsets_(static_cast<std::size_t>(num_slots), 0) {
    if (num_slots_ < 1) num_slots_ = 1;
    slot_size_ = capacity_ / static_cast<std::size_t>(num_slots_);
    if (slot_size_ < 64) slot_size_ = 64;  // floor: minimum 64 bytes per slot

    // Adjust capacity to actual allocated amount
    capacity_ = slot_size_ * static_cast<std::size_t>(num_slots_);

    // Round up to alignment so aligned_alloc / _aligned_malloc don't fail
    constexpr std::size_t kAlign = alignof(std::max_align_t);
    std::size_t alloc_size = (capacity_ + kAlign - 1) & ~(kAlign - 1);
    if (alloc_size < capacity_) alloc_size = capacity_;  // overflow guard

#if defined(_MSC_VER)
    memory_ = static_cast<std::uint8_t*>(_aligned_malloc(alloc_size, kAlign));
#else
    memory_ = static_cast<std::uint8_t*>(std::aligned_alloc(kAlign, alloc_size));
#endif
    if (!memory_) {
        log_error_cat(AE_LOG_CATEGORY,
                      "FrameAllocator: aligned_alloc failed for " + std::to_string(capacity_) + " bytes");
    }
}

FrameAllocator::~FrameAllocator() {
#if defined(_MSC_VER)
    _aligned_free(memory_);
#else
    std::free(memory_);
#endif
}

void* FrameAllocator::allocate(std::size_t size, std::size_t alignment) {
    std::size_t& offset = slot_offsets_[static_cast<std::size_t>(current_slot_)];
    std::size_t aligned = (offset + alignment - 1) & ~(alignment - 1);

    if (aligned + size > slot_size_) {
        if (!oom_logged_) {
            log_warning_cat(AE_LOG_CATEGORY,
                            "FrameAllocator OOM (slot " + std::to_string(current_slot_) +
                            "): requested " + std::to_string(size) +
                            " bytes, " + std::to_string(slot_size_ - offset) + " remaining (peak " +
                            std::to_string(peak_used_) + ")");
            oom_logged_ = true;
        }
        return nullptr;
    }

    oom_logged_ = false;
    std::size_t slot_start = static_cast<std::size_t>(current_slot_) * slot_size_;
    void* ptr = memory_ + slot_start + aligned;
    offset = aligned + size;

    std::size_t used_bytes = offset;
    if (used_bytes > peak_used_) peak_used_ = used_bytes;
    return ptr;
}

void FrameAllocator::end_frame() {
    std::size_t cur_offset = slot_offsets_[static_cast<std::size_t>(current_slot_)];
    if (cur_offset > peak_used_) peak_used_ = cur_offset;

    current_slot_ = (current_slot_ + 1) % num_slots_;
    slot_offsets_[static_cast<std::size_t>(current_slot_)] = 0;
    oom_logged_ = false;
}

void FrameAllocator::reset_all() {
    for (auto& off : slot_offsets_) {
        if (off > peak_used_) peak_used_ = off;
        off = 0;
    }
    current_slot_ = 0;
    oom_logged_ = false;
}

}  // namespace ae
