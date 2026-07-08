#include "ae/core/job_system.h"
#include "ae/core/frame_allocator.h"
#include "ae/core/log.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <iostream>
#include <numeric>
#include <string>
#include <thread>
#include <vector>

namespace {

// ===================================================================
// JobSystem tests
// ===================================================================

void test_job_system_submit_and_wait() {
    ae::JobSystem js;
    js.init(2);

    int counter = 0;
    auto handle = js.submit([&]() {
        counter = 42;
    });
    js.wait(handle);

    assert(counter == 42);
    js.shutdown();
    std::cout << "test_job_system_submit_and_wait passed.\n";
}

void test_job_system_submit_after_single_child() {
    ae::JobSystem js;
    js.init(2);

    bool parent_done = false;
    bool child_done = false;

    auto parent = js.submit([&]() {
        parent_done = true;
    });
    auto child = js.submit_after(parent, [&]() {
        assert(parent_done);
        child_done = true;
    });

    js.wait(child);
    assert(child_done);
    js.shutdown();
    std::cout << "test_job_system_submit_after_single_child passed.\n";
}

void test_job_system_submit_after_multiple_children() {
    ae::JobSystem js;
    js.init(2);

    bool parent_done = false;
    int child_count = 0;

    auto parent = js.submit([&]() {
        parent_done = true;
    });

    for (int i = 0; i < 3; ++i) {
        [[maybe_unused]] auto h = js.submit_after(parent, [&]() {
            assert(parent_done);
            ++child_count;
        });
    }

    js.wait_all();
    assert(parent_done);
    assert(child_count == 3);
    js.shutdown();
    std::cout << "test_job_system_submit_after_multiple_children passed.\n";
}

void test_job_system_submit_after_all() {
    ae::JobSystem js;
    js.init(2);

    int a = 0, b = 0, c = 0;
    auto ha = js.submit([&]() { a = 1; });
    auto hb = js.submit([&]() { b = 2; });
    auto hc = js.submit_after_all({ha, hb}, [&]() {
        assert(a == 1);
        assert(b == 2);
        c = a + b;
    });

    js.wait(hc);
    assert(c == 3);
    js.shutdown();
    std::cout << "test_job_system_submit_after_all passed.\n";
}

void test_job_system_submit_after_all_single_parent() {
    ae::JobSystem js;
    js.init(2);

    int value = 0;
    auto parent = js.submit([&]() { value = 10; });
    auto child = js.submit_after_all({parent}, [&]() {
        assert(value == 10);
        value *= 2;
    });

    js.wait(child);
    assert(value == 20);
    js.shutdown();
    std::cout << "test_job_system_submit_after_all_single_parent passed.\n";
}

void test_job_system_submit_after_all_empty_parents() {
    ae::JobSystem js;
    js.init(2);

    int value = 0;
    auto h = js.submit_after_all({}, [&]() { value = 99; });

    js.wait(h);
    assert(value == 99);
    js.shutdown();
    std::cout << "test_job_system_submit_after_all_empty_parents passed.\n";
}

void test_job_system_dispatch() {
    ae::JobSystem js;
    js.init(2);

    constexpr int kCount = 100;
    std::vector<int> results(kCount, 0);

    auto handles = js.dispatch(kCount, [&](int i) {
        results[static_cast<std::size_t>(i)] = i * i + 1;
    });

    assert(handles.size() == static_cast<std::size_t>(kCount));
    for (auto& h : handles) {
        js.wait(h);
    }

    for (int i = 0; i < kCount; ++i) {
        assert(results[static_cast<std::size_t>(i)] == i * i + 1);
    }
    js.shutdown();
    std::cout << "test_job_system_dispatch passed.\n";
}

void test_job_system_wait_all() {
    ae::JobSystem js;
    js.init(2);

    std::vector<int> results(10, 0);
    for (int i = 0; i < 10; ++i) {
        [[maybe_unused]] auto h = js.submit([&results, i]() {
            results[static_cast<std::size_t>(i)] = i * i;
        });
    }
    js.wait_all();

    for (int i = 0; i < 10; ++i) {
        assert(results[static_cast<std::size_t>(i)] == i * i);
    }
    js.shutdown();
    std::cout << "test_job_system_wait_all passed.\n";
}

void test_job_system_mixed_independent_and_dependent() {
    ae::JobSystem js;
    js.init(2);

    int phase = 0;

    auto dependent = js.submit_after(
        js.submit([&]() { phase = 1; }),
        [&]() {
            assert(phase == 1);
            phase = 2;
        });

    auto independent = js.submit([&]() {
        // This job has no parent; may run before or after dependent
    });

    js.wait(dependent);
    js.wait(independent);

    assert(phase == 2);
    js.shutdown();
    std::cout << "test_job_system_mixed_independent_and_dependent passed.\n";
}

void test_job_system_chained_dependencies() {
    ae::JobSystem js;
    js.init(2);

    std::vector<int> order;

    auto h1 = js.submit([&]() { order.push_back(1); });
    auto h2 = js.submit_after(h1, [&]() { order.push_back(2); });
    auto h3 = js.submit_after(h2, [&]() { order.push_back(3); });

    js.wait(h3);
    assert(order.size() == 3);
    assert(order[0] == 1);
    assert(order[1] == 2);
    assert(order[2] == 3);
    js.shutdown();
    std::cout << "test_job_system_chained_dependencies passed.\n";
}

void test_job_system_stress() {
    ae::JobSystem js;
    js.init(4);

    constexpr int kNumJobs = 1000;
    std::vector<int> counters(kNumJobs, 0);

    auto h = js.dispatch(kNumJobs, [&](int i) {
        counters[static_cast<std::size_t>(i)] = i;
        // Simulate some work
        volatile int sum = 0;
        for (int j = 0; j < 100; ++j) {
            sum += j;
        }
        (void)sum;
    });

    for (auto& handle : h) {
        js.wait(handle);
    }

    for (int i = 0; i < kNumJobs; ++i) {
        assert(counters[static_cast<std::size_t>(i)] == i);
    }
    js.shutdown();
    std::cout << "test_job_system_stress passed.\n";
}

void test_job_system_diamond_dependency() {
    ae::JobSystem js;
    js.init(2);

    // A diamond dependency graph:
    //   root
    //   /  \
    // left right
    //   \  /
    //   merged
    int root_done = 0, left_done = 0, right_done = 0, merged_done = 0;

    auto root = js.submit([&]() { root_done = 1; });
    auto left = js.submit_after(root, [&]() {
        assert(root_done == 1);
        left_done = 1;
    });
    auto right = js.submit_after(root, [&]() {
        assert(root_done == 1);
        right_done = 1;
    });
    auto merged = js.submit_after_all({left, right}, [&]() {
        assert(left_done == 1);
        assert(right_done == 1);
        merged_done = 1;
    });

    js.wait(merged);
    assert(merged_done == 1);
    js.shutdown();
    std::cout << "test_job_system_diamond_dependency passed.\n";
}

// ===================================================================
// FrameAllocator tests
// ===================================================================

void test_frame_allocator_basic_alloc() {
    ae::FrameAllocator alloc(1024, 2);  // 2 slots, 512 bytes each
    alloc.end_frame();  // start in slot 1

    void* p1 = alloc.allocate(64);
    assert(p1 != nullptr);
    assert(alloc.used() == 64);

    void* p2 = alloc.allocate(128);
    assert(p2 != nullptr);
    assert(alloc.used() == 64 + 128);

    // End frame and allocate in next slot
    alloc.end_frame();
    void* p3 = alloc.allocate(32);
    assert(p3 != nullptr);
    assert(alloc.used() == 32);
    // p1/p2 should still be readable until slot 0 gets recycled

    alloc.end_frame();  // wrap to slot 0 again — now p1/p2 region is recycled
    std::cout << "test_frame_allocator_basic_alloc passed.\n";
}

void test_frame_allocator_alignment() {
    ae::FrameAllocator alloc(1024, 1);  // single slot
    alloc.end_frame();

    void* p1 = alloc.allocate(1, 1);
    assert(p1 != nullptr);

    // Aligned to 16 bytes
    void* p2 = alloc.allocate(16, 16);
    assert(p2 != nullptr);
    assert((reinterpret_cast<std::uintptr_t>(p2) & 0xF) == 0);

    // Aligned to 64 bytes (cache line)
    void* p3 = alloc.allocate(64, 64);
    assert(p3 != nullptr);
    assert((reinterpret_cast<std::uintptr_t>(p3) & 0x3F) == 0);

    // Aligned to 128 bytes
    void* p4 = alloc.allocate(128, 128);
    assert(p4 != nullptr);
    assert((reinterpret_cast<std::uintptr_t>(p4) & 0x7F) == 0);

    std::cout << "test_frame_allocator_alignment passed.\n";
}

void test_frame_allocator_oom() {
    ae::FrameAllocator alloc(128, 1);
    alloc.end_frame();

    void* p = alloc.allocate(64);
    assert(p != nullptr);

    // Second 64-byte alloc fits (bumped from 0 → 64, next at 64 → 128)
    p = alloc.allocate(64);
    assert(p != nullptr);

    // Third alloc should fail — only 128 bytes per slot
    p = alloc.allocate(1);
    assert(p == nullptr);

    std::cout << "test_frame_allocator_oom passed.\n";
}

void test_frame_allocator_array_and_object() {
    ae::FrameAllocator alloc(1024, 1);
    alloc.end_frame();

    int* arr = alloc.allocate_array<int>(10);
    assert(arr != nullptr);
    for (int i = 0; i < 10; ++i) {
        arr[i] = i * 2;
    }
    for (int i = 0; i < 10; ++i) {
        assert(arr[i] == i * 2);
    }

    struct Point {
        float x, y, z;
        Point() : x(1), y(2), z(3) {}
    };
    Point* pt = alloc.allocate_object<Point>();
    assert(pt != nullptr);
    assert(pt->x == 1.0F);
    assert(pt->y == 2.0F);
    assert(pt->z == 3.0F);

    std::cout << "test_frame_allocator_array_and_object passed.\n";
}

void test_frame_allocator_reset_all() {
    ae::FrameAllocator alloc(512, 2);
    alloc.end_frame();

    [[maybe_unused]] auto p1 = alloc.allocate(64);
    alloc.end_frame();
    [[maybe_unused]] auto p2 = alloc.allocate(128);

    // Should have non-zero usage
    assert(alloc.used() == 128);

    alloc.reset_all();
    assert(alloc.used() == 0);
    assert(alloc.current_slot() == 0);

    // Allocation works after reset
    void* p3 = alloc.allocate(32);
    assert(p3 != nullptr);
    assert(alloc.used() == 32);

    std::cout << "test_frame_allocator_reset_all passed.\n";
}

void test_frame_allocator_peak_usage() {
    ae::FrameAllocator alloc(1024, 2);
    alloc.end_frame();

    // Use 200 in slot 1
    alloc.allocate(200);
    assert(alloc.peak_used() >= 200);

    alloc.end_frame();  // slot 0, 0 used
    alloc.allocate(150);
    assert(alloc.peak_used() >= 200);

    alloc.end_frame();  // slot 1 again
    alloc.allocate(100);
    assert(alloc.peak_used() >= 200);

    alloc.reset_all();
    assert(alloc.peak_used() >= 200);  // peak preserved through reset_all

    std::cout << "test_frame_allocator_peak_usage passed.\n";
}

void test_frame_allocator_slot_rotation() {
    ae::FrameAllocator alloc(300, 3);  // 3 slots, 100 bytes each
    alloc.end_frame();  // slot 1

    // Slot 1: fill 40 bytes
    alloc.allocate(40);
    assert(alloc.current_slot() == 1);
    assert(alloc.used() == 40);

    alloc.end_frame();  // slot 2
    assert(alloc.used() == 0);  // fresh slot

    alloc.allocate(60);
    assert(alloc.used() == 60);

    alloc.end_frame();  // slot 0
    alloc.allocate(80);
    assert(alloc.used() == 80);

    // Rotate back: slot 1 should be reset
    alloc.end_frame();  // slot 1
    assert(alloc.used() == 0);  // reset by end_frame

    std::cout << "test_frame_allocator_slot_rotation passed.\n";
}

void test_frame_allocator_slot_capacity() {
    ae::FrameAllocator alloc(192, 3);
    alloc.end_frame();

    // Each slot has 64 bytes (192/3)
    assert(alloc.slot_size() == 64);
    assert(alloc.num_slots() == 3);
    assert(alloc.capacity() == 192);

    // Fill the slot
    alloc.allocate(64);
    assert(alloc.allocate(1) == nullptr);  // OOM in this slot

    // Switching slots should give us fresh capacity
    alloc.end_frame();
    alloc.allocate(64);
    assert(alloc.used() == 64);

    std::cout << "test_frame_allocator_slot_capacity passed.\n";
}

}  // namespace

int main() {
    // Set log level to Error to avoid noise during tests
    ae::set_log_level(ae::LogLevel::Error);

    std::cout << "--- JobSystem Tests ---\n";
    test_job_system_submit_and_wait();
    test_job_system_submit_after_single_child();
    test_job_system_submit_after_multiple_children();
    test_job_system_submit_after_all();
    test_job_system_submit_after_all_single_parent();
    test_job_system_submit_after_all_empty_parents();
    test_job_system_dispatch();
    test_job_system_wait_all();
    test_job_system_mixed_independent_and_dependent();
    test_job_system_chained_dependencies();
    test_job_system_stress();
    test_job_system_diamond_dependency();

    std::cout << "\n--- FrameAllocator Tests ---\n";
    test_frame_allocator_basic_alloc();
    test_frame_allocator_alignment();
    test_frame_allocator_oom();
    test_frame_allocator_array_and_object();
    test_frame_allocator_reset_all();
    test_frame_allocator_peak_usage();
    test_frame_allocator_slot_rotation();
    test_frame_allocator_slot_capacity();

    std::cout << "\nAll core tests passed.\n";
    return 0;
}
