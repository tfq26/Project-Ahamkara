add_library(ae_core STATIC
    src/log.cpp
    src/time.cpp
)

target_compile_features(ae_core PUBLIC cxx_std_20)

target_include_directories(ae_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

