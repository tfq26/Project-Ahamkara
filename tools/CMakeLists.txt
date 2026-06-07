add_executable(ahamkara_controller_mapper
    controller_mapper.cpp
    ../client/src/controller_bindings.cpp
)

add_executable(ahamkara_asset_importer
    asset_importer.cpp
    asset_importer_common.cpp
    asset_importer_manifest.cpp
    asset_importer_registry.cpp
    asset_importer_texture.cpp
    asset_importer_material.cpp
    asset_importer_dispatch.cpp
)

target_compile_features(ahamkara_controller_mapper PRIVATE cxx_std_20)
target_compile_features(ahamkara_asset_importer PRIVATE cxx_std_20)

if(APPLE)
    target_compile_definitions(ahamkara_controller_mapper PRIVATE GL_SILENCE_DEPRECATION)
endif()

target_include_directories(ahamkara_controller_mapper
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../client/include
)

target_link_libraries(ahamkara_controller_mapper
    PRIVATE
        ae_render
)

target_link_libraries(ahamkara_asset_importer
    PRIVATE
        ae_render
)

set_target_properties(ahamkara_controller_mapper PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tools
)

set_target_properties(ahamkara_asset_importer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tools
)
