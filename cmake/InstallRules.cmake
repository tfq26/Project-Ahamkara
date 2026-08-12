include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

# Rewrites a target's public include directories to the build/install
# interface pair used by the exported Ahamkara package.
function(ahamkara_set_export_includes target rel_include)
    if(NOT TARGET ${target})
        return()
    endif()
    set_property(TARGET ${target} PROPERTY INTERFACE_INCLUDE_DIRECTORIES "")
    target_include_directories(${target}
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/${rel_include}>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )
endfunction()

ahamkara_set_export_includes(ae_core engine/core/include)
ahamkara_set_export_includes(ae_network engine/network/include)
ahamkara_set_export_includes(ae_runtime engine/runtime/include)
ahamkara_set_export_includes(ae_collision engine/collision/include)
ahamkara_set_export_includes(ae_physics engine/physics/include)

set(_AHAMKARA_EXPORT_LIBS ae_core ae_network ae_runtime)

install(TARGETS ${_AHAMKARA_EXPORT_LIBS}
    EXPORT AhamkaraTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Ahamkara
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Ahamkara
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Ahamkara
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

set(_AHAMKARA_EXTRA_LIBS)
set(_AHAMKARA_EXTRA_LIB_LIST
    ae_collision ae_physics ae_skeleton ae_animation ae_audio ae_input
    ae_ui ae_render ae_platform ahamkara_game wish_engine
)
foreach(_lib IN ITEMS ${_AHAMKARA_EXTRA_LIB_LIST})
    if(TARGET ${_lib})
        list(APPEND _AHAMKARA_EXTRA_LIBS ${_lib})
    endif()
endforeach()
if(_AHAMKARA_EXTRA_LIBS)
    install(TARGETS ${_AHAMKARA_EXTRA_LIBS}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Ahamkara
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Ahamkara
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Ahamkara
    )
endif()

install(DIRECTORY engine/core/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} COMPONENT Ahamkara)
install(DIRECTORY engine/network/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} COMPONENT Ahamkara OPTIONAL)
install(DIRECTORY engine/runtime/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} COMPONENT Ahamkara OPTIONAL)
install(DIRECTORY engine/collision/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} COMPONENT Ahamkara OPTIONAL)
install(DIRECTORY engine/physics/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} COMPONENT Ahamkara OPTIONAL)

if(TARGET ae_skeleton)
    install(DIRECTORY engine/skeleton/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} COMPONENT Ahamkara)
endif()
if(TARGET ae_animation)
    install(DIRECTORY engine/animation/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} COMPONENT Ahamkara)
endif()
if(TARGET ae_render)
    install(DIRECTORY engine/render/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} COMPONENT Ahamkara)
endif()
if(TARGET ae_platform)
    install(DIRECTORY engine/platform/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} COMPONENT Ahamkara)
endif()
if(TARGET ahamkara_game)
    install(DIRECTORY game/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} COMPONENT Ahamkara)
endif()
if(TARGET wish_engine)
    install(DIRECTORY wish/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} COMPONENT Ahamkara)
endif()

if(TARGET ahamkara_server)
    install(TARGETS ahamkara_server RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Ahamkara)
endif()
if(TARGET ahamkara)
    install(TARGETS ahamkara RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Ahamkara)
endif()

install(EXPORT AhamkaraTargets
    FILE AhamkaraTargets.cmake
    NAMESPACE Ahamkara::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Ahamkara
    COMPONENT Ahamkara
)

if(TARGET ae_core AND NOT TARGET Ahamkara::Core)
    add_library(Ahamkara::Core ALIAS ae_core)
endif()
if(TARGET ae_runtime AND NOT TARGET Ahamkara::Runtime)
    add_library(Ahamkara::Runtime ALIAS ae_runtime)
endif()
if(TARGET ae_network AND NOT TARGET Ahamkara::Network)
    add_library(Ahamkara::Network ALIAS ae_network)
endif()

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/AhamkaraConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/AhamkaraConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/AhamkaraConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Ahamkara
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/AhamkaraConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/AhamkaraConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Ahamkara
    COMPONENT Ahamkara
)

# ── Wish backend separate package ─────────────────────────────────────────
# Export wish_engine to its own target set so consumer projects can use
# find_package(Wish CONFIG) independently of the Ahamkara package.
# Note: wish_engine target install is handled in wish/CMakeLists.txt.
if(TARGET wish_engine)
    install(EXPORT WishTargets
        FILE    WishTargets.cmake
        NAMESPACE Wish::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Wish
        COMPONENT Wish
    )

    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/WishConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/WishConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/WishConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Wish
    )

    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/WishConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/WishConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Wish
        COMPONENT Wish
    )

    if(NOT TARGET Wish::Engine)
        add_library(Wish::Engine ALIAS wish_engine)
    endif()
endif()

# ── Install product manifests ────────────────────────────────────────────
include(GenerateProductManifest)
if(TARGET ae_core)
    generate_product_manifest(
        PRODUCT_NAME "Ahamkara Engine"
        ABI_MAJOR    1
        ABI_MINOR    0
        ABI_PATCH    0
        OUTPUT_FILE  "${CMAKE_CURRENT_BINARY_DIR}/ahamkara-manifest.json"
    )
    append_manifest_checksums(
        MANIFEST      "${CMAKE_CURRENT_BINARY_DIR}/ahamkara-manifest.json"
        ARTIFACT_DIR  "${CMAKE_CURRENT_BINARY_DIR}"
    )
    install_product_manifest("${CMAKE_CURRENT_BINARY_DIR}/ahamkara-manifest.json" "ahamkara")
endif()
if(TARGET wish_engine)
    generate_product_manifest(
        PRODUCT_NAME   "Wish Backend"
        ABI_MAJOR      1
        ABI_MINOR      0
        ABI_PATCH      0
        PROTOCOL_MAJOR 1
        PROTOCOL_MINOR 0
        OUTPUT_FILE    "${CMAKE_CURRENT_BINARY_DIR}/wish-manifest.json"
    )
    append_manifest_checksums(
        MANIFEST      "${CMAKE_CURRENT_BINARY_DIR}/wish-manifest.json"
        ARTIFACT_DIR  "${CMAKE_CURRENT_BINARY_DIR}"
    )
    install_product_manifest("${CMAKE_CURRENT_BINARY_DIR}/wish-manifest.json" "wish")
endif()

# ── Install CMake helper modules for consumer projects ────────────────────
install(FILES
    "${CMAKE_SOURCE_DIR}/cmake/CrossProductCompatibilityCheck.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Ahamkara
    COMPONENT Ahamkara
)
install(FILES
    "${CMAKE_SOURCE_DIR}/cmake/CrossProductCompatibilityCheck.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Wish
    COMPONENT Wish
)
