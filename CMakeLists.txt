cmake_minimum_required(VERSION 3.20)

project(Ahamkara VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# Configure Jolt options before fetching
set(TARGET_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(TARGET_HELLO_WORLD OFF CACHE BOOL "" FORCE)
set(TARGET_PERFORMANCE_TEST OFF CACHE BOOL "" FORCE)
set(TARGET_SAMPLES OFF CACHE BOOL "" FORCE)
set(USE_STATIC_CRT OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    jolt
    GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
    GIT_TAG        v5.0.0
)
FetchContent_MakeAvailable(jolt)

option(AHAMKARA_BUILD_CLIENT "Build the Ahamkara client" ON)
option(AHAMKARA_BUILD_SERVER "Build the Ahamkara dedicated server" ON)
option(AHAMKARA_BUILD_TESTS "Build the Ahamkara test targets" ON)

add_subdirectory(engine/core)
add_subdirectory(engine/network)
add_subdirectory(engine/runtime)
add_subdirectory(game)

if(AHAMKARA_BUILD_CLIENT)
    find_package(OpenGL REQUIRED)
    find_package(glfw3 REQUIRED)
    add_subdirectory(engine/platform)
    add_subdirectory(engine/render)
    add_subdirectory(client)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tools/CMakeLists.txt")
        add_subdirectory(tools)
    endif()
endif()

if(AHAMKARA_BUILD_SERVER)
    add_subdirectory(server)
endif()

if(AHAMKARA_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
    enable_testing()
    add_subdirectory(tests)
endif()
