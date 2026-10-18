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

# Generate a JSON manifest with build, ABI, and optional protocol metadata.
# Resolve the current Git revision and branch, using stable fallbacks outside Git.
function(_gpm_git_metadata out_hash out_branch)
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E git rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE _hash
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(NOT _hash)
        set(_hash "unknown")
    endif()
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E git rev-parse --abbrev-ref HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE _branch
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(NOT _branch)
        set(_branch "unknown")
    endif()
    set(${out_hash} "${_hash}" PARENT_SCOPE)
    set(${out_branch} "${_branch}" PARENT_SCOPE)
endfunction()

# Serialize the normalized product metadata to the requested JSON file.
function(_gpm_write_manifest)
    set(one_value_args
        OUTPUT_FILE PRODUCT_NAME VERSION GIT_HASH GIT_BRANCH COMPILER_ID
        COMPILER_VERSION BUILD_TYPE ABI_MAJOR ABI_MINOR ABI_PATCH
        INCLUDE_PROTOCOL PROTOCOL_MAJOR PROTOCOL_MINOR TIMESTAMP
    )
    cmake_parse_arguments(GPMW "" "${one_value_args}" "" ${ARGN})
    set(_content "{\n")
    set(_content "${_content}  \"product\": \"${GPMW_PRODUCT_NAME}\",\n")
    set(_content "${_content}  \"version\": \"${GPMW_VERSION}\",\n")
    set(_content "${_content}  \"commit\": \"${GPMW_GIT_HASH}\",\n")
    set(_content "${_content}  \"branch\": \"${GPMW_GIT_BRANCH}\",\n")
    set(_content "${_content}  \"toolchain\": {\n")
    set(_content "${_content}    \"compiler_id\": \"${GPMW_COMPILER_ID}\",\n")
    set(_content "${_content}    \"compiler_version\": \"${GPMW_COMPILER_VERSION}\",\n")
    set(_content "${_content}    \"build_type\": \"${GPMW_BUILD_TYPE}\"\n")
    set(_content "${_content}  },\n")
    set(_content "${_content}  \"abi\": {\n")
    set(_content "${_content}    \"major\": ${GPMW_ABI_MAJOR},\n")
    set(_content "${_content}    \"minor\": ${GPMW_ABI_MINOR},\n")
    set(_content "${_content}    \"patch\": ${GPMW_ABI_PATCH}\n")
    set(_content "${_content}  }")
    if(GPMW_INCLUDE_PROTOCOL)
        string(APPEND _content ",\n")
        string(APPEND _content "  \"protocol\": {\n")
        string(APPEND _content "    \"major\": ${GPMW_PROTOCOL_MAJOR},\n")
        string(APPEND _content "    \"minor\": ${GPMW_PROTOCOL_MINOR}\n")
        string(APPEND _content "  }")
    endif()
    string(APPEND _content ",\n")
    string(APPEND _content "  \"timestamp\": \"${GPMW_TIMESTAMP}\"\n")
    string(APPEND _content "}\n")
    file(WRITE "${GPMW_OUTPUT_FILE}" "${_content}")
    message(STATUS "Generated manifest: ${GPMW_OUTPUT_FILE}")
endfunction()

# Generate a JSON manifest with build, ABI, and optional protocol metadata.
function(generate_product_manifest)
    set(one_value_args
        PRODUCT_NAME ABI_MAJOR ABI_MINOR ABI_PATCH
        PROTOCOL_MAJOR PROTOCOL_MINOR OUTPUT_FILE
    )
    cmake_parse_arguments(GPM "" "${one_value_args}" "" ${ARGN})
    if(NOT GPM_PRODUCT_NAME OR NOT GPM_OUTPUT_FILE)
        message(FATAL_ERROR "generate_product_manifest: PRODUCT_NAME, OUTPUT_FILE required")
    endif()

    if(PROJECT_VERSION)
        set(_version "${PROJECT_VERSION}")
    else()
        set(_version "0.0.0")
    endif()
    _gpm_git_metadata(_git_hash _git_branch)

    set(_abi_major 0)
    set(_abi_minor 0)
    set(_abi_patch 0)
    if(DEFINED GPM_ABI_MAJOR)
        set(_abi_major "${GPM_ABI_MAJOR}")
    endif()
    if(DEFINED GPM_ABI_MINOR)
        set(_abi_minor "${GPM_ABI_MINOR}")
    endif()
    if(DEFINED GPM_ABI_PATCH)
        set(_abi_patch "${GPM_ABI_PATCH}")
    endif()

    set(_include_protocol FALSE)
    set(_protocol_major 0)
    set(_protocol_minor 0)
    if(DEFINED GPM_PROTOCOL_MAJOR)
        set(_protocol_major "${GPM_PROTOCOL_MAJOR}")
        set(_include_protocol TRUE)
    endif()
    if(DEFINED GPM_PROTOCOL_MINOR)
        set(_protocol_minor "${GPM_PROTOCOL_MINOR}")
        set(_include_protocol TRUE)
    endif()
    string(TIMESTAMP _timestamp UTC)
    _gpm_write_manifest(
        OUTPUT_FILE "${GPM_OUTPUT_FILE}"
        PRODUCT_NAME "${GPM_PRODUCT_NAME}"
        VERSION "${_version}"
        GIT_HASH "${_git_hash}"
        GIT_BRANCH "${_git_branch}"
        COMPILER_ID "${CMAKE_CXX_COMPILER_ID}"
        COMPILER_VERSION "${CMAKE_CXX_COMPILER_VERSION}"
        BUILD_TYPE "${CMAKE_BUILD_TYPE}"
        ABI_MAJOR "${_abi_major}"
        ABI_MINOR "${_abi_minor}"
        ABI_PATCH "${_abi_patch}"
        INCLUDE_PROTOCOL "${_include_protocol}"
        PROTOCOL_MAJOR "${_protocol_major}"
        PROTOCOL_MINOR "${_protocol_minor}"
        TIMESTAMP "${_timestamp}"
    )
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
