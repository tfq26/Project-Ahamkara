cmake_minimum_required(VERSION 3.20)
project(WishConsumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# This consumer proves that the installed Wish SDK can be found and linked
# without any Ahamkara or Flashback source.  It uses only find_package(Wish)
# and Wish::* targets — no sibling source paths.
find_package(Wish CONFIG REQUIRED)

add_executable(wish_consumer main.cpp)
target_link_libraries(wish_consumer PRIVATE Wish::wish_engine)
