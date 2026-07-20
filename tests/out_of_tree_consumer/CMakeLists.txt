cmake_minimum_required(VERSION 3.20)
project(AhamkaraConsumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Ahamkara CONFIG REQUIRED)

add_executable(ahamkara_consumer main.cpp game_module.h)
target_include_directories(ahamkara_consumer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ahamkara_consumer PRIVATE Ahamkara::Core Ahamkara::Runtime Ahamkara::Network)
