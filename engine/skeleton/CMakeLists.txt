add_library(ae_skeleton STATIC
    src/evaluate.cpp
)

target_compile_features(ae_skeleton PUBLIC cxx_std_20)

target_include_directories(ae_skeleton
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(ae_skeleton
    PUBLIC
        ae_core
)
