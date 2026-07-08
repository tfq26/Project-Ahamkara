add_library(ae_input STATIC
    src/input_map.cpp
)
target_compile_features(ae_input PUBLIC cxx_std_20)
target_include_directories(ae_input PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(ae_input PUBLIC ae_platform ae_core)
