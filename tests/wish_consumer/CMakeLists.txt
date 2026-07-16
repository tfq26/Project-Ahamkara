add_executable(wish_consumer
    main.cpp
)

target_compile_features(wish_consumer PRIVATE cxx_std_20)

target_include_directories(wish_consumer
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../..
        ${CMAKE_CURRENT_SOURCE_DIR}/../../wish/include
)

target_link_libraries(wish_consumer
    PRIVATE
        wish_engine
)

add_test(NAME wish_consumer COMMAND wish_consumer)
