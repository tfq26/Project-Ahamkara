add_library(ae_core STATIC
    src/config.cpp
    src/console.cpp
    src/crash_handler.cpp
    src/diagnostics.cpp
    src/file_watcher.cpp
    src/frame_allocator.cpp
    src/job_system.cpp
    src/log.cpp
    src/telemetry.cpp
    src/time.cpp
)

target_compile_features(ae_core PUBLIC cxx_std_20)

target_include_directories(ae_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(ae_core
    PUBLIC
        glm
)
