add_executable(ahamkara_smoke_tests
    src/network_smoke_tests.cpp
    src/camera_smoke_tests.cpp
    src/local_play_tests.cpp
    ../client/src/local_play.cpp
)

add_executable(ahamkara_world_tests
    src/world_tests.cpp
)

target_compile_features(ahamkara_smoke_tests PRIVATE cxx_std_20)
target_compile_features(ahamkara_world_tests PRIVATE cxx_std_20)

target_include_directories(ahamkara_smoke_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../client/include
)

target_link_libraries(ahamkara_smoke_tests
    PRIVATE
        ae_core
        ae_runtime
        ahamkara_game
)

target_link_libraries(ahamkara_world_tests
    PRIVATE
        ae_core
        ae_runtime
        ahamkara_game
)

add_test(NAME ahamkara_smoke_tests COMMAND ahamkara_smoke_tests)
add_test(NAME ahamkara_world_tests COMMAND ahamkara_world_tests)
