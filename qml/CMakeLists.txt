# QG QML module (URI `qg`).
#
# QML presentation only. Registered as a QML module so the application can load
# Main via loadFromModule("qg", "Main") and so tooling (qmllint, qmlimportscanner)
# sees the module structure.
#
# On-disk layout follows DESIGN.md §16 (qml/QG/...); QT_RESOURCE_ALIAS maps the
# files to the module root so the URI `qg` resolves cleanly.
#
# Theme.qml and Format.qml are singletons; mark them before qt_add_qml_module so
# the generated qmldir carries the `singleton` entries.

set_source_files_properties(QG/Theme.qml PROPERTIES
    QT_RESOURCE_ALIAS Theme.qml
    QT_QML_SINGLETON_TYPE TRUE)
set_source_files_properties(QG/Format.qml PROPERTIES
    QT_RESOURCE_ALIAS Format.qml
    QT_QML_SINGLETON_TYPE TRUE)
set_source_files_properties(QG/Icon.qml PROPERTIES QT_RESOURCE_ALIAS Icon.qml)
set_source_files_properties(QG/Main.qml PROPERTIES QT_RESOURCE_ALIAS Main.qml)
set_source_files_properties(QG/ContentView.qml PROPERTIES QT_RESOURCE_ALIAS ContentView.qml)
set_source_files_properties(QG/PathBar.qml PROPERTIES QT_RESOURCE_ALIAS PathBar.qml)
set_source_files_properties(QG/PlacesSidebar.qml PROPERTIES QT_RESOURCE_ALIAS PlacesSidebar.qml)
set_source_files_properties(QG/delegates/VideoCard.qml PROPERTIES QT_RESOURCE_ALIAS VideoCard.qml)
set_source_files_properties(QG/delegates/FolderCard.qml PROPERTIES QT_RESOURCE_ALIAS FolderCard.qml)
set_source_files_properties(QG/delegates/CompactRow.qml PROPERTIES QT_RESOURCE_ALIAS CompactRow.qml)
set_source_files_properties(QG/delegates/ManifestRow.qml PROPERTIES QT_RESOURCE_ALIAS ManifestRow.qml)
set_source_files_properties(QG/overlays/NoResults.qml PROPERTIES QT_RESOURCE_ALIAS NoResults.qml)
set_source_files_properties(QG/strips/ContextStrip.qml PROPERTIES QT_RESOURCE_ALIAS ContextStrip.qml)
set_source_files_properties(QG/strips/StatusStrip.qml PROPERTIES QT_RESOURCE_ALIAS StatusStrip.qml)
set_source_files_properties(QG/playback/Stage.qml PROPERTIES QT_RESOURCE_ALIAS Stage.qml)
set_source_files_properties(QG/playback/TransportBar.qml PROPERTIES QT_RESOURCE_ALIAS TransportBar.qml)
set_source_files_properties(QG/playback/ReelPanel.qml PROPERTIES QT_RESOURCE_ALIAS ReelPanel.qml)
set_source_files_properties(QG/overlays/ToastHost.qml PROPERTIES QT_RESOURCE_ALIAS ToastHost.qml)
set_source_files_properties(QG/overlays/StateOverlay.qml PROPERTIES QT_RESOURCE_ALIAS StateOverlay.qml)
set_source_files_properties(QG/dialogs/PropertiesDialog.qml PROPERTIES QT_RESOURCE_ALIAS PropertiesDialog.qml)
set_source_files_properties(QG/dialogs/RenameConflictDialog.qml PROPERTIES QT_RESOURCE_ALIAS RenameConflictDialog.qml)
set_source_files_properties(QG/dialogs/ConfirmDeleteDialog.qml PROPERTIES QT_RESOURCE_ALIAS ConfirmDeleteDialog.qml)

# Use the modern '/qt/qml/' resource prefix (QTP0001 NEW).
qt_policy(SET QTP0001 NEW)

qt_add_library(qg_qml STATIC)

# qt_add_qml_module extracts C++ metatypes, which needs AUTOMOC.
set_target_properties(qg_qml PROPERTIES AUTOMOC ON)

