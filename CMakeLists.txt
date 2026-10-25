cmake_minimum_required(VERSION 3.21)

project(qg
    VERSION 0.1.0
    DESCRIPTION "QG - native Ubuntu/Linux video file explorer"
    HOMEPAGE_URL "https://github.com/QGayze/QG"
    LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Language / build policy
# ---------------------------------------------------------------------------
# C++20 is the approved language baseline (DESIGN.md 15.5). Extensions are off
# so the code stays portable and warnings stay meaningful.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Let tooling (clangd, etc.) consume the compilation database.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Sanitizer builds (Prompt 71): `-DQG_SANITIZE=address,undefined` (or "address"
# / "undefined") compiles the whole project with the requested sanitizers.
set(QG_SANITIZE "" CACHE STRING "Sanitizers: address,undefined or address,undefined")
if(QG_SANITIZE)
    set(QG_SANITIZE_FLAGS "-fsanitize=${QG_SANITIZE};-fno-omit-frame-pointer;-g")
    add_compile_options(${QG_SANITIZE_FLAGS})
    add_link_options(${QG_SANITIZE_FLAGS})
endif()

# Default to a debuggable build when the caller picks nothing (e.g. a bare
# `cmake -S . -B build -G Ninja`). Presets set this explicitly.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# Make the project's own CMake modules (Dependencies.cmake) discoverable.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# ---------------------------------------------------------------------------
# Target-scoped warning policy
# ---------------------------------------------------------------------------
# An interface target keeps compiler flags attached to QG targets instead of
# polluting the global compiler configuration (see .hermes.md Build System).
add_library(qg_warnings INTERFACE)
target_compile_options(qg_warnings INTERFACE
    "$<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>:-Wall;-Wextra;-Wpedantic>")

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------
include(Dependencies)

# ---------------------------------------------------------------------------
# Targets
# ---------------------------------------------------------------------------
enable_testing()

add_subdirectory(qml)
add_subdirectory(src)
add_subdirectory(tests)

# ---------------------------------------------------------------------------
# Install rules (Prompt 72): executable, QML modules, icons, desktop entry,
# AppStream metadata — to standard XDG locations.
# ---------------------------------------------------------------------------
include(GNUInstallDirs)

install(TARGETS qg
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

# Icons: scalable + raster sizes in the hicolor theme
install(FILES
    resources/icons/hicolor/256x256/apps/io.github.QGayze.QG.png
    DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps"
)
foreach(QG_ICON_SIZE 32x32 48x48 64x64 128x128 256x256)
    install(FILES
        "resources/icons/hicolor/${QG_ICON_SIZE}/apps/io.github.QGayze.QG.svg"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/${QG_ICON_SIZE}/apps"
    )
endforeach()

# Desktop entry + AppStream metadata
install(FILES
    packaging/io.github.QGayze.QG.desktop
    DESTINATION "${CMAKE_INSTALL_DATADIR}/applications"
)
install(FILES
    packaging/io.github.QGayze.QG.metainfo.xml
    DESTINATION "${CMAKE_INSTALL_DATADIR}/metainfo"
)
