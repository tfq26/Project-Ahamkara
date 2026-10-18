# CrossProductCompatibilityCheck
# ===============================
# Verify that Ahamkara and Wish packages used by a consumer have compatible
# ABI and protocol versions.
#
# Usage:
#   include(cmake/CrossProductCompatibilityCheck.cmake)
#   check_ahamkara_wish_compatibility(AHAMKARA_ABI_MAJOR 1 ...)

# Read an installed Ahamkara manifest and update ABI values in the caller.
function(_ck_read_ahamkara_manifest prefix)
    set(_manifest "${prefix}/share/manifests/ahamkara-manifest.json")
    if(EXISTS "${_manifest}")
        file(READ "${_manifest}" _json)
        string(JSON _major GET "${_json}" "abi" "major")
        string(JSON _minor GET "${_json}" "abi" "minor")
        string(JSON _patch GET "${_json}" "abi" "patch")
        if(_major MATCHES "^[0-9]+$")
            set(_ahamkara_abi_major "${_major}" PARENT_SCOPE)
        endif()
        if(_minor MATCHES "^[0-9]+$")
            set(_ahamkara_abi_minor "${_minor}" PARENT_SCOPE)
        endif()
        if(_patch MATCHES "^[0-9]+$")
            set(_ahamkara_abi_patch "${_patch}" PARENT_SCOPE)
        endif()
        set(_manifest_found TRUE PARENT_SCOPE)
        message(STATUS "Read Ahamkara ABI from manifest: "
            "${_major}.${_minor}.${_patch}")
    endif()
endfunction()

# Read an installed Wish manifest and update ABI/protocol values in the caller.
function(_ck_read_wish_manifest prefix)
    set(_manifest "${prefix}/share/manifests/wish-manifest.json")
    if(EXISTS "${_manifest}")
        file(READ "${_manifest}" _json)
        string(JSON _major GET "${_json}" "abi" "major")
        string(JSON _minor GET "${_json}" "abi" "minor")
        if(_major MATCHES "^[0-9]+$")
            set(_wish_abi_major "${_major}" PARENT_SCOPE)
        endif()
        if(_minor MATCHES "^[0-9]+$")
            set(_wish_abi_minor "${_minor}" PARENT_SCOPE)
        endif()
        string(JSON _protocol_type TYPE "${_json}" "protocol")
        if(_protocol_type STREQUAL "OBJECT")
            string(JSON _protocol_major GET "${_json}" "protocol" "major")
            string(JSON _protocol_minor GET "${_json}" "protocol" "minor")
            if(_protocol_major MATCHES "^[0-9]+$")
                set(_wish_proto_major "${_protocol_major}")
                set(_wish_proto_major "${_protocol_major}" PARENT_SCOPE)
            endif()
            if(_protocol_minor MATCHES "^[0-9]+$")
                set(_wish_proto_minor "${_protocol_minor}")
                set(_wish_proto_minor "${_protocol_minor}" PARENT_SCOPE)
            endif()
            message(STATUS "Read Wish protocol from manifest: "
                "${_protocol_major}.${_protocol_minor}")
        endif()
        set(_manifest_found TRUE PARENT_SCOPE)
        message(STATUS "Read Wish from manifest: ABI ${_major}.${_minor}, "
            "proto ${_wish_proto_major}.${_wish_proto_minor}")
    endif()
endfunction()

