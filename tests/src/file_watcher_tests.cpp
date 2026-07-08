#include "ae/core/file_watcher.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <chrono>

// Simple test framework macros
#define TEST(name)                                   \
    do {                                             \
        printf("  TEST: %s ... ", name);              \
        fflush(stdout);

#define END_TEST(result)                             \
        printf(result ? "PASSED\n" : "FAILED\n");    \
        if (!(result)) { failures++; }               \
    } while(0)

static int failures = 0;

static std::string test_dir() {
    return std::filesystem::temp_directory_path().string() + "/ahamkara_file_watcher_test";
}

static void ensure_dir() {
    std::filesystem::create_directories(test_dir());
}

static void cleanup_dir() {
    std::filesystem::remove_all(test_dir());
}

static void write_file(const std::string& name, const std::string& content) {
    std::ofstream f(test_dir() + "/" + name);
    f << content;
}

static std::string file_path(const std::string& name) {
    return test_dir() + "/" + name;
}

// ============================================================
// FileWatcher tests
// ============================================================

static void test_watch_new_file() {
    cleanup_dir();
    ensure_dir();

    ae::FileWatcher fw;
    bool changed = false;

    fw.watch(file_path("test1.txt"), [&changed](const std::string&) {
        changed = true;
    });

    // Poll — file doesn't exist yet
    int n = fw.poll();
    TEST("poll on nonexistent file returns 0");
    END_TEST(n == 0);
    TEST("callback not called for nonexistent file");
    END_TEST(!changed);

    // Create the file
    write_file("test1.txt", "hello");

    // Poll — should detect the new file
    n = fw.poll();
    TEST("poll detects new file");
    END_TEST(n == 1 && changed);

    cleanup_dir();
}

static void test_watch_modify_file() {
    cleanup_dir();
    ensure_dir();
    write_file("test2.txt", "original");

    ae::FileWatcher fw;
    int change_count = 0;

    fw.watch(file_path("test2.txt"), [&change_count](const std::string&) {
        ++change_count;
    });

    // Initial poll — should trigger callback for initial appearance
    fw.poll();
    int first_count = change_count;

    // Modify the file
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    write_file("test2.txt", "modified");

    // Poll — should detect the change
    fw.poll();
    TEST("poll detects modified file");
    END_TEST(change_count > first_count);

    cleanup_dir();
}

static void test_watch_multiple_files() {
    cleanup_dir();
    ensure_dir();

    ae::FileWatcher fw;
    int changes = 0;

    fw.watch(file_path("multi_a.txt"), [&changes](const std::string&) { ++changes; });
    fw.watch(file_path("multi_b.txt"), [&changes](const std::string&) { ++changes; });

    // Create both files
    write_file("multi_a.txt", "a");
    write_file("multi_b.txt", "b");

    // Poll — should detect both
    int n = fw.poll();
    TEST("poll detects multiple files simultaneously");
    END_TEST(n == 2 && changes == 2);

    cleanup_dir();
}

static void test_unwatch() {
    cleanup_dir();
    ensure_dir();

    ae::FileWatcher fw;
    int changes = 0;

    fw.watch(file_path("unwatch_test.txt"), [&changes](const std::string&) { ++changes; });
    write_file("unwatch_test.txt", "data");

    fw.poll(); // initial detection
    int after_first_poll = changes;

    fw.unwatch(file_path("unwatch_test.txt"));

    write_file("unwatch_test.txt", "modified");
    fw.poll(); // should not trigger since we unwatched

    TEST("unwatch stops change detection");
    END_TEST(changes == after_first_poll);

    TEST("watched_count drops after unwatch");
    END_TEST(fw.watched_count() == 0);

    cleanup_dir();
}

static void test_clear() {
    cleanup_dir();
    ensure_dir();

    ae::FileWatcher fw;
    int changes = 0;

    fw.watch(file_path("clear_test.txt"), [&changes](const std::string&) { ++changes; });
    write_file("clear_test.txt", "data");
    fw.poll();

    fw.clear();
    write_file("clear_test.txt", "modified");
    fw.poll();

    TEST("clear resets all watchers");
    END_TEST(fw.watched_count() == 0 && changes == 1); // only the initial detection

    cleanup_dir();
}

static void test_watch_count() {
    ae::FileWatcher fw;

    TEST("initially empty");
    END_TEST(fw.watched_count() == 0);

    fw.watch("/tmp/dummy1.txt", [](const std::string&) {});
    fw.watch("/tmp/dummy2.txt", [](const std::string&) {});

    TEST("watched_count after adding files");
    END_TEST(fw.watched_count() == 2);

    fw.clear();
    TEST("watched_count after clear");
    END_TEST(fw.watched_count() == 0);
}

// ============================================================
// Main
// ============================================================

int main() {
    printf("FileWatcher Tests\n");
    printf("=================\n\n");

    test_watch_new_file();
    test_watch_modify_file();
    test_watch_multiple_files();
    test_unwatch();
    test_clear();
    test_watch_count();

    cleanup_dir();

    printf("\n");
    if (failures > 0) {
        printf("*** %d TEST(S) FAILED ***\n", failures);
        return 1;
    }
    printf("All tests passed.\n");
    return 0;
}
