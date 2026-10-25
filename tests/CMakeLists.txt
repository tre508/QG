# QG test suite.
#
# Non-UI tests link `qg_core` (config/logging/core/workers/persistence). GUI/QML
# tests link the QML module. Later prompts append focused test targets here.

# --- Build foundation smoke test (Prompt 01) ---------------------------------
add_executable(qg_smoke_test
    test_smoke.cpp
)
set_target_properties(qg_smoke_test PROPERTIES AUTOMOC ON)
target_link_libraries(qg_smoke_test PRIVATE
    Qt6::Core
    Qt6::Test
    qg_warnings
)
add_test(NAME qg_smoke COMMAND qg_smoke_test)

# --- XDG path resolution (Prompt 02) -----------------------------------------
add_executable(qg_xdg_paths_test
    test_xdg_paths.cpp
)
set_target_properties(qg_xdg_paths_test PROPERTIES AUTOMOC ON)
target_link_libraries(qg_xdg_paths_test PRIVATE
    qg_core
    Qt6::Test
    qg_warnings
)
add_test(NAME qg_xdg_paths COMMAND qg_xdg_paths_test)

# --- QML shell load test (Prompt 03) -----------------------------------------
qt_add_executable(qg_qml_test
    qml/tst_main.cpp
)
set_target_properties(qg_qml_test PROPERTIES AUTOMOC ON)
target_link_libraries(qg_qml_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickTest
    qg_qml
    qg_qmlplugin
    qg_playback
    qg_core
    qg_warnings
)

# The static `qg` module plugin is imported explicitly in qml/tst_main.cpp
# (Q_IMPORT_PLUGIN) so `import qg` resolves in the test.

add_test(NAME qg_qml COMMAND qg_qml_test -input "${CMAKE_CURRENT_SOURCE_DIR}/qml")