# Reject incompatible Ahamkara engine ABI versions with actionable errors.
function(_ck_check_ahamkara_abi expected actual)
    string(REPLACE "." ";" _expected "${expected}")
    string(REPLACE "." ";" _actual "${actual}")
    list(GET _expected 0 expected_major)
    list(GET _expected 1 expected_minor)
    list(GET _expected 2 expected_patch)
    list(GET _actual 0 actual_major)
    list(GET _actual 1 actual_minor)
    list(GET _actual 2 actual_patch)
    if(expected_major LESS actual_major)
        message(FATAL_ERROR
            "INCOMPATIBLE: Ahamkara engine ABI major version mismatch. "
            "Flashback expects ABI ${expected_major}.${expected_minor}.${expected_patch} "
            "but the installed engine provides ABI ${actual_major}.${actual_minor}.${actual_patch}. "
            "Rebuild Flashback against an engine with ABI major ${expected_major}.")
    elseif(expected_major GREATER actual_major)
        message(FATAL_ERROR
            "INCOMPATIBLE: Ahamkara engine ABI major version mismatch. "
            "Flashback expects ABI ${expected_major}.${expected_minor}.${expected_patch} "
            "but the installed engine provides ABI ${actual_major}.${actual_minor}.${actual_patch}. "
            "Upgrade Flashback to the latest SDK.")
    elseif(expected_minor GREATER actual_minor)
        message(FATAL_ERROR
            "INCOMPATIBLE: Ahamkara engine ABI minor version too new. "
            "Flashback expects ABI ${expected_major}.${expected_minor}.${expected_patch} "
            "but the installed engine provides ABI ${actual_major}.${actual_minor}.${actual_patch}. "
            "The engine is older than Flashback expects. Use a newer engine build.")
    endif()
endfunction()

# Reject incompatible Wish library ABI versions with actionable errors.
function(_ck_check_wish_abi expected actual)
    string(REPLACE "." ";" _expected "${expected}")
    string(REPLACE "." ";" _actual "${actual}")
    list(GET _expected 0 expected_major)
    list(GET _expected 1 expected_minor)
    list(GET _actual 0 actual_major)
    list(GET _actual 1 actual_minor)
    if(expected_major LESS actual_major)
        message(FATAL_ERROR
            "INCOMPATIBLE: Wish ABI major version mismatch. "
            "Flashback expects Wish ABI ${expected_major}.${expected_minor} "
            "but the installed Wish library provides ABI ${actual_major}.${actual_minor}. "
            "Rebuild Flashback against a Wish library with ABI major ${expected_major}.")
    elseif(expected_major GREATER actual_major)
        message(FATAL_ERROR
            "INCOMPATIBLE: Wish ABI major version mismatch. "
            "Flashback expects Wish ABI ${expected_major}.${expected_minor} "
            "but the installed Wish library provides ABI ${actual_major}.${actual_minor}. "
            "Update Wish to match Flashback expectations.")
    elseif(expected_minor GREATER actual_minor)
        message(FATAL_ERROR
            "INCOMPATIBLE: Wish ABI minor version too new. "
            "Flashback expects Wish ABI ${expected_major}.${expected_minor} "
            "but the installed Wish library provides ABI ${actual_major}.${actual_minor}. "
            "The Wish library is older than Flashback requires. Use a newer Wish build.")
    endif()
endfunction()

# Reject incompatible Wish wire protocol versions with actionable errors.
function(_ck_check_wish_protocol expected actual)
    string(REPLACE "." ";" _expected "${expected}")
    string(REPLACE "." ";" _actual "${actual}")
    list(GET _expected 0 expected_major)
    list(GET _expected 1 expected_minor)
    list(GET _actual 0 actual_major)
    list(GET _actual 1 actual_minor)
    if(expected_major LESS actual_major)
        message(FATAL_ERROR
            "INCOMPATIBLE: Wish wire protocol major version mismatch. "
            "Flashback expects protocol ${expected_major}.${expected_minor} "
            "but the installed Wish library provides protocol ${actual_major}.${actual_minor}. "
            "Flashback's protocol level is too old for this Wish library. "
            "Update Flashback or use an older Wish.")
    elseif(expected_major GREATER actual_major)
        message(FATAL_ERROR
            "INCOMPATIBLE: Wish wire protocol major version mismatch. "
            "Flashback expects protocol ${expected_major}.${expected_minor} "
            "but the installed Wish library provides protocol ${actual_major}.${actual_minor}. "
            "Flashback's protocol level is too new for this Wish library. "
            "Update Wish or use an older Flashback.")
    elseif(expected_minor GREATER actual_minor)
        message(FATAL_ERROR
            "INCOMPATIBLE: Wish wire protocol minor version too new. "
            "Flashback expects protocol ${expected_major}.${expected_minor} "
            "but the installed Wish library provides protocol ${actual_major}.${actual_minor}. "
            "Flashback requires a newer protocol extension. "
            "Update Wish or downgrade Flashback.")
    endif()
