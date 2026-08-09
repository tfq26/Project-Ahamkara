# ── Wish SDK library ────────────────────────────────────────────────────
# This is the game-neutral, standalone Wish platform library.  It must
# NOT link any Ahamkara or Flashback targets — only stdlib and platform
# system libraries.  See docs/architecture/repository-split.md.
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
    admin/src/html_escape.cpp
    admin/src/metrics_collector.cpp
    integrations/nakama/src/nakama_bridge.cpp
    integrations/flashback/src/game_session_adapter.cpp
)

target_compile_features(wish_engine PUBLIC cxx_std_20)

target_include_directories(wish_engine
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)

if(WIN32)
    target_link_libraries(wish_engine PRIVATE ws2_32)
endif()

# ── Install & export rules for the Wish SDK ─────────────────────────────
# Wish is an independent product: it exports its own CMake package so that
# out-of-tree consumers (including Flashback) can find_package(Wish).
include(GNUInstallDirs)

install(TARGETS wish_engine
    EXPORT WishTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Wish
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Wish
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Wish
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    COMPONENT Wish
)

# session_runtime.h lives outside include/ but is referenced by the public
# header session_group.h.  Install it into the same logical path.
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/session/session_runtime.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/wish/session
    COMPONENT Wish
)

install(EXPORT WishTargets
    FILE WishTargets.cmake
    NAMESPACE Wish::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Wish
    COMPONENT Wish
)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/WishConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_SOURCE_DIR}/cmake/WishConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/WishConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Wish
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/WishConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/WishConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Wish
    COMPONENT Wish
)

# ── Admin info page tests ─────────────────────────────────────────────────
add_executable(wish_admin_info_page_tests
    admin/tests/info_page_tests.cpp
    admin/admin_server.cpp
)

target_compile_features(wish_admin_info_page_tests PRIVATE cxx_std_20)

target_include_directories(wish_admin_info_page_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(wish_admin_info_page_tests
    PRIVATE
        wish_engine
)

add_test(NAME wish_admin_info_page_tests COMMAND wish_admin_info_page_tests)

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

# Auth tests (fail-closed, dev mode)
add_executable(wish_auth_tests
    core/tests/auth_tests.cpp
)

target_compile_features(wish_auth_tests PRIVATE cxx_std_20)

target_include_directories(wish_auth_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(wish_auth_tests
    PRIVATE
        wish_engine
)

add_test(NAME wish_auth_tests COMMAND wish_auth_tests)
