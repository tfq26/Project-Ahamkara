add_executable(ahamkara_controller_mapper
    controller_mapper.cpp
    ../client/src/controller_bindings.cpp
)

target_compile_features(ahamkara_controller_mapper PRIVATE cxx_std_20)

if(APPLE)
    target_compile_definitions(ahamkara_controller_mapper PRIVATE GL_SILENCE_DEPRECATION)
endif()

target_include_directories(ahamkara_controller_mapper
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../client/include
)

target_link_libraries(ahamkara_controller_mapper
    PRIVATE
        ae_core
        ae_platform
        ae_render
)

set_target_properties(ahamkara_controller_mapper PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tools
)
