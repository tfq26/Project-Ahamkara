add_library(ae_audio STATIC
    src/audio_engine.cpp
)
target_compile_features(ae_audio PUBLIC cxx_std_20)
target_include_directories(ae_audio PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(ae_audio PUBLIC miniaudio_lib ae_core)