qt_add_qml_module(qg_qml
    URI qg
    VERSION 1.0
    QML_FILES
        QG/Theme.qml
        QG/Format.qml
        QG/Icon.qml
        QG/Main.qml
        QG/ContentView.qml
        QG/PathBar.qml
        QG/PlacesSidebar.qml
        QG/delegates/VideoCard.qml
        QG/delegates/FolderCard.qml
        QG/delegates/CompactRow.qml
        QG/delegates/ManifestRow.qml
        QG/overlays/NoResults.qml
        QG/strips/ContextStrip.qml
        QG/strips/StatusStrip.qml
        QG/playback/Stage.qml
        QG/playback/TransportBar.qml
        QG/playback/ReelPanel.qml
QG/overlays/ToastHost.qml
QG/overlays/StateOverlay.qml
QG/dialogs/PropertiesDialog.qml
QG/dialogs/RenameConflictDialog.qml
QG/dialogs/ConfirmDeleteDialog.qml
)

# Monoline icon SVGs (DESIGN.md §2.3) shipped under a stable qrc prefix so
# Icon.qml can resolve them at runtime. BASE flattens each file to its leaf name
# beneath /qt/qml/qg/icons/.
qt_add_resources(qg_qml "icons"
    PREFIX "/qt/qml/qg/icons"
    BASE "../resources/icons/monoline"
    FILES
        ../resources/icons/monoline/activity.svg
        ../resources/icons/monoline/chevron-right.svg
        ../resources/icons/monoline/close.svg
        ../resources/icons/monoline/copy.svg
        ../resources/icons/monoline/cut.svg
        ../resources/icons/monoline/delete-permanent.svg
        ../resources/icons/monoline/density.svg
        ../resources/icons/monoline/density-compact.svg
        ../resources/icons/monoline/density-gallery.svg
        ../resources/icons/monoline/density-standard.svg
        ../resources/icons/monoline/device.svg
        ../resources/icons/monoline/edit.svg
        ../resources/icons/monoline/eject.svg
        ../resources/icons/monoline/favorite.svg
        ../resources/icons/monoline/folder.svg
        ../resources/icons/monoline/fullscreen.svg
        ../resources/icons/monoline/lock.svg
        ../resources/icons/monoline/more.svg
        ../resources/icons/monoline/mute.svg
        ../resources/icons/monoline/next.svg
        ../resources/icons/monoline/open-with.svg
        ../resources/icons/monoline/parent.svg
        ../resources/icons/monoline/pause.svg
        ../resources/icons/monoline/placeholder.svg
        ../resources/icons/monoline/play.svg
        ../resources/icons/monoline/prev.svg
        ../resources/icons/monoline/properties.svg
        ../resources/icons/monoline/refresh.svg
        ../resources/icons/monoline/reveal.svg
        ../resources/icons/monoline/search.svg
        ../resources/icons/monoline/seek-back.svg
        ../resources/icons/monoline/seek-forward.svg
        ../resources/icons/monoline/sort.svg
        ../resources/icons/monoline/trash.svg
        ../resources/icons/monoline/volume.svg
        ../resources/icons/monoline/warn.svg
        ../resources/icons/monoline/watched.svg
        # Sidebar semantics (FIX-UI-003): Places, Pinned/Recent, device kinds and
        # the unmount affordance. Every name in Icon.qml must appear here.
        ../resources/icons/monoline/documents.svg
        ../resources/icons/monoline/desktop.svg
        ../resources/icons/monoline/downloads.svg
        ../resources/icons/monoline/drive-removable.svg
        ../resources/icons/monoline/drive.svg
        ../resources/icons/monoline/home.svg
        ../resources/icons/monoline/network.svg
        ../resources/icons/monoline/pin.svg
        ../resources/icons/monoline/recent.svg
        ../resources/icons/monoline/unmount.svg
        ../resources/icons/monoline/videos.svg
)

target_link_libraries(qg_qml PUBLIC
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::QuickEffects
)
