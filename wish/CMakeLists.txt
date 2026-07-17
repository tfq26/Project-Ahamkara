add_library(wish_engine STATIC
    core/src/engine_identity.cpp
    core/src/match_runtime.cpp
    core/src/activity_manager.cpp
    core/src/activity_loader.cpp
    core/src/error_envelope.cpp
    core/src/error_catalog.cpp
    core/src/log.cpp
    core/src/sequence_tracker.cpp
    net/src/transport_config.cpp
    session/src/session_model.cpp
    replication/src/replication_frame.cpp
    admin/src/admin_command.cpp
    admin/src/heartbeat_service.cpp
    integrations/nakama/src/nakama_bridge.cpp
)

target_compile_features(wish_engine PUBLIC cxx_std_20)

target_include_directories(wish_engine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

if(WIN32)
    target_link_libraries(wish_engine PRIVATE ws2_32)
endif()

# ── Heartbeat tests ─────────────────────────────────────────────────────
enable_testing()
add_executable(wish_heartbeat_tests
    admin/tests/heartbeat_tests.cpp
)

target_compile_features(wish_heartbeat_tests PRIVATE cxx_std_20)

target_include_directories(wish_heartbeat_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(wish_heartbeat_tests
    PRIVATE
        wish_engine
)

add_test(NAME wish_heartbeat_tests COMMAND wish_heartbeat_tests)
