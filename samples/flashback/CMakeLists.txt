add_executable(flashback
    src/main.cpp
)

target_compile_features(flashback PRIVATE cxx_std_20)

target_link_libraries(flashback
    PRIVATE
        ae_runtime
        ae_render
)

set_target_properties(flashback PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/samples/flashback
)
