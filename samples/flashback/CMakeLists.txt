# Flashback: an independent game built on the Ahamkara engine. It links the
# reusable client runtime and its own game module, so engine improvements show
# up here automatically on rebuild while Flashback owns its game identity.

add_subdirectory(game_module)

add_executable(flashback
    src/main.cpp
)

target_compile_features(flashback PRIVATE cxx_std_20)

target_link_libraries(flashback
    PRIVATE
        ahamkara_client_lib
        flashback_game_module
)

set_target_properties(flashback PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/samples/flashback
)
