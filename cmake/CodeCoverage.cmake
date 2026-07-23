# CodeCoverage.cmake
#
# Code coverage instrumentation setup for GCC/Clang.
#
# Usage in CMakeLists.txt:
#   include(cmake/CodeCoverage.cmake)
#   setup_coverage()        # adds --coverage globally when CODE_COVERAGE=ON
#
# Activate with:
#   cmake -DCODE_COVERAGE=ON <other args>
#
# After building and running tests, generate reports with gcovr:
#   gcovr --root . --filter 'engine/' --filter 'client/' --filter 'server/' \
#         --filter 'game/' --filter 'wish/' --filter 'tests/' \
#         --html coverage.html --xml coverage.xml
#
# Or via the convenience script:
#   ./scripts/run-coverage.sh

include_guard(GLOBAL)

option(CODE_COVERAGE "Enable code coverage instrumentation (GCC/Clang)" OFF)

function(setup_coverage)
    if(NOT CODE_COVERAGE)
        return()
    endif()

    # Only GCC and Clang support --coverage
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        message(STATUS "Code coverage: adding --coverage flags for ${CMAKE_CXX_COMPILER_ID}")

        # These flags tell the compiler to instrument the binary for coverage:
        # -fprofile-arcs   : instrument arcs (branches) for gcov
        # -ftest-coverage  : generate .gcno notes files
        # --coverage        : shorthand for both above at compile time
        #                     and links libgcov at link time
        add_compile_options(--coverage)
        add_link_options(--coverage)

        # Instruct gcov to use a relative path prefix so coverage reports
        # map back to source tree paths rather than absolute build paths.
        set(ENV{GCOV_PREFIX_STRIP} "0")
    else()
        message(WARNING "CODE_COVERAGE is ON but compiler "
                        "${CMAKE_CXX_COMPILER_ID} does not support --coverage")
    endif()
endfunction()
