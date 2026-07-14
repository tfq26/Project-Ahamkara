# InstallRules.cmake
#
# Install rules for Ahamkara engine libraries, public headers, and
# server/client binaries.  Included from the root CMakeLists.txt.
#
# Usage:
#   include(cmake/InstallRules.cmake)

# ---------------------------------------------------------------------------
# Engine static libraries
# ---------------------------------------------------------------------------
# These targets are available in every supported configuration.
set(_AHAMKARA_CORE_LIBS
    ae_core
    ae_collision
    ae_physics
    ae_network
    ae_runtime
    ahamkara_game
    wish_engine
)

# Client libraries only exist when AHAMKARA_BUILD_CLIENT is enabled. Keep the
# install manifest aligned with the targets actually created by the preset.
foreach(_AHAMKARA_CLIENT_LIB
        ae_animation
        ae_audio
        ae_input
        ae_ui
        ae_render
        ae_platform)
    if(TARGET ${_AHAMKARA_CLIENT_LIB})
        list(APPEND _AHAMKARA_CORE_LIBS ${_AHAMKARA_CLIENT_LIB})
    endif()
endforeach()

install(
    TARGETS ${_AHAMKARA_CORE_LIBS}
    LIBRARY    DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE    DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME    DESTINATION ${CMAKE_INSTALL_BINDIR}
)

unset(_AHAMKARA_CLIENT_LIB)
unset(_AHAMKARA_CORE_LIBS)

# ---------------------------------------------------------------------------
# Public headers  (engine modules)
# ---------------------------------------------------------------------------
install(
    DIRECTORY   engine/core/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

install(
    DIRECTORY   engine/collision/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

install(
    DIRECTORY   engine/physics/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

install(
    DIRECTORY   engine/network/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

install(
    DIRECTORY   engine/runtime/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

install(
    DIRECTORY   engine/animation/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

install(
    DIRECTORY   engine/audio/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

install(
    DIRECTORY   engine/input/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

install(
    DIRECTORY   engine/ui/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

if(TARGET ae_render)
    install(
        DIRECTORY   engine/render/include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.h"
    )
endif()

if(TARGET ae_platform)
    install(
        DIRECTORY   engine/platform/include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.h"
    )
endif()

# ---------------------------------------------------------------------------
# Game-layer headers
# ---------------------------------------------------------------------------
install(
    DIRECTORY   game/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

# ---------------------------------------------------------------------------
# Wish engine headers
# ---------------------------------------------------------------------------
install(
    DIRECTORY   wish/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

# ---------------------------------------------------------------------------
# Server binary (if built)
# ---------------------------------------------------------------------------
if(TARGET ahamkara_server)
    install(
        TARGETS ahamkara_server
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

# ---------------------------------------------------------------------------
# Client binary (if built)
# ---------------------------------------------------------------------------
if(TARGET ahamkara)
    install(
        TARGETS ahamkara
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()