endfunction()

# Check consumer ABI/protocol arguments against installed product manifests.
function(check_ahamkara_wish_compatibility)
    set(one_value_args
        AHAMKARA_ABI_MAJOR AHAMKARA_ABI_MINOR AHAMKARA_ABI_PATCH
        WISH_ABI_MAJOR WISH_ABI_MINOR WISH_PROTO_MAJOR WISH_PROTO_MINOR
        PREFIX_PATH
    )
    cmake_parse_arguments(CK "" "${one_value_args}" "" ${ARGN})
    foreach(_var IN ITEMS
        AHAMKARA_ABI_MAJOR AHAMKARA_ABI_MINOR AHAMKARA_ABI_PATCH
        WISH_ABI_MAJOR WISH_ABI_MINOR WISH_PROTO_MAJOR WISH_PROTO_MINOR
    )
        if(NOT DEFINED CK_${_var})
            message(FATAL_ERROR
                "check_ahamkara_wish_compatibility: required argument ${_var} not set")
        endif()
    endforeach()

    set(_search_prefixes)
    if(CK_PREFIX_PATH)
        list(APPEND _search_prefixes "${CK_PREFIX_PATH}")
    endif()
    foreach(_package_dir IN ITEMS "${Ahamkara_DIR}" "${Wish_DIR}")
        if(_package_dir)
            get_filename_component(_prefix "${_package_dir}/../../.." ABSOLUTE)
            list(APPEND _search_prefixes "${_prefix}")
        endif()
    endforeach()

    set(_ahamkara_abi_major 1)
    set(_ahamkara_abi_minor 0)
    set(_ahamkara_abi_patch 0)
    set(_wish_abi_major 1)
    set(_wish_abi_minor 0)
    set(_wish_proto_major 1)
    set(_wish_proto_minor 0)
    set(_manifest_found FALSE)
    foreach(_prefix IN LISTS _search_prefixes)
        _ck_read_ahamkara_manifest("${_prefix}")
        _ck_read_wish_manifest("${_prefix}")
    endforeach()

    if(NOT _manifest_found)
        message(STATUS "No product manifests found — using default ABI values"
            " (Ahamkara ${_ahamkara_abi_major}.${_ahamkara_abi_minor}.${_ahamkara_abi_patch},"
            " Wish ${_wish_abi_major}.${_wish_abi_minor},"
            " Wish proto ${_wish_proto_major}.${_wish_proto_minor})")
    endif()
    _ck_check_ahamkara_abi(
        "${CK_AHAMKARA_ABI_MAJOR}.${CK_AHAMKARA_ABI_MINOR}.${CK_AHAMKARA_ABI_PATCH}"
        "${_ahamkara_abi_major}.${_ahamkara_abi_minor}.${_ahamkara_abi_patch}")
    _ck_check_wish_abi(
        "${CK_WISH_ABI_MAJOR}.${CK_WISH_ABI_MINOR}"
        "${_wish_abi_major}.${_wish_abi_minor}")
    _ck_check_wish_protocol(
        "${CK_WISH_PROTO_MAJOR}.${CK_WISH_PROTO_MINOR}"
        "${_wish_proto_major}.${_wish_proto_minor}")
    message(STATUS "Cross-product compatibility: PASS"
        " (Ahamkara ABI ${_ahamkara_abi_major}.${_ahamkara_abi_minor}.${_ahamkara_abi_patch}, "
        "Wish ABI ${_wish_abi_major}.${_wish_abi_minor}, "
        "Wish proto ${_wish_proto_major}.${_wish_proto_minor})")
endfunction()
