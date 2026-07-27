# CrossProductCompatibilityCheck
# ===============================
# CMake-time checks that verify Ahamkara engine and Wish backend versions
# are mutually compatible. Called at configure time from consumer projects
# (e.g. Flashback standalone).
#
# Usage:
#   include(cmake/CrossProductCompatibilityCheck.cmake)
#   check_ahamkara_wish_compatibility(
#     AHAMKARA_ABI_MAJOR 1  AHAMKARA_ABI_MINOR 0  AHAMKARA_ABI_PATCH 0
#     WISH_ABI_MAJOR     1  WISH_ABI_MINOR     0
#     WISH_PROTO_MAJOR   1  WISH_PROTO_MINOR   0
#   )
#
# The function first attempts to read ABI/protocol values from the installed
# product manifests (share/manifests/*-manifest.json). If the manifests are
# found, their values override the in-tree defaults, so the check always
# reflects the actual installed artifacts.
#
# On failure the function calls message(FATAL_ERROR) with a stable,
# actionable diagnostic string.

function(check_ahamkara_wish_compatibility)
    set(options "")
    set(one_value_args
        AHAMKARA_ABI_MAJOR AHAMKARA_ABI_MINOR AHAMKARA_ABI_PATCH
        WISH_ABI_MAJOR     WISH_ABI_MINOR
        WISH_PROTO_MAJOR   WISH_PROTO_MINOR
        PREFIX_PATH
    )
    set(multi_value_args "")
    cmake_parse_arguments(CK "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})

    # ── Validate that required arguments are present ─────────────────────
    foreach(_var IN ITEMS
        AHAMKARA_ABI_MAJOR AHAMKARA_ABI_MINOR AHAMKARA_ABI_PATCH
        WISH_ABI_MAJOR     WISH_ABI_MINOR
        WISH_PROTO_MAJOR   WISH_PROTO_MINOR
    )
        if(NOT DEFINED CK_${_var})
            message(FATAL_ERROR
                "check_ahamkara_wish_compatibility: required argument ${_var} not set")
        endif()
    endforeach()

    # ── Determine install prefix for manifest lookup ─────────────────────
    # If PREFIX_PATH was provided, use it; otherwise derive from Ahamkara_DIR
    set(_search_prefixes "")
    if(CK_PREFIX_PATH)
        list(APPEND _search_prefixes "${CK_PREFIX_PATH}")
    endif()
    if(DEFINED Ahamkara_DIR)
        # Ahamkara_DIR looks like <prefix>/lib/cmake/Ahamkara
        get_filename_component(_guess "${Ahamkara_DIR}/../../.." ABSOLUTE)
        list(APPEND _search_prefixes "${_guess}")
    endif()
    if(DEFINED Wish_DIR)
        get_filename_component(_guess "${Wish_DIR}/../../.." ABSOLUTE)
        list(APPEND _search_prefixes "${_guess}")
    endif()

    # ── Try to read ABI from installed manifests ─────────────────────────
    set(_ahamkara_abi_major 1)
    set(_ahamkara_abi_minor 0)
    set(_ahamkara_abi_patch 0)
    set(_wish_abi_major 1)
    set(_wish_abi_minor 0)
    set(_wish_proto_major 1)
    set(_wish_proto_minor 0)
    set(_manifest_found FALSE)

    foreach(_prefix IN LISTS _search_prefixes)
        set(_ae_manifest "${_prefix}/share/manifests/ahamkara-manifest.json")
        set(_wish_manifest "${_prefix}/share/manifests/wish-manifest.json")

        if(EXISTS "${_ae_manifest}")
            file(READ "${_ae_manifest}" _ae_json)
            string(JSON _ae_abi_major GET "${_ae_json}" "abi" "major")
            string(JSON _ae_abi_minor GET "${_ae_json}" "abi" "minor")
            string(JSON _ae_abi_patch GET "${_ae_json}" "abi" "patch")
            if(_ae_abi_major MATCHES "^[0-9]+$")
                set(_ahamkara_abi_major "${_ae_abi_major}")
            endif()
            if(_ae_abi_minor MATCHES "^[0-9]+$")
                set(_ahamkara_abi_minor "${_ae_abi_minor}")
            endif()
            if(_ae_abi_patch MATCHES "^[0-9]+$")
                set(_ahamkara_abi_patch "${_ae_abi_patch}")
            endif()
            set(_manifest_found TRUE)
            message(STATUS "Read Ahamkara ABI from manifest: ${_ahamkara_abi_major}.${_ahamkara_abi_minor}.${_ahamkara_abi_patch}")
        endif()

        if(EXISTS "${_wish_manifest}")
            file(READ "${_wish_manifest}" _wish_json)
            string(JSON _wish_abi_major GET "${_wish_json}" "abi" "major")
            string(JSON _wish_abi_minor GET "${_wish_json}" "abi" "minor")
            if(_wish_abi_major MATCHES "^[0-9]+$")
                set(_wish_abi_major "${_wish_abi_major}")
            endif()
            if(_wish_abi_minor MATCHES "^[0-9]+$")
                set(_wish_abi_minor "${_wish_abi_minor}")
            endif()
            # Read optional protocol version
            string(JSON _wish_no_proto TYPE "${_wish_json}" "protocol")
            if(_wish_no_proto STREQUAL "OBJECT")
                string(JSON _wish_proto_major GET "${_wish_json}" "protocol" "major")
                string(JSON _wish_proto_minor GET "${_wish_json}" "protocol" "minor")
                if(_wish_proto_major MATCHES "^[0-9]+$")
                    set(_wish_proto_major "${_wish_proto_major}")
                endif()
                if(_wish_proto_minor MATCHES "^[0-9]+$")
                    set(_wish_proto_minor "${_wish_proto_minor}")
                endif()
                message(STATUS "Read Wish protocol from manifest: ${_wish_proto_major}.${_wish_proto_minor}")
            endif()
            set(_manifest_found TRUE)
            message(STATUS "Read Wish from manifest: ABI ${_wish_abi_major}.${_wish_abi_minor}, proto ${_wish_proto_major}.${_wish_proto_minor}")
        endif()
    endforeach()

    if(NOT _manifest_found)
        message(STATUS "No product manifests found — using default ABI values"
            " (Ahamkara ${_ahamkara_abi_major}.${_ahamkara_abi_minor}.${_ahamkara_abi_patch},"
            " Wish ${_wish_abi_major}.${_wish_abi_minor},"
            " Wish proto ${_wish_proto_major}.${_wish_proto_minor})")
    endif()

    if(CK_AHAMKARA_ABI_MAJOR LESS _ahamkara_abi_major)
        message(FATAL_ERROR
            "INCOMPATIBLE: Ahamkara engine ABI major version mismatch. "
            "Flashback expects ABI ${CK_AHAMKARA_ABI_MAJOR}.${CK_AHAMKARA_ABI_MINOR}.${CK_AHAMKARA_ABI_PATCH} "
            "but the installed engine provides ABI ${_ahamkara_abi_major}.${_ahamkara_abi_minor}.${_ahamkara_abi_patch}. "
            "Rebuild Flashback against an engine with ABI major ${CK_AHAMKARA_ABI_MAJOR}.")
    elseif(CK_AHAMKARA_ABI_MAJOR GREATER _ahamkara_abi_major)
        message(FATAL_ERROR
            "INCOMPATIBLE: Ahamkara engine ABI major version mismatch. "
            "Flashback expects ABI ${CK_AHAMKARA_ABI_MAJOR}.${CK_AHAMKARA_ABI_MINOR}.${CK_AHAMKARA_ABI_PATCH} "
            "but the installed engine provides ABI ${_ahamkara_abi_major}.${_ahamkara_abi_minor}.${_ahamkara_abi_patch}. "
            "Upgrade Flashback to the latest SDK.")
    elseif(CK_AHAMKARA_ABI_MINOR GREATER _ahamkara_abi_minor)
        message(FATAL_ERROR
            "INCOMPATIBLE: Ahamkara engine ABI minor version too new. "
            "Flashback expects ABI ${CK_AHAMKARA_ABI_MAJOR}.${CK_AHAMKARA_ABI_MINOR}.${CK_AHAMKARA_ABI_PATCH} "
            "but the installed engine provides ABI ${_ahamkara_abi_major}.${_ahamkara_abi_minor}.${_ahamkara_abi_patch}. "
            "The engine is older than Flashback expects. Use a newer engine build.")
    endif()

    # ── Wish ABI check ──────────────────────────────────────────────────
    # (_wish_abi_major/minor were loaded from manifest or set to defaults above)

    if(CK_WISH_ABI_MAJOR LESS _wish_abi_major)
        message(FATAL_ERROR
            "INCOMPATIBLE: Wish ABI major version mismatch. "
            "Flashback expects Wish ABI ${CK_WISH_ABI_MAJOR}.${CK_WISH_ABI_MINOR} "
            "but the installed Wish library provides ABI ${_wish_abi_major}.${_wish_abi_minor}. "
            "Rebuild Flashback against a Wish library with ABI major ${CK_WISH_ABI_MAJOR}.")
    elseif(CK_WISH_ABI_MAJOR GREATER _wish_abi_major)
        message(FATAL_ERROR
            "INCOMPATIBLE: Wish ABI major version mismatch. "
            "Flashback expects Wish ABI ${CK_WISH_ABI_MAJOR}.${CK_WISH_ABI_MINOR} "
            "but the installed Wish library provides ABI ${_wish_abi_major}.${_wish_abi_minor}. "
            "Update Wish to match Flashback expectations.")
    elseif(CK_WISH_ABI_MINOR GREATER _wish_abi_minor)
        message(FATAL_ERROR
            "INCOMPATIBLE: Wish ABI minor version too new. "
            "Flashback expects Wish ABI ${CK_WISH_ABI_MAJOR}.${CK_WISH_ABI_MINOR} "
            "but the installed Wish library provides ABI ${_wish_abi_major}.${_wish_abi_minor}. "
            "The Wish library is older than Flashback requires. Use a newer Wish build.")
    endif()

    # ── Wish protocol check ─────────────────────────────────────────────
    # (_wish_proto_major/minor were loaded from manifest or set to defaults above)

    if(CK_WISH_PROTO_MAJOR LESS _wish_proto_major)
        message(FATAL_ERROR
            "INCOMPATIBLE: Wish wire protocol major version mismatch. "
            "Flashback expects protocol ${CK_WISH_PROTO_MAJOR}.${CK_WISH_PROTO_MINOR} "
            "but the installed Wish library provides protocol ${_wish_proto_major}.${_wish_proto_minor}. "
            "Flashback's protocol level is too old for this Wish library. "
            "Update Flashback or use an older Wish.")
    elseif(CK_WISH_PROTO_MAJOR GREATER _wish_proto_major)
        message(FATAL_ERROR
            "INCOMPATIBLE: Wish wire protocol major version mismatch. "
            "Flashback expects protocol ${CK_WISH_PROTO_MAJOR}.${CK_WISH_PROTO_MINOR} "
            "but the installed Wish library provides protocol ${_wish_proto_major}.${_wish_proto_minor}. "
            "Flashback's protocol level is too new for this Wish library. "
            "Update Wish or use an older Flashback.")
    elseif(CK_WISH_PROTO_MINOR GREATER _wish_proto_minor)
        message(FATAL_ERROR
            "INCOMPATIBLE: Wish wire protocol minor version mismatch. "
            "Flashback expects protocol ${CK_WISH_PROTO_MAJOR}.${CK_WISH_PROTO_MINOR} "
            "but the installed Wish library provides protocol ${_wish_proto_major}.${_wish_proto_minor}. "
            "Flashback requires a newer protocol extension. "
            "Update Wish or downgrade Flashback.")
    endif()

    message(STATUS "Cross-product compatibility: PASS"
        " (Ahamkara ABI ${_ahamkara_abi_major}.${_ahamkara_abi_minor}.${_ahamkara_abi_patch}, "
        "Wish ABI ${_wish_abi_major}.${_wish_abi_minor}, "
        "Wish proto ${_wish_proto_major}.${_wish_proto_minor})")
endfunction()
