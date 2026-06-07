#include "ae/core/frame_allocator.h"

#include <cstdlib>

namespace ae {

FrameAllocator::FrameAllocator(std::size_t arena_size_bytes)
    : memory_(static_cast<std::uint8_t*>(std::aligned_alloc(alignof(std::max_align_t), arena_size_bytes)))
    , capacity_(arena_size_bytes) {
}

FrameAllocator::~FrameAllocator() {
    std::free(memory_);
}

void* FrameAllocator::allocate(std::size_t size, std::size_t alignment) {
    // Align the current offset
    std::size_t aligned = (offset_ + alignment - 1) & ~(alignment - 1);
    if (aligned + size > capacity_) {
        return nullptr;  // Out of memory
    }
    void* ptr = memory_ + aligned;
    offset_ = aligned + size;
    std::size_t used_bytes = offset_;
    if (used_bytes > peak_used_) peak_used_ = used_bytes;
    return ptr;
}

void FrameAllocator::reset() {
    if (offset_ > peak_used_) peak_used_ = offset_;
    offset_ = 0;
}

}  // namespace ae
