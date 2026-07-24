# ── Wish independent consumer fixture ────────────────────────────────────────
#
# Proves that Wish builds, links, and runs without any dependency on
# engine/, game/, client/, server/, or samples/flashback/ source trees.
#
# Links only wish_engine — no Ahamkara or Flashback targets.
# Uses only Wish public headers (wish/*).
# ─────────────────────────────────────────────────────────────────────────────

add_executable(wish_consumer_standalone
    main.cpp
)

target_compile_features(wish_consumer_standalone PRIVATE cxx_std_20)

target_link_libraries(wish_consumer_standalone
    PRIVATE
        Wish::wish_engine
)

add_test(NAME wish_consumer_standalone COMMAND wish_consumer_standalone)
