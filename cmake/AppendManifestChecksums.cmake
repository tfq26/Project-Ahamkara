# AppendManifestChecksums
# ========================
# Post-build CMake script that computes SHA256 checksums of built artifacts
# and appends them to a product manifest JSON file.
#
# Two modes:
#
# 1. Auto-scan mode — glob ARTIFACT_DIR for matching files:
#   cmake -D MANIFEST=<path> -D ARTIFACT_DIR=<dir>
#         -D ARTIFACT_PATTERNS="lib/*.a;lib/*.so;bin/*"
#         -P cmake/AppendManifestChecksums.cmake
#
# 2. Explicit file list mode:
#   cmake -D MANIFEST=<path>
#         -D ARTIFACT_FILES="file1;file2;file3"
#         -P cmake/AppendManifestChecksums.cmake
#
# The script reads the existing manifest JSON, computes SHA256 for each
# matching artifact, and writes an updated manifest with an "artifacts"
# block that maps relative paths to {sha256, size}.

cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED MANIFEST)
    message(FATAL_ERROR "MANIFEST is required")
endif()
if(NOT DEFINED ARTIFACT_DIR AND NOT DEFINED ARTIFACT_FILES)
    message(FATAL_ERROR "Either ARTIFACT_DIR or ARTIFACT_FILES must be set")
endif()

# Make sure the manifest exists
if(NOT EXISTS "${MANIFEST}")
    message(WARNING "Manifest not found, skipping checksums: ${MANIFEST}")
    return()
endif()

# Collect artifact files
set(_artifact_files "")
if(DEFINED ARTIFACT_FILES)
    # Explicit file list — use as-is (should be absolute paths)
    set(_artifact_files ${ARTIFACT_FILES})
else()
    # Auto-scan mode
    if(NOT DEFINED ARTIFACT_PATTERNS)
        # The Ninja multi-target build places .a files in per-target dirs
        # (engine/*/, game/, wish/) while install rules redirect to lib/.
        set(ARTIFACT_PATTERNS
            "*.a"           # Matches all static libs recursively
            "*.so"          # Shared libraries
            "*.dylib"       # macOS dylibs
            "*.lib"         # Windows import libs
            "*.dll"         # Windows DLLs
            "ahamkara_server"     # Server binary
            "ahamkara"            # Client binary
            "ahamkara_*"          # Other binaries
            "flashback"           # Flashback binary
            "wish_test_client"    # Test client
        )
    endif()
    # Iterate over each glob pattern separately — CMake does not expand
    # semicolons inside file(GLOB) arguments.
    set(_artifact_files "")
    foreach(_pattern IN LISTS ARTIFACT_PATTERNS)
        file(GLOB_RECURSE _matched
            RELATIVE "${ARTIFACT_DIR}"
            "${ARTIFACT_DIR}/${_pattern}"
        )
        list(APPEND _artifact_files ${_matched})
    endforeach()
    # Exclude fetched-dependency artifacts
    list(FILTER _artifact_files EXCLUDE REGEX "^_deps/")
    list(SORT _artifact_files)
    list(REMOVE_DUPLICATES _artifact_files)
endif()

# Compute checksums and build the artifacts block
set(_artifact_entries "")
set(_count 0)

foreach(_item IN LISTS _artifact_files)
    if(DEFINED ARTIFACT_FILES)
        set(_abs_path "${_item}")
        file(RELATIVE_PATH _rel_path "${ARTIFACT_DIR}" "${_abs_path}")
    else()
        set(_abs_path "${ARTIFACT_DIR}/${_item}")
        set(_rel_path "${_item}")
    endif()

    if(NOT EXISTS "${_abs_path}" OR IS_DIRECTORY "${_abs_path}")
        continue()
    endif()

    file(SHA256 "${_abs_path}" _checksum)
    file(SIZE "${_abs_path}" _size)

    if(_artifact_entries)
        string(APPEND _artifact_entries ",\n")
    endif()
    string(APPEND _artifact_entries
        "    \"${_rel_path}\": {\n"
        "      \"sha256\": \"${_checksum}\",\n"
        "      \"size\": ${_size}\n"
        "    }")
    math(EXPR _count "${_count} + 1")
endforeach()

# Read existing manifest and strip trailing whitespace
file(READ "${MANIFEST}" _manifest_raw)
string(REGEX REPLACE "[ \t\r\n]+$" "" _manifest_body "${_manifest_raw}")

# Remove the trailing "}" so we can inject the artifacts block before it
string(LENGTH "${_manifest_body}" _body_len)
math(EXPR _last_char_idx "${_body_len} - 1")
string(SUBSTRING "${_manifest_body}" ${_last_char_idx} 1 _last_char)
if(_last_char STREQUAL "}")
    math(EXPR _new_len "${_body_len} - 1")
    string(SUBSTRING "${_manifest_body}" 0 ${_new_len} _manifest_body)
endif()

# Build the updated manifest
set(_updated "${_manifest_body},")
string(APPEND _updated "\n  \"artifacts\": {\n${_artifact_entries}\n  }\n}\n")

file(WRITE "${MANIFEST}" "${_updated}")
message(STATUS "Appended ${_count} artifact checksums to ${MANIFEST}")
