// Entry point for the Qt Quick Test binary (Prompt 03).
// QUICK_TEST_MAIN generates main(); the test cases live in tst_*.qml next to it.
//
// The `qg` QML module is a *static* module, so its plugin and embedded QML
// resources must be force-imported into this test binary. Without
// Q_IMPORT_PLUGIN the linker drops the module's resource registration and
// `import qg` fails with "plugin qg_qmlplugin not found".
//
// The `qg.core` runtime types (playback/search/etc.) are registered by the
// application at startup; the tests register them here too so a shell that
// references them (e.g. Stage's VideoSurfaceItem) loads (REMEDIATION-005).

#include <QtPlugin>
#include <QtQuickTest/quicktest.h>

#include "core/composition.hpp"

Q_IMPORT_PLUGIN(qgPlugin)

static const bool s_coreTypesRegistered = []() {
    qg::registerCoreQmlTypes();
    return true;
}();

QUICK_TEST_MAIN(qg_qml_tests)