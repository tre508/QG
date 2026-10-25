#pragma once

// QG application object: the ownership root for startup/shutdown and the C++
// services, models and controllers wired into QML (DESIGN.md §15.1, §23.1).

#include <memory>

#include "config.hpp"

class QQmlApplicationEngine;

namespace qg {

class BookmarkRepository;
class BrowserController;
class CoreServices;
class Database;
class MountService;
class PlacesModel;
class RecentRepository;
class SettingsRepository;

class App {
public:
    App();
    ~App();

    App(const App &) = delete;
    App &operator=(const App &) = delete;

    // Resolve configuration, create directories, initialize logging, open the
    // database, build the browser/places/mount services, compose the remaining
    // subsystems (CoreServices), load the QML shell and inject every controller
    // into it. Returns false (with a logged error) if startup fails.
    bool initialize();

    // Enter the Qt event loop. Call only after a successful initialize().
    int exec();

    // Register the C++ types exposed to QML under the `qg.core` module. Kept
    // separate from initialize() so tests can verify the QML type surface
    // (review.md D-19) without starting the full application.
    static void registerQmlTypes();

    const Config &config() const { return m_config; }

private:
    Config m_config;

    // Declared before m_engine so the engine (and therefore QML, which holds
    // pointers to the controllers) is destroyed first.
    std::unique_ptr<Database> m_database;
    std::unique_ptr<RecentRepository> m_recent;
    std::unique_ptr<SettingsRepository> m_settings;
    std::unique_ptr<BookmarkRepository> m_bookmarks;
    std::unique_ptr<BrowserController> m_browser;
    std::unique_ptr<PlacesModel> m_places;
    std::unique_ptr<MountService> m_mounts;
    std::unique_ptr<CoreServices> m_services;
    std::unique_ptr<QQmlApplicationEngine> m_engine;

    bool m_initialized = false;
};

} // namespace qg