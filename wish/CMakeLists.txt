# ── Wish Engine ──────────────────────────────────────────────────────────────
# Standalone CMake project.  Also compatible with add_subdirectory() from a
# parent project such as the Ahamkara monorepo (root CMakeLists.txt).
#
# Builds without engine/, game/, client/, server/, or samples/flashback/
# source trees.  Public headers import no ahamkara/* or Flashback/game
# headers.  See docs/wish/architecture.md for the boundary contract.
# ─────────────────────────────────────────────────────────────────────────────

cmake_minimum_required(VERSION 3.20)

# ── Version (used whether standalone or subdirectory) ────────────────────────
set(WISH_VERSION 0.1.0)

# ── Project declaration ─────────────────────────────────────────────────────
# Detect standalone vs subdirectory build.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(_WISH_STANDALONE TRUE)
    project(Wish VERSION ${WISH_VERSION} LANGUAGES CXX)

    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    option(WISH_BUILD_TESTS "Build Wish tests" ON)
else()
    set(_WISH_STANDALONE FALSE)
    # Consumed via add_subdirectory() — inherit C++ standard from parent.
    if(NOT DEFINED WISH_BUILD_TESTS)
        set(WISH_BUILD_TESTS ${BUILD_TESTING})
    endif()
endif()

# ── Library target ──────────────────────────────────────────────────────────

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
)

if(WIN32)
    target_link_libraries(wish_engine PRIVATE ws2_32)
endif()

# ── Namespaced alias ────────────────────────────────────────────────────────
# Enables Wish::wish_engine consumption from both in-tree and installed use.

add_library(Wish::wish_engine ALIAS wish_engine)

# ── Tests ───────────────────────────────────────────────────────────────────

if(WISH_BUILD_TESTS)
    include(CTest)
    enable_testing()

    # Heartbeat tests
    add_executable(wish_heartbeat_tests
        admin/tests/heartbeat_tests.cpp
    )
    target_compile_features(wish_heartbeat_tests PRIVATE cxx_std_20)
    target_include_directories(wish_heartbeat_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_link_libraries(wish_heartbeat_tests
        PRIVATE
            wish_engine
    )
    add_test(NAME wish_heartbeat_tests COMMAND wish_heartbeat_tests)

    # SessionGroup unit tests
    add_executable(session_group_tests
        session/tests/session_group_tests.cpp
    )
    target_compile_features(session_group_tests PRIVATE cxx_std_20)
    target_include_directories(session_group_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_link_libraries(session_group_tests
        PRIVATE
            wish_engine
    )
    add_test(NAME session_group_tests COMMAND session_group_tests)

    # Independent consumer fixture — proves no Ahamkara/Flashback deps.
    add_subdirectory(tests/consumer)
endif()

# ── Install / Package (standalone only) ─────────────────────────────────────
# When built as a subdirectory the parent project (Ahamkara) owns the install
# rules via cmake/InstallRules.cmake.

if(_WISH_STANDALONE)
    include(GNUInstallDirs)

    install(TARGETS wish_engine
        EXPORT WishTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    install(DIRECTORY include/wish
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.h"
    )

    # ── CMake package config for find_package(Wish CONFIG) ──────────────────

    include(CMakePackageConfigHelpers)

    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/WishConfigVersion.cmake"
        VERSION ${WISH_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/WishConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/WishConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Wish
    )

    install(EXPORT WishTargets
        FILE WishTargets.cmake
        NAMESPACE Wish::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Wish
    )

    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/WishConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/WishConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Wish
    )
endif()
