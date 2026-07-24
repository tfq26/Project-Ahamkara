# GenerateManifest.cmake
#
# Generates a JSON artifact manifest for the named product and writes it to
# the build directory.  The manifest is automatically included in CPack
# archives via install().
#
# Usage:
#   include(cmake/GenerateManifest.cmake)
#   add_artifact_manifest(
#       TARGET      ahamkara_server
#       PRODUCT     "Ahamkara"
#       VERSION     "${PROJECT_VERSION}"
#       ABI_VERSION 1
#       NAMESPACE   ae
#       OUTPUT      "${CMAKE_BINARY_DIR}/ahamkara-manifest.json"
#   )
#
# The manifest contains:
#   - product name, version, build codename
#   - git commit SHA (or "unknown")
#   - build date (UTC, ISO-8601)
#   - toolchain info (compiler id + version, C++ standard)
#   - ABI / protocol version
#   - list of installed files with SHA-256 checksums
#
# Installation requires a separate install() call for the output file.

include(GNUInstallDirs)

function(add_artifact_manifest)
    set(_options)
    set(_one_value KEYWORDS_MISSING_VALUES
        TARGET PRODUCT VERSION ABI_VERSION NAMESPACE OUTPUT
    )
    set(_multi_value "")
    cmake_parse_arguments(_am "${_options}" "${_one_value}" "${_multi_value}" ${ARGN})

    if(NOT _am_PRODUCT OR NOT _am_OUTPUT)
        message(FATAL_ERROR "add_artifact_manifest: PRODUCT and OUTPUT are required")
    endif()

    # ---- resolve git commit ----
    set(_git_sha "unknown")
    if(EXISTS "${CMAKE_SOURCE_DIR}/.git")
        execute_process(
            COMMAND git rev-parse --short HEAD
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            OUTPUT_VARIABLE _git_sha
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
            TIMEOUT 5
        )
    endif()

    # ---- resolve compiler info ----
    set(_compiler_id    "${CMAKE_CXX_COMPILER_ID}")
    set(_compiler_ver   "${CMAKE_CXX_COMPILER_VERSION}")
    set(_cxx_std        "${CMAKE_CXX_STANDARD}")

    # ---- build date in UTC ----
    string(TIMESTAMP _build_date "%Y-%m-%dT%H:%M:%SZ" UTC)

    # ---- collect files that will be installed ----
    # We capture the install manifest that CMake generates.
    set(_install_manifest "${CMAKE_BINARY_DIR}/install_manifest.txt")

    # ---- generate JSON ----
    set(_abi_version "${_am_ABI_VERSION}")
    if(NOT _abi_version)
        set(_abi_version "0")
    endif()

    set(_namespace "${_am_NAMESPACE}")
    if(NOT _namespace)
        set(_namespace "unknown")
    endif()

    set(_product    "${_am_PRODUCT}")
    set(_version    "${_am_VERSION}")
    if(NOT _version)
        set(_version "0.0.0")
    endif()

    configure_file(
        "${CMAKE_SOURCE_DIR}/cmake/manifest.json.in"
        "${_am_OUTPUT}"
        @ONLY
    )

    message(STATUS "Generated artifact manifest: ${_am_OUTPUT}")

    # ---- custom target so we can add a dependency ----
    if(_am_TARGET)
        add_custom_target("${_am_TARGET}_manifest" ALL
            DEPENDS "${_am_OUTPUT}"
            COMMENT "Generating manifest for ${_am_PRODUCT}"
        )
        add_dependencies(${_am_TARGET} "${_am_TARGET}_manifest")
    endif()

    # ---- install the manifest alongside the package ----
    get_filename_component(_out_dir "${_am_OUTPUT}" DIRECTORY)
    install(
        FILES "${_am_OUTPUT}"
        DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/${_namespace}"
        COMPONENT Ahamkara
    )
endfunction()
