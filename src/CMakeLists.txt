# QG application target and non-UI core library.

# ---------------------------------------------------------------------------
# Non-UI core library (DESIGN.md §15.1): config, logging, core domain,
# workers and persistence. GUI/QML targets link this instead of recompiling
# the same sources. It has no QML/Quick dependency.
# ---------------------------------------------------------------------------

# Compile the canonical schema.sql into the binary so there is exactly one copy
# of the schema text (see persistence/schema_sql.hpp.in).
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/persistence")
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/persistence/schema.sql" QG_SCHEMA_SQL)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/persistence/schema_sql.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/persistence/schema_sql.hpp"
    @ONLY)

add_library(qg_core STATIC
    config.cpp
    config.hpp
    logging.cpp
    logging.hpp

    core/errors.hpp
    core/events.cpp
    core/events.hpp
    core/identity.cpp
    core/identity.hpp
    core/types.hpp

    workers/priority_pool.cpp
    workers/priority_pool.hpp
    workers/task_token.cpp
    workers/task_token.hpp
    workers/tasks.cpp
    workers/tasks.hpp

    persistence/database.cpp
    persistence/database.hpp
    persistence/repositories.cpp
    persistence/repositories.hpp

    filesystem/paths.cpp
    filesystem/paths.hpp
    filesystem/enumerator.cpp
    filesystem/enumerator.hpp
    filesystem/mounts.cpp
    filesystem/mounts.hpp
    filesystem/watcher.cpp
    filesystem/watcher.hpp
    filesystem/operations.cpp
    filesystem/operations.hpp
    filesystem/trash.cpp
    filesystem/trash.hpp

    models/folder_model.cpp
    models/folder_model.hpp
    models/places_model.cpp
    models/places_model.hpp
    models/reel_model.cpp
    models/reel_model.hpp
    models/selection_state.cpp
    models/selection_state.hpp

    media/thumbnails.cpp
    media/thumbnails.hpp
    media/probe.cpp
    media/probe.hpp

    sorting/sort_service.cpp
    sorting/sort_service.hpp

    search/search_service.cpp
    search/search_service.hpp
    search/recursive_search_service.cpp
    search/recursive_search_service.hpp

    models/search_proxy.cpp
    models/search_proxy.hpp
    models/recursive_search_model.cpp
    models/recursive_search_model.hpp

    controllers/browser_controller.cpp
    controllers/browser_controller.hpp
    controllers/thumbnail_controller.cpp
    controllers/thumbnail_controller.hpp
    controllers/state_controller.cpp
    controllers/state_controller.hpp
    controllers/probe_controller.cpp
    controllers/probe_controller.hpp
    controllers/search_controller.cpp
    controllers/search_controller.hpp
    controllers/actions_controller.cpp
    controllers/actions_controller.hpp
    controllers/mounts_controller.cpp
    controllers/mounts_controller.hpp
    controllers/type_to_select_buffer.cpp
    controllers/type_to_select_buffer.hpp
)

# libmpv (single-handle playback backend + surface, P46/P47/P48)
add_library(qg_playback OBJECT
    playback/mpv_backend.cpp
    playback/mpv_backend.hpp
    playback/video_surface.cpp
    playback/video_surface.hpp
    playback/playback_session.cpp
    playback/playback_session.hpp
    playback/playback_manager.cpp
    playback/playback_manager.hpp
    playback/playback_context.cpp
    playback/playback_context.hpp
    controllers/playback_controller.cpp
    controllers/playback_controller.hpp

    # Central keyboard router (REMEDIATION-012 / D-11): playback-dependent, so it
    # lives here with the other playback controllers rather than in qg_core.
    controllers/keyboard_framework.cpp
    controllers/keyboard_framework.hpp

    core/composition.cpp
    core/composition.hpp
)
set_target_properties(qg_playback PROPERTIES AUTOMOC ON)
target_include_directories(qg_playback PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(qg_playback PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    qg_core
)
target_link_libraries(qg_playback PRIVATE
    ${QG_MPV_LIBRARIES}
)

set_target_properties(qg_core PROPERTIES AUTOMOC ON)

target_include_directories(qg_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}"
)

target_link_libraries(qg_core PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Concurrent
    SQLite::SQLite3
)

target_link_libraries(qg_core PRIVATE
    qg_warnings
)

# FFmpeg libraries (in-process thumbnail extraction / probing). Headers are only
# needed to compile qg_core; the libraries are linked publicly so the final
# executables resolve the symbols.
target_include_directories(qg_core PRIVATE
    ${QG_AVFORMAT_INCLUDE_DIRS}
    ${QG_AVCODEC_INCLUDE_DIRS}
    ${QG_AVUTIL_INCLUDE_DIRS}
    ${QG_SWSCALE_INCLUDE_DIRS}
)
target_link_libraries(qg_core PUBLIC
    ${QG_AVFORMAT_LIBRARIES}
    ${QG_AVCODEC_LIBRARIES}
    ${QG_AVUTIL_LIBRARIES}
    ${QG_SWSCALE_LIBRARIES}
)

# ---------------------------------------------------------------------------
# QG executable
# ---------------------------------------------------------------------------
qt_add_executable(qg
    main.cpp
    app.cpp
    app.hpp
    qml_plugin.cpp
)

set_target_properties(qg PROPERTIES
    AUTOMOC ON
    WIN32_EXECUTABLE OFF
    MACOSX_BUNDLE OFF
)

target_include_directories(qg PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(qg PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    qg_core
    qg_playback
    qg_qml
    qg_qmlplugin
    qg_warnings
)

# The static `qg` module plugin is imported explicitly in src/qml_plugin.cpp
# (Q_IMPORT_PLUGIN), which is required because loadFromModule references no QML
# type at compile time. No qt_import_qml_plugins() call is needed for a shared
# system Qt, and it would only emit missing-target warnings here.
