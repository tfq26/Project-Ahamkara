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
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)

if(WIN32)
    target_link_libraries(wish_engine PRIVATE ws2_32)
endif()

# ── Package / Export support ─────────────────────────────────────────────
# Export wish_engine so external consumers can find_package(Wish CONFIG).
install(TARGETS wish_engine
    EXPORT WishTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Ahamkara
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Ahamkara
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT WishTargets
    FILE WishTargets.cmake
    NAMESPACE Wish::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Wish
    COMPONENT Ahamkara
)

include(CMakePackageConfigHelpers)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/WishConfigVersion.cmake"
    VERSION "0.1.0"
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
    COMPONENT Ahamkara
)

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
