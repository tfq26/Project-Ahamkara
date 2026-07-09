#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "ae/core/telemetry.h"

#define TEST(name)                                    \
    static void test_##name();                        \
    struct Register_##name {                          \
        Register_##name() {                           \
            tests.push_back(&test_##name);            \
            names.push_back(#name);                   \
        }                                             \
    } reg_##name;                                     \
    static void test_##name()

#define RUN_TESTS()                                  \
    do {                                              \
        int passed = 0, failed = 0;                   \
        for (std::size_t i_ = 0; i_ < tests.size(); ++i_) { \
            std::cout << "  " << names[i_] << "... "; \
            try {                                     \
                tests[i_]();                          \
                std::cout << "PASS\n";                \
                ++passed;                             \
            } catch (const std::exception& e) {       \
                std::cout << "FAIL (" << e.what() << ")\n"; \
                ++failed;                             \
            } catch (...) {                           \
                std::cout << "FAIL (unknown)\n";      \
                ++failed;                             \
            }                                         \
        }                                             \
        std::cout << "\n" << passed << "/" << (passed + failed) << " tests passed.\n"; \
        return (failed == 0) ? 0 : 1;                 \
    } while(0)

namespace {
std::vector<void (*)()> tests;
std::vector<const char*> names;

void require(bool cond, const char* msg) {
    if (!cond) throw std::runtime_error(msg);
}
}  // anonymous namespace

// ===================================================================
// Test: counter basic operations
// ===================================================================
TEST(counter_basic) {
    ae::TelemetrySystem::instance().clear();

    ae::TelemetryCounter c("test.counter");
    require(c.peek() == 0, "counter should start at 0");
    require(c.name() == "test.counter", "counter name mismatch");

    c.add();
    require(c.peek() == 1, "counter add() should increment by 1");

    c.add(5);
    require(c.peek() == 6, "counter add(5) should add 5");

    auto val = c.reset();
    require(val == 6, "reset should return current value");
    require(c.peek() == 0, "counter should be 0 after reset");
}

// ===================================================================
// Test: gauge basic operations
// ===================================================================
TEST(gauge_basic) {
    ae::TelemetrySystem::instance().clear();

    ae::TelemetryGauge g("test.gauge");
    require(g.get() == 0, "gauge should start at 0");

    g.set(42);
    require(g.get() == 42, "gauge set(42) should work");

    g.add(-10);
    require(g.get() == 32, "gauge add(-10) should decrease");
}

// ===================================================================
// Test: histogram basic operations
// ===================================================================
TEST(histogram_basic) {
    ae::TelemetrySystem::instance().clear();

    ae::TelemetryHistogram h("test.hist", {1.0, 5.0, 10.0});
    // buckets: <1, [1,5), [5,10), >=10

    h.observe(0.5);  // bucket 0
    h.observe(2.0);  // bucket 1
    h.observe(5.0);  // bucket 2 (5 is not < 5, so bucket index 2)
    h.observe(20.0); // bucket 3 (overflow)

    auto buckets = h.reset();
    require(buckets.size() == 4, "should have 4 buckets");
    require(buckets[0] == 1, "bucket 0 should be 1");
    require(buckets[1] == 1, "bucket 1 should be 1");
    require(buckets[2] == 1, "bucket 2 should be 1");
    require(buckets[3] == 1, "bucket 3 (overflow) should be 1");

    // Verify reset cleared
    auto empty = h.reset();
    for (auto v : empty) {
        require(v == 0, "all buckets should be 0 after reset");
    }
}

// ===================================================================
// Test: telemetry system snapshot
// ===================================================================
TEST(system_snapshot) {
    ae::TelemetrySystem::instance().clear();

    // Create metrics BEFORE snapshot
    ae::TelemetryCounter c("sys.counter");
    ae::TelemetryGauge g("sys.gauge");
    ae::TelemetryHistogram h("sys.hist", {0.5, 1.0});

    c.add(3);
    g.set(100);
    h.observe(0.3);
    h.observe(0.8);

    auto snap = ae::TelemetrySystem::instance().snapshot();

    require(snap.counters.size() == 1, "should have 1 counter");
    require(snap.counters[0].name == "sys.counter", "counter name");
    require(snap.counters[0].value == 3, "counter value");

    require(snap.gauges.size() == 1, "should have 1 gauge");
    require(snap.gauges[0].name == "sys.gauge", "gauge name");
    require(snap.gauges[0].value == 100, "gauge value");

    require(snap.histograms.size() == 1, "should have 1 histogram");
    require(snap.histograms[0].name == "sys.hist", "hist name");
    require(snap.histograms[0].buckets.size() == 3, "3 histogram buckets");
}

// ===================================================================
// Test: CSV flush
// ===================================================================
TEST(csv_flush) {
    ae::TelemetrySystem::instance().clear();

    ae::TelemetryCounter c("csv.counter");
    ae::TelemetryGauge g("csv.gauge");
    ae::TelemetryHistogram h("csv.hist", {1.0});

    c.add(7);
    g.set(99);
    h.observe(0.5);

    auto path = std::filesystem::temp_directory_path() / "telemetry_test.csv";
    ae::TelemetrySystem::instance().flush_to_csv(path.string());

    // Verify file exists and has content
    std::ifstream file(path);
    require(file.is_open(), "CSV file should exist");
    std::string header;
    std::getline(file, header);
    require(header == "type,name,value", "CSV header");

    int lines = 0;
    std::string line;
    while (std::getline(file, line)) ++lines;
    // 1 boundary = 2 buckets, so: 1 counter + 1 gauge + 2 histogram = 4 data lines
    require(lines == 4, "CSV should have 4 data lines (1 counter + 1 gauge + 2 histogram)");

    std::filesystem::remove(path);
}

// ===================================================================
// Test: concurrent counter adds
// ===================================================================
TEST(concurrent_counters) {
    ae::TelemetrySystem::instance().clear();
    ae::TelemetryCounter c("concurrent.counter");

    constexpr int kThreads = 4;
    constexpr int kIterations = 10000;
    std::vector<std::thread> threads;

    for (int i = 0; i < kThreads; ++i) {
        threads.emplace_back([&c]() {
            for (int j = 0; j < kIterations; ++j) {
                c.add(1);
            }
        });
    }

    for (auto& t : threads) t.join();

    require(c.peek() == kThreads * kIterations,
            "concurrent counter should sum correctly");
}

// ===================================================================
// Test: clear system
// ===================================================================
TEST(clear_system) {
    ae::TelemetrySystem::instance().clear();

    {
        ae::TelemetryCounter c("clear.counter");
        ae::TelemetryGauge g("clear.gauge");
    } // metrics go out of scope

    ae::TelemetrySystem::instance().clear();

    auto snap = ae::TelemetrySystem::instance().snapshot();
    require(snap.counters.empty(), "no counters after clear");
    require(snap.gauges.empty(), "no gauges after clear");
    require(snap.histograms.empty(), "no histograms after clear");
}

// ===================================================================
int main() {
    RUN_TESTS();
}
