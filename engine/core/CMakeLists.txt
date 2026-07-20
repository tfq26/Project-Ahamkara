add_library(ae_core STATIC
    src/config.cpp
    src/console.cpp
    src/crash_handler.cpp
    src/diagnostics.cpp
    src/error_report.cpp
    src/error_registry.cpp
    src/error_types.cpp
    src/file_watcher.cpp
    src/frame_allocator.cpp
    src/frame_pacer.cpp
    src/frame_profiler.cpp
    src/job_system.cpp
    src/streaming_executor.cpp
    src/log.cpp
    src/memory_budget.cpp
    src/telemetry.cpp
    src/time.cpp
)

target_compile_features(ae_core PUBLIC cxx_std_20)

target_include_directories(ae_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# glm is required to compile public math headers in-tree, but must not be
# re-exported by the installed package (consumers supply their own math stack
# or use non-math core APIs).
target_link_libraries(ae_core
    PUBLIC
        $<BUILD_INTERFACE:glm>
)
