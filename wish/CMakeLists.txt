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
    session/src/session_group.cpp
    replication/src/replication_frame.cpp
    admin/src/admin_command.cpp
    admin/src/heartbeat_service.cpp
    admin/src/metrics_collector.cpp
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

# SessionGroup unit tests (standalone, no engine deps)
add_executable(session_group_tests
    session/tests/session_group_tests.cpp
)

target_compile_features(session_group_tests PRIVATE cxx_std_20)

target_include_directories(session_group_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(session_group_tests
    PRIVATE
        wish_engine
)

add_test(NAME session_group_tests COMMAND session_group_tests)

# ── Identity service tests ────────────────────────────────────────────────
add_executable(identity_service_tests
    core/tests/identity_service_tests.cpp
)

target_compile_features(identity_service_tests PRIVATE cxx_std_20)

target_include_directories(identity_service_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(identity_service_tests
    PRIVATE
        wish_engine
)

add_test(NAME identity_service_tests COMMAND identity_service_tests)

# ── Roster service tests ──────────────────────────────────────────────────
add_executable(roster_service_tests
    core/tests/roster_service_tests.cpp
)

target_compile_features(roster_service_tests PRIVATE cxx_std_20)

target_include_directories(roster_service_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(roster_service_tests
    PRIVATE
        wish_engine
)

add_test(NAME roster_service_tests COMMAND roster_service_tests)

# ── Invite service tests ──────────────────────────────────────────────────
add_executable(invite_service_tests
    core/tests/invite_service_tests.cpp
)

target_compile_features(invite_service_tests PRIVATE cxx_std_20)

target_include_directories(invite_service_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(invite_service_tests
    PRIVATE
        wish_engine
)

add_test(NAME invite_service_tests COMMAND invite_service_tests)

# ── Validation telemetry tests ────────────────────────────────────────────
add_executable(validation_telemetry_tests
    core/tests/validation_telemetry_tests.cpp
)

target_compile_features(validation_telemetry_tests PRIVATE cxx_std_20)

target_include_directories(validation_telemetry_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(validation_telemetry_tests
    PRIVATE
        wish_engine
)

add_test(NAME validation_telemetry_tests COMMAND validation_telemetry_tests)
