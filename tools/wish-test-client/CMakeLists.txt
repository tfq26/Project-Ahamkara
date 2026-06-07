add_executable(wish_test_client
    main.cpp
)

target_compile_features(wish_test_client PRIVATE cxx_std_20)

target_link_libraries(wish_test_client
    PRIVATE
        ae_network
        ae_runtime
        ahamkara_game
)

set_target_properties(wish_test_client PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tools
)
