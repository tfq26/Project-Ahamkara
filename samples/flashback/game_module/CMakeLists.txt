# Flashback game module — independent game module implementing the GameModule
# contract. Consumed by the Flashback executable and tested in isolation.

add_library(flashback_game_module STATIC
    src/game_module.cpp
    src/flashback_error_catalog.cpp
    src/flashback_error_presentation.cpp
)

target_compile_features(flashback_game_module PUBLIC cxx_std_20)

target_include_directories(flashback_game_module
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(flashback_game_module
    PUBLIC
        ae_runtime
        ae_core
)

# ── Smoke test ──────────────────────────────────────────────────────────────
add_executable(flashback_game_module_tests
    tests/game_module_tests.cpp
)

target_compile_features(flashback_game_module_tests PRIVATE cxx_std_20)

target_include_directories(flashback_game_module_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(flashback_game_module_tests
    PRIVATE
        flashback_game_module
        ae_runtime
        ae_core
)

add_test(NAME flashback_game_module_tests COMMAND flashback_game_module_tests)

# ── Flashback error system tests ─────────────────────────────────────────────
add_executable(flashback_error_tests
    tests/flashback_error_tests.cpp
)

target_compile_features(flashback_error_tests PRIVATE cxx_std_20)

target_include_directories(flashback_error_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(flashback_error_tests
    PRIVATE
        flashback_game_module
        ae_runtime
        ae_core
)

add_test(NAME flashback_error_tests COMMAND flashback_error_tests)
