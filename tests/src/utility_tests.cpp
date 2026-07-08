#include "ae/core/cli_utils.h"
#include "ae/core/tick.h"
#include "ae/core/time.h"
#include "ae/core/job_system.h"

#include <cassert>
#include <chrono>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

// ===================================================================
// ae::trim()
// ===================================================================

void test_trim_empty() {
    assert(ae::trim("") == "");
    assert(ae::trim("   ") == "");
    assert(ae::trim("\t\r\n") == "");
    std::cout << "test_trim_empty passed.\n";
}

void test_trim_no_whitespace() {
    assert(ae::trim("hello") == "hello");
    assert(ae::trim("a") == "a");
    assert(ae::trim("no_whitespace_here") == "no_whitespace_here");
    std::cout << "test_trim_no_whitespace passed.\n";
}

void test_trim_leading() {
    assert(ae::trim("  hello") == "hello");
    assert(ae::trim("\t\tleading_tab") == "leading_tab");
    assert(ae::trim("   a") == "a");
    std::cout << "test_trim_leading passed.\n";
}

void test_trim_trailing() {
    assert(ae::trim("hello  ") == "hello");
    assert(ae::trim("trailing_tab\t\t") == "trailing_tab");
    assert(ae::trim("a   ") == "a");
    std::cout << "test_trim_trailing passed.\n";
}

void test_trim_both() {
    assert(ae::trim("  hello  ") == "hello");
    assert(ae::trim("\t middle \t") == "middle");
    assert(ae::trim("   a   ") == "a");
    assert(ae::trim("\n\r  spaced  \r\n") == "spaced");
    std::cout << "test_trim_both passed.\n";
}

void test_trim_preserves_internal() {
    assert(ae::trim("  hello world  ") == "hello world");
    assert(ae::trim("\ta\tb\t") == "a\tb");
    assert(ae::trim("  multi   spaces  ") == "multi   spaces");
    std::cout << "test_trim_preserves_internal passed.\n";
}

// ===================================================================
// ae::compute_frame_dt()
// ===================================================================

void test_compute_frame_dt_basic() {
    auto t = std::chrono::steady_clock::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    float dt = ae::compute_frame_dt(t);
    assert(dt > 0.0F);
    assert(dt < 1.0F);
    std::cout << "test_compute_frame_dt_basic passed.\n";
}

void test_compute_frame_dt_zero_latency() {
    // dt should be very small when called back-to-back on the same timestamp
    auto t = std::chrono::steady_clock::now();
    float dt = ae::compute_frame_dt(t);
    assert(dt >= 0.0F);
    assert(dt < 0.01F);
    std::cout << "test_compute_frame_dt_zero_latency passed.\n";
}

// ===================================================================
// ae::FixedTimestepAccumulator
// ===================================================================

void test_fixed_timestep_accumulator_basic_consumption() {
    ae::FixedTimestepAccumulator acc(1.0 / 60.0);
    acc.begin_frame();
    acc.accumulate(1.0 / 30.0);

    assert(acc.can_consume());
    assert(acc.consume());
    assert(acc.can_consume());
    assert(acc.consume());
    assert(!acc.can_consume());
    assert(acc.steps_this_frame() == 2);
    std::cout << "test_fixed_timestep_accumulator_basic_consumption passed.\n";
}

void test_fixed_timestep_accumulator_interpolation_alpha() {
    ae::FixedTimestepAccumulator acc(1.0 / 60.0);
    acc.begin_frame();
    acc.accumulate(0.010);

    const float alpha_before = acc.interpolation_alpha();
    assert(alpha_before > 0.0F);
    assert(alpha_before < 1.0F);

    assert(!acc.consume()); // not enough accumulated time for a fixed step yet
    std::cout << "test_fixed_timestep_accumulator_interpolation_alpha passed.\n";
}

void test_fixed_timestep_accumulator_reset() {
    ae::FixedTimestepAccumulator acc(1.0 / 60.0);
    acc.begin_frame();
    acc.accumulate(0.05);
    assert(acc.can_consume());
    acc.reset();
    assert(!acc.can_consume());
    assert(acc.steps_this_frame() == 0);
    assert(acc.interpolation_alpha() == 0.0F);
    std::cout << "test_fixed_timestep_accumulator_reset passed.\n";
}

// ===================================================================
// JobSystem — basic
// ===================================================================

void test_job_system_submit_and_wait() {
    ae::JobSystem js;
    js.init(1);

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
    js.init(1);

    bool parent_done = false;
    bool child_done = false;

    auto parent = js.submit([&]() {
        parent_done = true;
    });
    auto child = js.submit_after(parent, [&]() {
        assert(parent_done);  // parent must complete before child runs
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

    // Submit 3 children that all depend on the same parent
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

void test_job_system_mixed_independent_and_dependent() {
    ae::JobSystem js;
    js.init(2);

    int phase = 0;  // 0 = none, 1 = independent done, 2 = dependent done

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

}  // namespace

int main() {
    // trim
    test_trim_empty();
    test_trim_no_whitespace();
    test_trim_leading();
    test_trim_trailing();
    test_trim_both();
    test_trim_preserves_internal();

    // compute_frame_dt
    test_compute_frame_dt_basic();
    test_compute_frame_dt_zero_latency();

    // fixed_timestep_accumulator
    test_fixed_timestep_accumulator_basic_consumption();
    test_fixed_timestep_accumulator_interpolation_alpha();
    test_fixed_timestep_accumulator_reset();

    // job_system
    test_job_system_submit_and_wait();
    test_job_system_submit_after_single_child();
    test_job_system_submit_after_multiple_children();
    test_job_system_mixed_independent_and_dependent();
    test_job_system_wait_all();

    std::cout << "All utility tests passed.\n";
    return 0;
}
