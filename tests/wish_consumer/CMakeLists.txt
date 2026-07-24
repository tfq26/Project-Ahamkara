add_executable(wish_consumer
    main.cpp
)

target_compile_features(wish_consumer PRIVATE cxx_std_20)

# No repo-root or Ahamkara include paths — this fixture proves Wish
# builds and links independently of engine/, game/, client/, server/,
# and samples/flashback/ source trees.
target_link_libraries(wish_consumer
    PRIVATE
        wish_engine
)

add_test(NAME wish_consumer COMMAND wish_consumer)
