# Out-of-tree Wish SDK consumer smoke test.
#
# Installs the Wish SDK component to a temporary prefix, then configures,
# builds, and runs a tiny external project that uses only find_package(Wish).
#
# Usage:
#   cmake -DSOURCE_DIR=<src> -DBINARY_DIR=<build> -DCONSUMER_SRC=<consumer-path>
#         -P cmake/WishOutOfTreeConsumerTest.cmake

if(NOT DEFINED SOURCE_DIR OR NOT DEFINED BINARY_DIR OR NOT DEFINED CONSUMER_SRC)
    message(FATAL_ERROR "WishOutOfTreeConsumerTest.cmake missing required -D args")
endif()

set(PREFIX "${BINARY_DIR}/wish-package-prefix")
set(CONSUMER_BUILD "${BINARY_DIR}/wish-package-consumer-build")

file(REMOVE_RECURSE "${PREFIX}" "${CONSUMER_BUILD}")

execute_process(
    COMMAND ${CMAKE_COMMAND} --install "${BINARY_DIR}" --prefix "${PREFIX}" --component Wish
    RESULT_VARIABLE install_rc
)
if(NOT install_rc EQUAL 0)
    message(FATAL_ERROR "Failed to install Wish package component (rc=${install_rc})")
endif()

execute_process(
    COMMAND ${CMAKE_COMMAND}
        -S "${CONSUMER_SRC}"
        -B "${CONSUMER_BUILD}"
        -G Ninja
        -DCMAKE_BUILD_TYPE=Debug
        -DCMAKE_PREFIX_PATH=${PREFIX}
    RESULT_VARIABLE config_rc
)
if(NOT config_rc EQUAL 0)
    message(FATAL_ERROR "Failed to configure out-of-tree Wish consumer (rc=${config_rc})")
endif()

execute_process(
    COMMAND ${CMAKE_COMMAND} --build "${CONSUMER_BUILD}"
    RESULT_VARIABLE build_rc
)
if(NOT build_rc EQUAL 0)
    message(FATAL_ERROR "Failed to build out-of-tree Wish consumer (rc=${build_rc})")
endif()

execute_process(
    COMMAND "${CONSUMER_BUILD}/wish_consumer"
    RESULT_VARIABLE run_rc
    OUTPUT_VARIABLE run_out
    ERROR_VARIABLE run_err
)
if(NOT run_rc EQUAL 0)
    message(FATAL_ERROR "Wish consumer run failed (rc=${run_rc})\nstdout:\n${run_out}\nstderr:\n${run_err}")
endif()

message(STATUS "Out-of-tree Wish SDK consumer smoke passed")
