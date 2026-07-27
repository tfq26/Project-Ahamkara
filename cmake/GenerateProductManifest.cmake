# GenerateProductManifest
# ========================
# Writes a JSON product manifest into the build directory at configure time.
# The manifest records version, git commit, toolchain info, ABI/protocol level,
# and dependency pins.
#
# Usage:
#   include(cmake/GenerateProductManifest.cmake)
#   generate_product_manifest(
#     PRODUCT_NAME   "Ahamkara Engine"    # Human-readable product name
#     ABI_MAJOR      1                    # ABI major version
#     ABI_MINOR      0                    # ABI minor version
#     ABI_PATCH      0                    # ABI patch version
#     [PROTOCOL_MAJOR  1]                 # Protocol major version (optional)
#     [PROTOCOL_MINOR  0]                 # Protocol minor version (optional)
#     OUTPUT_FILE    <path>               # Absolute path to the JSON output
#   )
#
# The manifest is (re)generated every time CMake configures, so it always
# reflects the current build configuration.

function(generate_product_manifest)
    set(options "")
    set(one_value_args PRODUCT_NAME ABI_MAJOR ABI_MINOR ABI_PATCH PROTOCOL_MAJOR PROTOCOL_MINOR OUTPUT_FILE)
    set(multi_value_args "")
    cmake_parse_arguments(GPM "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})

    if(NOT GPM_PRODUCT_NAME OR NOT GPM_OUTPUT_FILE)
        message(FATAL_ERROR "generate_product_manifest: PRODUCT_NAME, OUTPUT_FILE required")
    endif()

    # --- Gather build-time metadata -------------------------------------------
    # Product version (from project())
    if(PROJECT_VERSION)
        set(_manifest_version "${PROJECT_VERSION}")
    else()
        set(_manifest_version "0.0.0")
    endif()

    # Git commit (short hash)
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E git rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE _git_hash
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(NOT _git_hash)
        set(_git_hash "unknown")
    endif()

    # Git branch
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E git rev-parse --abbrev-ref HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE _git_branch
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(NOT _git_branch)
        set(_git_branch "unknown")
    endif()

    # Toolchain info
    set(_compiler_id "${CMAKE_CXX_COMPILER_ID}")
    set(_compiler_version "${CMAKE_CXX_COMPILER_VERSION}")
    set(_build_type "${CMAKE_BUILD_TYPE}")

    # ABI version (default 0 when not specified)
    if(DEFINED GPM_ABI_MAJOR)
        set(_abi_major "${GPM_ABI_MAJOR}")
    else()
        set(_abi_major 0)
    endif()
    if(DEFINED GPM_ABI_MINOR)
        set(_abi_minor "${GPM_ABI_MINOR}")
    else()
        set(_abi_minor 0)
    endif()
    if(DEFINED GPM_ABI_PATCH)
        set(_abi_patch "${GPM_ABI_PATCH}")
    else()
        set(_abi_patch 0)
    endif()

    # Timestamp
    string(TIMESTAMP _timestamp UTC)

    # Protocol version (optional, used by Wish backend)
    set(_include_protocol FALSE)
    if(DEFINED GPM_PROTOCOL_MAJOR)
        set(_protocol_major "${GPM_PROTOCOL_MAJOR}")
        set(_include_protocol TRUE)
    else()
        set(_protocol_major 0)
    endif()
    if(DEFINED GPM_PROTOCOL_MINOR)
        set(_protocol_minor "${GPM_PROTOCOL_MINOR}")
        set(_include_protocol TRUE)
    else()
        set(_protocol_minor 0)
    endif()

    # --- Write manifest file -------------------------------------------------
    set(_manifest_content "{\n")
    set(_manifest_content "${_manifest_content}  \"product\": \"${GPM_PRODUCT_NAME}\",\n")
    set(_manifest_content "${_manifest_content}  \"version\": \"${_manifest_version}\",\n")
    set(_manifest_content "${_manifest_content}  \"commit\": \"${_git_hash}\",\n")
    set(_manifest_content "${_manifest_content}  \"branch\": \"${_git_branch}\",\n")
    set(_manifest_content "${_manifest_content}  \"toolchain\": {\n")
    set(_manifest_content "${_manifest_content}    \"compiler_id\": \"${_compiler_id}\",\n")
    set(_manifest_content "${_manifest_content}    \"compiler_version\": \"${_compiler_version}\",\n")
    set(_manifest_content "${_manifest_content}    \"build_type\": \"${_build_type}\"\n")
    set(_manifest_content "${_manifest_content}  },\n")
    set(_manifest_content "${_manifest_content}  \"abi\": {\n")
    set(_manifest_content "${_manifest_content}    \"major\": ${_abi_major},\n")
    set(_manifest_content "${_manifest_content}    \"minor\": ${_abi_minor},\n")
    set(_manifest_content "${_manifest_content}    \"patch\": ${_abi_patch}\n")
    set(_manifest_content "${_manifest_content}  }")
    if(_include_protocol)
        string(APPEND _manifest_content ",\n")
        string(APPEND _manifest_content "  \"protocol\": {\n")
        string(APPEND _manifest_content "    \"major\": ${_protocol_major},\n")
        string(APPEND _manifest_content "    \"minor\": ${_protocol_minor}\n")
        string(APPEND _manifest_content "  }")
    endif()
    string(APPEND _manifest_content ",\n")
    string(APPEND _manifest_content "  \"timestamp\": \"${_timestamp}\"\n")
    string(APPEND _manifest_content "}\n")

    file(WRITE "${GPM_OUTPUT_FILE}" "${_manifest_content}")
    message(STATUS "Generated manifest: ${GPM_OUTPUT_FILE}")
endfunction()

# ── Helper: install the manifest with a predictable filename ────────────
function(install_product_manifest manifest_path product_slug)
    if(EXISTS "${manifest_path}")
        install(
            FILES "${manifest_path}"
            DESTINATION ${CMAKE_INSTALL_DATADIR}/manifests
            RENAME "${product_slug}-manifest.json"
            COMPONENT Ahamkara
        )
    endif()
endfunction()

# ── Helper: append artifact checksums to a manifest post-build ──────────
#
# This function wraps the AppendManifestChecksums.cmake script so it runs
# at install time (after all artifacts are built). It updates the manifest
# JSON in-place, adding SHA256 checksums and file sizes for built artifacts.
#
# Usage:
#   append_manifest_checksums(
#     MANIFEST       <path-to-manifest.json>       # Required
#     ARTIFACT_DIR   <binary-dir>                   # Required: build tree root
#     [ARTIFACT_PATTERNS  "lib/*.a;lib/*.so;bin/*"] # Default covers .a/.so/.dylib/.lib/.dll
#     [COMPONENT     "Ahamkara"]                    # Install component (default: Ahamkara)
#   )
#
function(append_manifest_checksums)
    set(options "")
    set(one_value_args MANIFEST ARTIFACT_DIR ARTIFACT_PATTERNS COMPONENT)
    set(multi_value_args "")
    cmake_parse_arguments(AMC "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})

    if(NOT AMC_MANIFEST OR NOT AMC_ARTIFACT_DIR)
        message(FATAL_ERROR "append_manifest_checksums: MANIFEST and ARTIFACT_DIR required")
    endif()

    if(NOT AMC_ARTIFACT_PATTERNS)
        # Ninja multi-target layout: .a files are in per-target subdirs, not lib/
        set(AMC_ARTIFACT_PATTERNS
            "*.a"
            "*.so"
            "*.dylib"
            "*.lib"
            "*.dll"
            "ahamkara_server"
            "ahamkara"
            "ahamkara_*"
            "flashback"
            "wish_test_client"
        )
    endif()
    if(NOT AMC_COMPONENT)
        set(AMC_COMPONENT "Ahamkara")
    endif()

    # Run at install time (after all artifacts are built), under the same
    # component as the manifest install so --component selects both.
    install(CODE
        "set(MANIFEST \"${AMC_MANIFEST}\")
         set(ARTIFACT_DIR \"${AMC_ARTIFACT_DIR}\")
         set(ARTIFACT_PATTERNS \"${AMC_ARTIFACT_PATTERNS}\")
         include(\"${CMAKE_CURRENT_LIST_DIR}/AppendManifestChecksums.cmake\")"
        COMPONENT "${AMC_COMPONENT}"
    )
endfunction()