# --- Drag/drop + clipboard contract (REM-014 / D-27) ------------------------
add_test(NAME dragdrop COMMAND qg_qml_test -input "${CMAKE_CURRENT_SOURCE_DIR}/qml/tst_dragdrop.qml")
set_tests_properties(dragdrop PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# QML/UI tests run headless; never require a real display.
set_tests_properties(qg_qml PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# --- Worker/task infrastructure (Prompt 04) ----------------------------------
add_executable(qg_workers_test
    unit/test_workers.cpp
)
set_target_properties(qg_workers_test PROPERTIES AUTOMOC ON)
target_link_libraries(qg_workers_test PRIVATE
    qg_core
    Qt6::Test
    qg_warnings
)
add_test(NAME qg_workers COMMAND qg_workers_test)

# --- Core domain types, errors, identity, events (Prompt 05) -----------------
add_executable(qg_core_test
    unit/test_core.cpp
)
set_target_properties(qg_core_test PROPERTIES AUTOMOC ON)
target_link_libraries(qg_core_test PRIVATE
    qg_core
    Qt6::Test
    qg_warnings
)
add_test(NAME qg_core COMMAND qg_core_test)

# --- SQLite persistence foundation (Prompt 06) -------------------------------
add_executable(qg_persistence_test
    unit/test_persistence.cpp
)
set_target_properties(qg_persistence_test PROPERTIES AUTOMOC ON)
target_link_libraries(qg_persistence_test PRIVATE
    qg_core
    Qt6::Test
    qg_warnings
)
add_test(NAME qg_persistence COMMAND qg_persistence_test)

# --- Media format registry (Prompt 07) ---------------------------------------
add_executable(qg_formats_test
    unit/test_formats.cpp
)
set_target_properties(qg_formats_test PROPERTIES AUTOMOC ON)
target_link_libraries(qg_formats_test PRIVATE
    qg_core
    Qt6::Test
    qg_warnings
)
add_test(NAME qg_formats COMMAND qg_formats_test)

# --- Path helpers + filename validation (Prompt 08) --------------------------
add_executable(qg_paths_test
    unit/test_paths.cpp
)
set_target_properties(qg_paths_test PROPERTIES AUTOMOC ON)
target_link_libraries(qg_paths_test PRIVATE
    qg_core
    Qt6::Test
    qg_warnings
)
add_test(NAME qg_paths COMMAND qg_paths_test)

# --- Filesystem enumerator (Prompt 09) ---------------------------------------
add_executable(qg_enumerator_test
    unit/test_enumerator.cpp
)
set_target_properties(qg_enumerator_test PROPERTIES AUTOMOC ON)
target_link_libraries(qg_enumerator_test PRIVATE
    qg_core
    Qt6::Test
    qg_warnings
)
add_test(NAME qg_enumerator COMMAND qg_enumerator_test)

# --- Canonical FolderModel (Prompt 10) ---------------------------------------
add_executable(qg_folder_model_test
    unit/test_folder_model.cpp
)
set_target_properties(qg_folder_model_test PROPERTIES AUTOMOC ON)
target_link_libraries(qg_folder_model_test PRIVATE
    qg_core
    Qt6::Test
    qg_warnings
)
add_test(NAME qg_folder_model COMMAND qg_folder_model_test)

# --- Mount discovery + PlacesModel (Prompt 11) -------------------------------
add_executable(qg_mounts_test
    unit/test_mounts.cpp
)
set_target_properties(qg_mounts_test PROPERTIES AUTOMOC ON)
target_link_libraries(qg_mounts_test PRIVATE
    qg_core
    Qt6::Test
    qg_warnings
)
add_test(NAME qg_mounts COMMAND qg_mounts_test)

add_executable(qg_places_model_test
    unit/test_places_model.cpp
)
set_target_properties(qg_places_model_test PROPERTIES AUTOMOC ON)
target_link_libraries(qg_places_model_test PRIVATE
    qg_core
    Qt6::Test
    qg_warnings
)
add_test(NAME qg_places_model COMMAND qg_places_model_test)

# --- Directory watcher + identity diff (Prompt 12) ---------------------------
add_executable(qg_watcher_test
    unit/test_watcher.cpp
)
set_target_properties(qg_watcher_test PROPERTIES AUTOMOC ON)
target_link_libraries(qg_watcher_test PRIVATE
    qg_core
    Qt6::Test
    qg_warnings
)
add_test(NAME qg_watcher COMMAND qg_watcher_test)

# --- BrowserController (Prompt 13) -------------------------------------------
add_executable(qg_browser_controller_test
    unit/test_browser_controller.cpp
)
set_target_properties(qg_browser_controller_test PROPERTIES AUTOMOC ON)
target_link_libraries(qg_browser_controller_test PRIVATE
    qg_core
    Qt6::Test
    qg_warnings
)
add_test(NAME qg_browser_controller COMMAND qg_browser_controller_test)

# --- Thumbnail cache + freedesktop reader (Prompt 23) ------------------------
add_executable(qg_thumbnails_test
    unit/test_thumbnails.cpp
)
set_target_properties(qg_thumbnails_test PROPERTIES AUTOMOC ON)
target_link_libraries(qg_thumbnails_test PRIVATE
    qg_core
    Qt6::Test
    qg_warnings
)
add_test(NAME qg_thumbnails COMMAND qg_thumbnails_test)

# --- In-process thumbnail extraction (Prompt 24) -----------------------------
add_executable(qg_thumbnails_extract_test
    unit/test_thumbnails_extract.cpp
)
set_target_properties(qg_thumbnails_extract_test PROPERTIES AUTOMOC ON)
target_link_libraries(qg_thumbnails_extract_test PRIVATE
    qg_core
    Qt6::Test
    qg_warnings
)
add_test(NAME qg_thumbnails_extract COMMAND qg_thumbnails_extract_test)

# --- Visible-first thumbnail scheduler + LRU (Prompt 25) ---------------------
add_executable(qg_thumb_scheduler_test
    unit/test_thumb_scheduler.cpp
)
set_target_properties(qg_thumb_scheduler_test PROPERTIES AUTOMOC ON)
target_link_libraries(qg_thumb_scheduler_test PRIVATE
    qg_core
    Qt6::Test
    qg_warnings
)
add_test(NAME qg_thumb_scheduler COMMAND qg_thumb_scheduler_test)

# --- Thumbnail failure markers / retry / invalidation (Prompt 26) ------------
add_executable(qg_thumb_failure_test
    unit/test_thumb_failure.cpp
)
set_target_properties(qg_thumb_failure_test PROPERTIES AUTOMOC ON)
target_link_libraries(qg_thumb_failure_test PRIVATE
    qg_core
    Qt6::Test
    qg_warnings
)
add_test(NAME qg_thumb_failure COMMAND qg_thumb_failure_test)

# --- Thumbnail results -> FolderModel wiring (Prompt 27) ---------------------
add_executable(qg_thumb_wiring_test
    unit/test_thumb_wiring.cpp
)
set_target_properties(qg_thumb_wiring_test PROPERTIES AUTOMOC ON)
target_link_libraries(qg_thumb_wiring_test PRIVATE
    qg_core
    Qt6::Test
    qg_warnings
)
add_test(NAME qg_thumb_wiring COMMAND qg_thumb_wiring_test)

# --- Thumbnail data-to-visual delivery (VIS-010) ------------------------------
add_executable(qg_thumb_delivery_test
    unit/test_thumb_delivery.cpp
)
set_target_properties(qg_thumb_delivery_test PROPERTIES AUTOMOC ON)
target_link_libraries(qg_thumb_delivery_test PRIVATE
    qg_core
    Qt6::Test
    qg_warnings
)
add_test(NAME qg_thumb_delivery COMMAND qg_thumb_delivery_test)

# --- Media probing (Prompt 28) -----------------------------------------------
add_executable(qg_probe_test
    unit/test_probe.cpp
)
set_target_properties(qg_probe_test PROPERTIES AUTOMOC ON)
target_link_libraries(qg_probe_test PRIVATE
    qg_core
    Qt6::Test
    qg_warnings
)
add_test(NAME qg_probe COMMAND qg_probe_test)

# --- Identity<->path mapping + reconciliation (Prompt 29) --------------------
add_executable(qg_identity_map_test
    unit/test_identity_map.cpp
)
set_target_properties(qg_identity_map_test PROPERTIES AUTOMOC ON)
target_link_libraries(qg_identity_map_test PRIVATE
    qg_core
    Qt6::Test
    qg_warnings
)
add_test(NAME qg_identity_map COMMAND qg_identity_map_test)

# --- StateController: favorite/watched/progress (Prompt 30) ------------------
add_executable(qg_state_controller_test
    unit/test_state_controller.cpp
)
set_target_properties(qg_state_controller_test PROPERTIES AUTOMOC ON)
target_link_libraries(qg_state_controller_test PRIVATE
    qg_core
    Qt6::Test
    qg_warnings
)
add_test(NAME qg_state_controller COMMAND qg_state_controller_test)

# --- Per-folder settings persistence (Prompt 31) ----------------------------
add_executable(qg_settings_test
    unit/test_settings.cpp
)
set_target_properties(qg_settings_test PROPERTIES AUTOMOC ON)
target_link_libraries(qg_settings_test PRIVATE
    qg_core
    Qt6::Test
    qg_warnings
)
add_test(NAME qg_settings COMMAND qg_settings_test)

# --- Probe results -> model wiring + unknown handling (Prompt 32) ------------
add_executable(qg_probe_wiring_test
    unit/test_probe_wiring.cpp
)
set_target_properties(qg_probe_wiring_test PROPERTIES AUTOMOC ON)
target_link_libraries(qg_probe_wiring_test PRIVATE
    qg_core
    Qt6::Test
    qg_warnings
)
add_test(NAME qg_probe_wiring COMMAND qg_probe_wiring_test)

# --- Probe memory/performance with 100k entries (FIX-PERF-006) ---------------
add_executable(qg_probe_memory_test
    unit/test_probe_memory.cpp
)
set_target_properties(qg_probe_memory_test PROPERTIES AUTOMOC ON)
target_link_libraries(qg_probe_memory_test PRIVATE
    qg_core
    qg_playback
    Qt6::Test
    qg_warnings
)
add_test(NAME qg_probe_memory COMMAND qg_probe_memory_test)
set_tests_properties(qg_probe_memory PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# --- SortService + SortSpec deterministic order (Prompt 33) ------------------
add_executable(qg_sort_test
    unit/test_sort.cpp
)
set_target_properties(qg_sort_test PROPERTIES AUTOMOC ON)
target_link_libraries(qg_sort_test PRIVATE
    qg_core
    Qt6::Test
    qg_warnings
)
add_test(NAME qg_sort COMMAND qg_sort_test)

# --- Order menu -> model re-sort + per-folder persistence (Prompt 34) --------
add_executable(qg_sort_wiring_test
    unit/test_sort_wiring.cpp
)
set_target_properties(qg_sort_wiring_test PROPERTIES AUTOMOC ON)
target_link_libraries(qg_sort_wiring_test PRIVATE
    qg_core
    Qt6::Test
    qg_warnings
)
add_test(NAME qg_sort_wiring COMMAND qg_sort_wiring_test)

# --- SearchService (Prompt 39 / REMEDIATION-001) ----------------------------
add_executable(qg_search_service_test
    unit/test_search_service.cpp
)
set_target_properties(qg_search_service_test PROPERTIES AUTOMOC ON)
target_link_libraries(qg_search_service_test PRIVATE
    qg_core
    Qt6::Test
    qg_warnings
)
add_test(NAME qg_search_service
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/run_gl_test.sh $<TARGET_FILE:qg_search_service_test>
)

# --- SearchProxy (Prompt 40 / REMEDIATION-001) ------------------------------
add_executable(qg_search_proxy_test
    unit/test_search_proxy.cpp
)
set_target_properties(qg_search_proxy_test PROPERTIES AUTOMOC ON)
target_link_libraries(qg_search_proxy_test PRIVATE
    qg_core
    Qt6::Test
    qg_warnings
)
add_test(NAME qg_search_proxy
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/run_gl_test.sh $<TARGET_FILE:qg_search_proxy_test>
)

# --- SearchController (Prompt 41 / REMEDIATION-001) -------------------------
add_executable(qg_search_controller_test
    unit/test_search_controller.cpp
)
set_target_properties(qg_search_controller_test PROPERTIES AUTOMOC ON)
target_link_libraries(qg_search_controller_test PRIVATE
    qg_core
    Qt6::Test
    qg_warnings
)
add_test(NAME qg_search_controller
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/run_gl_test.sh $<TARGET_FILE:qg_search_controller_test>
)

# SelectionState test (Prompt 35). SelectionState is now part of qg_core
# (REMEDIATION-003), so the test links the library instead of compiling it.
add_executable(qg_selection_test
    unit/test_selection_state.cpp
)
set_target_properties(qg_selection_test PROPERTIES AUTOMOC ON)
target_link_libraries(qg_selection_test PRIVATE qg_core Qt6::Test qg_warnings)
add_test(NAME selection COMMAND qg_selection_test)

# --- Performance benchmarks (Prompt 69) -------------------------------------
add_executable(qg_performance_test
    performance/test_performance.cpp
)
set_target_properties(qg_performance_test PROPERTIES AUTOMOC ON)
target_link_libraries(qg_performance_test PRIVATE
    Qt6::Core
    Qt6::Test
    qg_core
    qg_warnings
)
add_test(NAME performance COMMAND qg_performance_test)
set_tests_properties(performance PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)


# --- Composition: CoreServices ownership + wiring (REMEDIATION-003) ----------
add_executable(qg_composition_test
    unit/test_composition.cpp
)
set_target_properties(qg_composition_test PROPERTIES AUTOMOC ON)
target_link_libraries(qg_composition_test PRIVATE
    qg_playback
    qg_core
    Qt6::Test
    qg_warnings
)
add_test(NAME qg_composition COMMAND qg_composition_test)
set_tests_properties(qg_composition PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)


# --- VideoSurfaceItem renderer lifecycle (REMEDIATION-005 / D-06, D-20) -------
add_executable(qg_video_surface_test
    unit/test_video_surface.cpp
)
set_target_properties(qg_video_surface_test PROPERTIES AUTOMOC ON)
target_link_libraries(qg_video_surface_test PRIVATE
    qg_playback
    qg_core
    Qt6::Test
    qg_warnings
)

# --- Offset-containment test (PLAY-VIS-003: frames at item rect, not window origin) ----
add_executable(qg_video_surface_offset_test
    unit/test_video_surface_offset.cpp
)
set_target_properties(qg_video_surface_offset_test PROPERTIES AUTOMOC ON)
target_link_libraries(qg_video_surface_offset_test PRIVATE
    qg_playback
    qg_core
    Qt6::Test
    Qt6::Gui
    Qt6::Quick
    qg_warnings
)
add_test(NAME qg_video_surface_offset
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/run_gl_test.sh $<TARGET_FILE:qg_video_surface_offset_test>
)
set_tests_properties(qg_video_surface_offset PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# --- Decoded-frame end-to-end render (REMEDIATION-005 / D-06, D-20) ------------
add_executable(qg_video_render_test
    unit/test_video_render.cpp
)
set_target_properties(qg_video_render_test PROPERTIES AUTOMOC ON)
target_link_libraries(qg_video_render_test PRIVATE
    qg_playback
    qg_core
    Qt6::Test
    Qt6::Gui
    Qt6::Quick
    qg_warnings
)
add_test(NAME qg_video_render
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/run_gl_test.sh $<TARGET_FILE:qg_video_render_test>
)
add_test(NAME qg_video_surface COMMAND qg_video_surface_test)
set_tests_properties(qg_video_surface PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)


# --- Playback single-handle + open/stop/reopen (REMEDIATION-005) --------------
add_executable(qg_playback_lifecycle_test
    unit/test_playback_lifecycle.cpp
)
set_target_properties(qg_playback_lifecycle_test PROPERTIES AUTOMOC ON)
target_link_libraries(qg_playback_lifecycle_test PRIVATE
    qg_playback
    qg_core
    Qt6::Test
    Qt6::Gui
    qg_warnings
)
add_test(NAME qg_playback_lifecycle COMMAND qg_playback_lifecycle_test)
set_tests_properties(qg_playback_lifecycle PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)


# --- Transport state machine: EOF/error/external-stop (REMEDIATION-006) -------
# Needs a real decoded stream (vo=libmpv), so it runs under run_gl_test.sh and
# QSKIPs when no GL context exists.
add_executable(qg_transport_state_test
    unit/test_transport_state.cpp
)
set_target_properties(qg_transport_state_test PROPERTIES AUTOMOC ON)
target_link_libraries(qg_transport_state_test PRIVATE
    qg_playback
    qg_core
    Qt6::Test
    Qt6::Gui
    Qt6::Quick
    qg_warnings
)
add_test(NAME qg_transport_state
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/run_gl_test.sh $<TARGET_FILE:qg_transport_state_test>
)

# --- Ordered playback context (REMEDIATION-007 / D-07, D-24) -----------------
add_executable(qg_playback_context_test
    unit/test_playback_context.cpp
)
set_target_properties(qg_playback_context_test PROPERTIES AUTOMOC ON)
target_link_libraries(qg_playback_context_test PRIVATE
    qg_playback
    qg_core
    Qt6::Test
    qg_warnings
)
add_test(NAME qg_playback_context
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/run_gl_test.sh $<TARGET_FILE:qg_playback_context_test>
)


# --- Reel projection model (REMEDIATION-007 / D-23) --------------------------
add_executable(qg_reel_model_test
    unit/test_reel_model.cpp
)
set_target_properties(qg_reel_model_test PROPERTIES AUTOMOC ON)
target_link_libraries(qg_reel_model_test PRIVATE
    qg_core
    Qt6::Test
    qg_warnings
)
add_test(NAME qg_reel_model
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/run_gl_test.sh $<TARGET_FILE:qg_reel_model_test>
)


# --- Reel / ordered-context integration (REMEDIATION-007 / D-07, D-23, D-24) --
add_executable(qg_reel_wiring_test
    unit/test_reel_wiring.cpp
)
set_target_properties(qg_reel_wiring_test PROPERTIES AUTOMOC ON)
target_link_libraries(qg_reel_wiring_test PRIVATE
    qg_playback
    qg_core
    Qt6::Test
    Qt6::Gui
    qg_warnings
)
add_test(NAME qg_reel_wiring
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/run_gl_test.sh $<TARGET_FILE:qg_reel_wiring_test>
)

# --- Resume policy + stored position (REMEDIATION-008 / D-26) ----------------
add_executable(qg_resume_policy_test
    unit/test_resume_policy.cpp
)
set_target_properties(qg_resume_policy_test PROPERTIES AUTOMOC ON)
target_link_libraries(qg_resume_policy_test PRIVATE
    qg_playback
    qg_core
    Qt6::Test
    qg_warnings
)
add_test(NAME qg_resume_policy
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/run_gl_test.sh $<TARGET_FILE:qg_resume_policy_test>
)


# --- Resume end-to-end on a real stream (REMEDIATION-008 / D-26) -------------
# Needs a decoded stream (vo=libmpv): runs under run_gl_test.sh, QSKIPs if no GL.
add_executable(qg_resume_playback_test
    unit/test_resume_playback.cpp
)
set_target_properties(qg_resume_playback_test PROPERTIES AUTOMOC ON)
target_link_libraries(qg_resume_playback_test PRIVATE
    qg_playback
    qg_core
    Qt6::Test
    Qt6::Gui
    Qt6::Quick
    qg_warnings
)
add_test(NAME qg_resume_playback
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/run_gl_test.sh $<TARGET_FILE:qg_resume_playback_test>
)

# --- Card actions → controllers (REMEDIATION-009 / D-04, D-25) ---------------
add_executable(qg_card_actions_test
    unit/test_card_actions.cpp
)
set_target_properties(qg_card_actions_test PROPERTIES AUTOMOC ON)
target_link_libraries(qg_card_actions_test PRIVATE
    qg_core
    qg_playback
    Qt6::Test
    Qt6::Quick
    qg_warnings
)
add_test(NAME qg_card_actions
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/run_gl_test.sh $<TARGET_FILE:qg_card_actions_test>
)

# --- Central keyboard routing (REMEDIATION-012 / D-11) -----------------------
add_executable(qg_keyboard_test
    unit/test_keyboard.cpp
)
set_target_properties(qg_keyboard_test PROPERTIES AUTOMOC ON)
target_link_libraries(qg_keyboard_test PRIVATE
    qg_playback
    qg_core
    Qt6::Test
    Qt6::Gui
    qg_warnings
)
add_test(NAME qg_keyboard COMMAND qg_keyboard_test)
set_tests_properties(qg_keyboard PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)


# --- Action feedback: dialogs, toasts, Trash + Undo (REMEDIATION-010 / D-08) --
add_executable(qg_action_feedback_test
    unit/test_action_feedback.cpp
)
set_target_properties(qg_action_feedback_test PROPERTIES AUTOMOC ON)
target_link_libraries(qg_action_feedback_test PRIVATE
    qg_core
    Qt6::Test
    Qt6::Gui
    qg_warnings
)
add_test(NAME qg_action_feedback COMMAND qg_action_feedback_test)
set_tests_properties(qg_action_feedback PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

