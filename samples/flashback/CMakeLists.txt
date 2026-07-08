# Flashback: a thin demo over the real engine/game/client stack. It links the
# reusable client runtime, so engine improvements show up here automatically on
# rebuild (no separately-maintained gameplay code to drift).
add_executable(flashback
    src/main.cpp
)

target_compile_features(flashback PRIVATE cxx_std_20)

target_link_libraries(flashback
    PRIVATE
        ahamkara_client_lib
)

set_target_properties(flashback PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/samples/flashback
)
