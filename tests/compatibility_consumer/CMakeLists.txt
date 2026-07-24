cmake_minimum_required(VERSION 3.20)

# ==========================================================================
# Compatibility Consumer — clean-room smoke test
# ==========================================================================
#
# This project is configured by CompatibilitySmokeTest.cmake (or invoked
# directly by the compatibility CI workflow) in an environment that has
# pre-built Ahamkara and Wish packages on CMAKE_PREFIX_PATH but NO source
# checkout of either product.

project(CompatibilityConsumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- discover installed products ----
find_package(Ahamkara CONFIG REQUIRED)
find_package(Wish CONFIG REQUIRED)

# ---- consumer smoke executable ----
# The EXPECTED_*_VERSION defines can be overridden via CMAKE to test
# both compatible and incompatible version combinations.
add_executable(compatibility_consumer
    main.cpp
)

target_compile_features(compatibility_consumer PRIVATE cxx_std_20)

target_compile_definitions(compatibility_consumer PRIVATE
    EXPECTED_AE_ABI_VERSION=$<IF:$<BOOL:${AE_ABI_VERSION_OVERRIDE}>,${AE_ABI_VERSION_OVERRIDE},1>
    EXPECTED_WISH_ABI_VERSION=$<IF:$<BOOL:${WISH_ABI_VERSION_OVERRIDE}>,${WISH_ABI_VERSION_OVERRIDE},1>
    EXPECTED_AE_NET_PROTOCOL_VERSION=$<IF:$<BOOL:${AE_NET_PROTOCOL_OVERRIDE}>,${AE_NET_PROTOCOL_OVERRIDE},1>
    EXPECTED_WISH_SESSION_PROTOCOL_VERSION=$<IF:$<BOOL:${WISH_SESSION_PROTOCOL_OVERRIDE}>,${WISH_SESSION_PROTOCOL_OVERRIDE},1>
)

target_link_libraries(compatibility_consumer PRIVATE
    Ahamkara::Core
    Ahamkara::Runtime
    Ahamkara::Network
    Wish::Engine
)

add_test(NAME compatibility_consumer COMMAND compatibility_consumer)

# ---- Flashback headless smoke (server-only path) ----
add_executable(flashback_headless_smoke
    flashback_smoke.cpp
)

target_compile_features(flashback_headless_smoke PRIVATE cxx_std_20)

target_link_libraries(flashback_headless_smoke PRIVATE
    Ahamkara::Core
    Ahamkara::Runtime
    Ahamkara::Network
    Wish::Engine
)

add_test(NAME flashback_headless_smoke COMMAND flashback_headless_smoke)
