add_library(wish_engine STATIC
    core/src/engine_identity.cpp
    core/src/match_runtime.cpp
    core/src/activity_manager.cpp
    core/src/activity_loader.cpp
    net/src/transport_config.cpp
    session/src/session_model.cpp
    replication/src/replication_frame.cpp
    admin/src/admin_command.cpp
    integrations/nakama/src/nakama_bridge.cpp
)

target_compile_features(wish_engine PUBLIC cxx_std_20)

target_include_directories(wish_engine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(wish_engine
    PUBLIC
        ae_core
        ae_network
)
