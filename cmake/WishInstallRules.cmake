# WishInstallRules
# =================
# Install rules for the standalone Wish package. Designed to be included from
# wish/CMakeLists.txt when AHAMKARA_BUILD_WISH is ON.
#
# Produces:
#   - WishTargets.cmake  (export set)
#   - WishConfig.cmake    (find_package entry point)
#   - WishConfigVersion.cmake
#   - Headers under include/
#   - Library under lib/

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

# ── Install the wish_engine target ──────────────────────────────────────
install(TARGETS wish_engine
    EXPORT  WishTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Wish
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Wish
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Wish
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# ── Install headers ─────────────────────────────────────────────────────
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    COMPONENT Wish
    FILES_MATCHING PATTERN "*.h"
)

# ── Export targets ──────────────────────────────────────────────────────
install(EXPORT WishTargets
    FILE    WishTargets.cmake
    NAMESPACE Wish::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Wish
    COMPONENT Wish
)

# ── Generate and install WishConfig.cmake ───────────────────────────────
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/WishConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_SOURCE_DIR}/cmake/WishConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/WishConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Wish
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/WishConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/WishConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Wish
    COMPONENT Wish
)
