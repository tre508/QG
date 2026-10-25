// Force-import the statically linked `qg` QML module plugin (Prompt 03).
//
// The executable does not reference any QML type at compile time (the shell is
// loaded dynamically via QQmlApplicationEngine::loadFromModule), so the linker
// would otherwise drop the plugin's type registration and embedded QML
// resources, and `import qg` would fail with "No module named qg found".

#include <QtPlugin>

Q_IMPORT_PLUGIN(qgPlugin)
