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
# These targets are always available.
set(_AHAMKARA_CORE_LIBS
    ae_core
    ae_collision
    ae_physics
    ae_network
    ae_runtime
    ae_animation
    ae_audio
    ae_input
    ae_ui
    ahamkara_game
    wish_engine
)

# ae_render and ae_platform are only built when the client is enabled.
if(TARGET ae_render)
    list(APPEND _AHAMKARA_CORE_LIBS ae_render)
endif()
if(TARGET ae_platform)
    list(APPEND _AHAMKARA_CORE_LIBS ae_platform)
endif()

install(
    TARGETS ${_AHAMKARA_CORE_LIBS}
    LIBRARY    DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE    DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME    DESTINATION ${CMAKE_INSTALL_BINDIR}
)

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
