add_library(wish_engine STATIC
    core/src/engine_identity.cpp
    core/src/match_runtime.cpp
    core/src/matchmaking_service.cpp
    core/src/activity_manager.cpp
    core/src/activity_loader.cpp
    core/src/error_envelope.cpp
    core/src/error_catalog.cpp
    core/src/observability.cpp
    core/src/log.cpp
    core/src/sequence_tracker.cpp
    core/src/session_orchestrator.cpp
    net/src/transport_config.cpp
    session/src/session_model.cpp
    session/src/session_group.cpp
    session/src/party.cpp
    replication/src/replication_frame.cpp
    admin/src/admin_command.cpp
    admin/src/heartbeat_service.cpp
    admin/src/metrics_collector.cpp
    integrations/nakama/src/nakama_bridge.cpp
    integrations/flashback/src/game_session_adapter.cpp
)

target_compile_features(wish_engine PUBLIC cxx_std_20)

target_include_directories(wish_engine
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
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

# Party unit tests
add_executable(party_tests
    session/tests/party_tests.cpp
)

target_compile_features(party_tests PRIVATE cxx_std_20)

target_include_directories(party_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(party_tests
    PRIVATE
        wish_engine
)

add_test(NAME party_tests COMMAND party_tests)

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

# SessionOrchestrator unit tests
add_executable(session_orchestrator_tests
    core/tests/session_orchestrator_tests.cpp
)

target_compile_features(session_orchestrator_tests PRIVATE cxx_std_20)

target_include_directories(session_orchestrator_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(session_orchestrator_tests
    PRIVATE
        wish_engine
)

add_test(NAME session_orchestrator_tests COMMAND session_orchestrator_tests)
