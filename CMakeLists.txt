cmake_minimum_required(VERSION 3.20)

if(POLICY CMP0177)
    cmake_policy(SET CMP0177 NEW)
    set(CMAKE_POLICY_DEFAULT_CMP0177 NEW)
endif()

project(Ahamkara VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

if(WIN32)
    add_compile_definitions(NOMINMAX)
    # Force all targets to use dynamic CRT (MD/MDd) to avoid RuntimeLibrary
    # mismatches with FetchContent dependencies like JoltPhysics.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Sanitizers (AddressSanitizer + UndefinedBehaviorSanitizer)
# ---------------------------------------------------------------------------
option(AHAMKARA_ENABLE_SANITIZE "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

if(AHAMKARA_ENABLE_SANITIZE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address,undefined -fno-omit-frame-pointer")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address,undefined")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=address,undefined")
    else()
        message(WARNING "AHAMKARA_ENABLE_SANITIZE is ON but the compiler (${CMAKE_CXX_COMPILER_ID}) does not support -fsanitize. Ignoring.")
    endif()
endif()

include(FetchContent)
# Do not recursively install FetchContent dependencies with cmake --install.
set(CMAKE_SKIP_INSTALL_ALL_DEPENDENCY ON)

# Configure Jolt options before fetching
set(TARGET_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(TARGET_HELLO_WORLD OFF CACHE BOOL "" FORCE)
set(TARGET_PERFORMANCE_TEST OFF CACHE BOOL "" FORCE)
set(TARGET_SAMPLES OFF CACHE BOOL "" FORCE)
# NOTE: Jolt v5.0.0 uses USE_STATIC_MSVC_RUNTIME_LIBRARY, not USE_STATIC_CRT
set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF CACHE BOOL "" FORCE)
set(ENABLE_ALL_WARNINGS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    jolt
    GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
    GIT_TAG        v5.0.0
    SOURCE_SUBDIR  Build
)

# GLM
set(GLM_TEST_ENABLE OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
)

# EnTT
set(ENTT_BUILD_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    entt
    GIT_REPOSITORY https://github.com/skypjack/entt.git
    GIT_TAG        v3.13.0
)

# miniaudio
FetchContent_Declare(
    miniaudio
    GIT_REPOSITORY https://github.com/mackron/miniaudio.git
    GIT_TAG        master
)

# glm/entt/miniaudio are general engine deps; Jolt is only required for
# physics/collision/game paths and can be skipped for pure package export.
FetchContent_MakeAvailable(glm entt miniaudio)
if(NOT AHAMKARA_ENGINE_ONLY)
    FetchContent_MakeAvailable(jolt)
endif()

# Force-fix Jolt CRT on Windows in case its internal CMake overrode our setting
if(WIN32 AND TARGET Jolt)
    set_property(TARGET Jolt PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

option(AHAMKARA_BUILD_WISH_ADMIN "Build the Wish admin HTTP server (requires Lua/sol2)" OFF)

if(AHAMKARA_BUILD_WISH_ADMIN)
    # Lua (CMake-compatible repo wrapper)
    FetchContent_Declare(
        lua
        GIT_REPOSITORY https://github.com/marovira/lua.git
        GIT_TAG        master
    )

    # sol2 (C++ bindings for Lua)
    set(SOL2_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        sol2
        GIT_REPOSITORY https://github.com/ThePhD/sol2.git
        GIT_TAG        v3.3.0
    )

    FetchContent_MakeAvailable(lua sol2)
endif()

# Jolt v5.0.0 uses memset/memcpy on non-trivially-copyable types which
# triggers -Wnontrivial-memcall under Apple Clang.  Jolt enables -Werror
# so we need to suppress this specific diagnostic on the Jolt target.
if(TARGET Jolt AND CMAKE_CXX_COMPILER_ID MATCHES "AppleClang|Clang")
    target_compile_options(Jolt PRIVATE -Wno-nontrivial-memcall)
endif()

# Create an interface library for miniaudio so targets can link to it easily
add_library(miniaudio_lib INTERFACE)
target_include_directories(miniaudio_lib INTERFACE ${miniaudio_SOURCE_DIR})

option(AHAMKARA_BUILD_CLIENT "Build the Ahamkara client" ON)
option(AHAMKARA_BUILD_SERVER "Build the Ahamkara dedicated server" ON)
option(AHAMKARA_BUILD_TESTS "Build the Ahamkara test targets" ON)
option(AHAMKARA_BUILD_SAMPLES "Build sample games that consume the engine" ON)
option(AHAMKARA_BUILD_WISH "Build Wish backend modules (not part of pure engine package)" ON)
option(AHAMKARA_BUILD_GAME "Build Flashback/gameplay library (not part of pure engine package)" ON)
option(AHAMKARA_ENGINE_ONLY "Configure pure engine targets only (disables wish/game/client/server/samples)" OFF)

if(AHAMKARA_ENGINE_ONLY)
    set(AHAMKARA_BUILD_WISH OFF CACHE BOOL "" FORCE)
    set(AHAMKARA_BUILD_GAME OFF CACHE BOOL "" FORCE)
    set(AHAMKARA_BUILD_CLIENT OFF CACHE BOOL "" FORCE)
    set(AHAMKARA_BUILD_SERVER OFF CACHE BOOL "" FORCE)
    set(AHAMKARA_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
endif()

add_subdirectory(engine/core)
add_subdirectory(engine/network)
add_subdirectory(engine/runtime)
# Collision/physics require Jolt and are excluded from pure engine-only package builds.
if(NOT AHAMKARA_ENGINE_ONLY)
    add_subdirectory(engine/collision)
    add_subdirectory(engine/physics)
endif()

# ae_skeleton and ae_animation are headless (no GLFW/OpenGL deps); they must
# be built before game/ which depends on animation headers transitively.
# Excluded from engine-only packages since game code is outside the engine boundary.
if(NOT AHAMKARA_ENGINE_ONLY)
    add_subdirectory(engine/skeleton)
    add_subdirectory(engine/animation)
endif()

# Product modules live outside the pure Ahamkara engine package.
if(AHAMKARA_BUILD_WISH)
    add_subdirectory(wish)
endif()

# Wish consumer fixture — proves Wish builds, links, and runs without Ahamkara or Flashback.
if(AHAMKARA_BUILD_WISH AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/wish_consumer/CMakeLists.txt")
    enable_testing()
    add_subdirectory(tests/wish_consumer)
endif()
if(AHAMKARA_BUILD_GAME)
    if(NOT AHAMKARA_BUILD_WISH)
        message(FATAL_ERROR "AHAMKARA_BUILD_GAME requires AHAMKARA_BUILD_WISH (game links wish_engine).")
    endif()
    add_subdirectory(game)
endif()

if(AHAMKARA_BUILD_CLIENT)
    find_package(OpenGL REQUIRED)
    find_package(glfw3 REQUIRED)
    add_subdirectory(engine/platform)
    add_subdirectory(engine/render)
    add_subdirectory(engine/ui)
    add_subdirectory(engine/input)
    add_subdirectory(engine/audio)
    add_subdirectory(client)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tools/CMakeLists.txt")
        add_subdirectory(tools)
    endif()
    if(AHAMKARA_BUILD_TESTS)
        enable_testing()
    endif()
    if(AHAMKARA_BUILD_SAMPLES AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/samples/flashback/CMakeLists.txt")
        add_subdirectory(samples/flashback)
    endif()
endif()

if(AHAMKARA_BUILD_SERVER)
    add_subdirectory(server)
endif()

if(AHAMKARA_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
    add_subdirectory(tests)
endif()

if(AHAMKARA_BUILD_GAME AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tools/wish-test-client/CMakeLists.txt")
    add_subdirectory(tools/wish-test-client)
endif()

# ---------------------------------------------------------------------------
# Packaging
# ---------------------------------------------------------------------------
set(CPACK_PACKAGE_NAME "Ahamkara")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VENDOR "Ahamkara")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Ahamkara game engine")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/user/ahamkara")

set(CPACK_GENERATOR "TGZ;ZIP")

set(CPACK_ARCHIVE_COMPONENT_INSTALL OFF)

include(CPack)

# Install rules (libraries, headers, binaries)
include(cmake/InstallRules.cmake)
