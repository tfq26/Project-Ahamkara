add_library(ae_network STATIC
    src/udp_socket.cpp
    src/network_simulator.cpp
    src/network_clock.cpp
    src/sequence_tracker.cpp
)

target_compile_features(ae_network PUBLIC cxx_std_20)

target_include_directories(ae_network
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ae_network
    PUBLIC
        ae_core
)
