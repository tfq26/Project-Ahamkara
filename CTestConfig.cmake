# Ahamkara CTest Configuration
# =============================
#
# This file enables CTest's built-in memory checking support.
# CTest reads it automatically from the source or build directory
# when running `ctest -T memcheck`.
#
# Usage:
#   ctest -T memcheck --test-dir build/<preset>
#
# This requires Valgrind to be installed on the system.
#
# To override suppression files without modifying this file:
#   ctest -T memcheck --test-dir build/debug \
#     --memorycheck-command valgrind \
#     --memorycheck-options "--suppressions=/custom/path.supp"
#

# Find Valgrind and configure memory checking
find_program(MEMORYCHECK_COMMAND valgrind)

if(MEMORYCHECK_COMMAND)
    # Base Valgrind memcheck flags used by CTest -T memcheck
    set(MEMORYCHECK_COMMAND_OPTIONS
        "--tool=memcheck"
        "--leak-check=full"
        "--show-leak-kinds=definite,indirect,possible"
        "--track-origins=yes"
        "--errors-for-leak-kinds=definite,indirect,possible"
        "--error-exitcode=1"
        "--quiet"
        CACHE STRING "Valgrind memcheck options for CTest"
    )

    # Point to the project's suppression file
    set(_ahamkara_suppressions
        "${CMAKE_SOURCE_DIR}/scripts/valgrind-suppressions.txt"
    )
    if(EXISTS "${_ahamkara_suppressions}")
        set(MEMORYCHECK_SUPPRESSIONS_FILE "${_ahamkara_suppressions}")
        message(STATUS "CTest memcheck: using suppressions from ${_ahamkara_suppressions}")
    else()
        message(WARNING "CTest memcheck: suppressions file not found at ${_ahamkara_suppressions}")
    endif()
endif()
