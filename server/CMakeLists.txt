add_executable(ahamkara_server
    src/dedicated_server_main.cpp
    ../wish/admin/admin_server.cpp
)

target_compile_features(ahamkara_server PRIVATE cxx_std_20)
target_include_directories(ahamkara_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(ahamkara_server
    PRIVATE
        ae_network
        ae_runtime
        ahamkara_game
        wish_engine
)

if(TARGET ae_render)
    target_link_libraries(ahamkara_server PRIVATE ae_render)
endif()

set_target_properties(ahamkara_server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/server
)
