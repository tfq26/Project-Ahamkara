include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

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
foreach(_lib IN ITEMS ae_collision ae_physics ae_skeleton ae_animation ae_audio ae_input ae_ui ae_render ae_platform ahamkara_game wish_engine)
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
