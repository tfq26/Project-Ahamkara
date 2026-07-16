if(NOT DEFINED SOURCE_DIR OR NOT DEFINED BINARY_DIR OR NOT DEFINED CONSUMER_SRC)
    message(FATAL_ERROR "OutOfTreeConsumerTest.cmake missing required -D args")
endif()

set(PREFIX "${BINARY_DIR}/ahamkara-package-prefix")
set(CONSUMER_BUILD "${BINARY_DIR}/ahamkara-package-consumer-build")

file(REMOVE_RECURSE "${PREFIX}" "${CONSUMER_BUILD}")

execute_process(
    COMMAND ${CMAKE_COMMAND} --install "${BINARY_DIR}" --prefix "${PREFIX}" --component Ahamkara
    RESULT_VARIABLE install_rc
)
if(NOT install_rc EQUAL 0)
    message(FATAL_ERROR "Failed to install Ahamkara package component (rc=${install_rc})")
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
    message(FATAL_ERROR "Failed to configure out-of-tree consumer (rc=${config_rc})")
endif()

execute_process(
    COMMAND ${CMAKE_COMMAND} --build "${CONSUMER_BUILD}"
    RESULT_VARIABLE build_rc
)
if(NOT build_rc EQUAL 0)
    message(FATAL_ERROR "Failed to build out-of-tree consumer (rc=${build_rc})")
endif()

execute_process(
    COMMAND "${CONSUMER_BUILD}/ahamkara_consumer"
    RESULT_VARIABLE run_rc
    OUTPUT_VARIABLE run_out
    ERROR_VARIABLE run_err
)
if(NOT run_rc EQUAL 0)
    message(FATAL_ERROR "Consumer run failed (rc=${run_rc})\nstdout:\n${run_out}\nstderr:\n${run_err}")
endif()

message(STATUS "Out-of-tree Ahamkara package consumer smoke passed")
