#include "ae/core/frame_allocator.h"
#include "ae/core/log.h"

#if defined(_MSC_VER)
#include <malloc.h>
#endif

#include <cstdlib>

#define AE_LOG_CATEGORY "Core"

namespace ae {

FrameAllocator::FrameAllocator(std::size_t arena_size_bytes)
    : capacity_(arena_size_bytes) {
#if defined(_MSC_VER)
    memory_ = static_cast<std::uint8_t*>(_aligned_malloc(arena_size_bytes, alignof(std::max_align_t)));
#else
    memory_ = static_cast<std::uint8_t*>(std::aligned_alloc(alignof(std::max_align_t), arena_size_bytes));
#endif
    if (!memory_) {
        log_error_cat(AE_LOG_CATEGORY,
                      "FrameAllocator: aligned_alloc failed for " + std::to_string(arena_size_bytes) + " bytes");
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
    // Align the current offset
    std::size_t aligned = (offset_ + alignment - 1) & ~(alignment - 1);
    if (aligned + size > capacity_) {
        if (!oom_logged_) {
            log_warning_cat(AE_LOG_CATEGORY,
                            "FrameAllocator OOM: requested " + std::to_string(size) +
                            " bytes, " + std::to_string(capacity_ - offset_) + " remaining (peak " +
                            std::to_string(peak_used_) + ")");
            oom_logged_ = true;
        }
        return nullptr;  // Out of memory
    }
    oom_logged_ = false;
    void* ptr = memory_ + aligned;
    offset_ = aligned + size;
    std::size_t used_bytes = offset_;
    if (used_bytes > peak_used_) peak_used_ = used_bytes;
    return ptr;
}

void FrameAllocator::reset() {
    if (offset_ > peak_used_) peak_used_ = offset_;
    offset_ = 0;
    oom_logged_ = false;
}

}  // namespace ae
