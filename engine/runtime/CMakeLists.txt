add_library(ae_runtime STATIC
    src/application.cpp
    src/free_camera.cpp
    src/metrics.cpp
    src/performance_logger.cpp
    src/streaming_executor.cpp
)

target_compile_features(ae_runtime PUBLIC cxx_std_20)

target_include_directories(ae_runtime
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(ae_runtime
    PUBLIC
        ae_core
)
