# =============================================================================
# QG dependency discovery and feature checks
# =============================================================================
# Central place for external dependency detection (DESIGN.md 15.5). Kept out of
# the root CMakeLists.txt so later prompts can extend the probe set without
# churning the top level.
#
# Detection policy for Prompt 01 (build foundation):
#   * Qt 6 and OpenGL are REQUIRED - the empty skeleton already links them.
#   * The remaining native libraries are part of the approved stack but are only
#     *consumed* by later prompts. Prompt 01 detects them and reports absence
#     clearly (Completion Criteria: "Required dependencies are detected or their
#     absence is reported clearly"). The prompts that actually link them promote
#     the corresponding QG_*_FOUND flag to a hard requirement via the
#     qg_require_dependency() helper below.
# =============================================================================

# --- Qt 6 (required) ---------------------------------------------------------
# Modules named by DESIGN.md 15.5. Test/Concurrent/DBus are included so the
# build contract matches the specification from the start.
find_package(Qt6 6.5 REQUIRED COMPONENTS
    Core
    Gui
    Qml
    Quick
    QuickControls2
    QuickTest
    QuickEffects
    Svg
    Concurrent
    Test
    DBus)

# --- OpenGL (required for the Qt Quick scene-graph backend) ------------------
set(OpenGL_GL_PREFERENCE GLVND)
find_package(OpenGL REQUIRED)

# --- pkg-config (required to probe native libraries) -------------------------
find_package(PkgConfig REQUIRED)

# --- Native library probes ---------------------------------------------------
# SQLite3 (persistence, DESIGN.md 21) - provided by CMake's FindSQLite3 module.
find_package(SQLite3 QUIET)

# libmpv (playback, DESIGN.md 19) - pkg-config module name is "mpv".
pkg_check_modules(QG_MPV QUIET mpv)

# FFmpeg (probing + thumbnail extraction, DESIGN.md 20).
pkg_check_modules(QG_AVFORMAT QUIET libavformat)
pkg_check_modules(QG_AVCODEC QUIET libavcodec)
pkg_check_modules(QG_AVUTIL QUIET libavutil)
pkg_check_modules(QG_SWSCALE QUIET libswscale)

# GLib / GIO (Trash, mounts, desktop integration, DESIGN.md 17/24).
pkg_check_modules(QG_GLIB QUIET glib-2.0)
pkg_check_modules(QG_GIO QUIET gio-2.0)

# --- Aggregate flags ---------------------------------------------------------
# FFmpeg is only usable when all four core libraries are present together.
set(QG_FFMPEG_FOUND FALSE)
if(QG_AVFORMAT_FOUND AND QG_AVCODEC_FOUND AND QG_AVUTIL_FOUND AND QG_SWSCALE_FOUND)
    set(QG_FFMPEG_FOUND TRUE)
endif()

# -----------------------------------------------------------------------------
# qg_require_dependency(<QG_*_FOUND flag> <human label>)
# Promotes a previously-detected optional dependency to a hard configure-time
# error. Later prompts call this once their targets actually link the library.
# -----------------------------------------------------------------------------
function(qg_require_dependency _flag _label)
    if(NOT ${_flag})
        message(FATAL_ERROR
            "QG requires ${_label}, which was not found during configuration. "
            "Install the matching development package (e.g. libsqlite3-dev, "
            "libmpv-dev, libavformat-dev/libavcodec-dev/libavutil-dev/"
            "libswscale-dev, libglib2.0-dev) and re-run CMake. If a package "
            "cannot be installed system-wide, an extracted dev package can be "
            "pointed at with -D<PackageName>_ROOT=<prefix>.")
    endif()
endfunction()

# Prompt 06 consumes SQLite for persistence; from here it is a hard requirement.
qg_require_dependency(SQLite3_FOUND "SQLite3 (persistence, Prompt 06)")

# Prompts 24/28 decode and probe media in-process with FFmpeg libraries.
qg_require_dependency(QG_FFMPEG_FOUND "FFmpeg libraries (thumbnails/probe, Prompt 24)")

# -----------------------------------------------------------------------------
# Dependency report
# -----------------------------------------------------------------------------
function(_qg_report _label _found _version)
    if(_found)
        if(_version)
            message(STATUS "  [ok]      ${_label} ${_version}")
        else()
            message(STATUS "  [ok]      ${_label}")
        endif()
    else()
        message(WARNING "  [MISSING] ${_label} - required by a later prompt")
    endif()
endfunction()

message(STATUS "QG dependency summary:")

_qg_report("Qt6"            TRUE "${Qt6_VERSION}")
_qg_report("OpenGL"         TRUE "${OPENGL_gl_LIBRARY}")
_qg_report("SQLite3"        ${SQLite3_FOUND} "${SQLite3_VERSION}")
_qg_report("libmpv"         ${QG_MPV_FOUND} "${QG_MPV_VERSION}")
_qg_report("FFmpeg"         ${QG_FFMPEG_FOUND} "${QG_AVFORMAT_VERSION}")
_qg_report("GLib/GIO"       ${QG_GIO_FOUND} "${QG_GIO_VERSION}")

unset(_qg_report)
