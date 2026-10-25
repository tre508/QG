// QG entry point (DESIGN.md 15.1, 23.1).
//
// Responsibilities are deliberately minimal: create the QGuiApplication, force
// the Qt Quick OpenGL scene-graph backend BEFORE any QQuickWindow exists, then
// hand control to App. Business logic lives in App and its services.

#include <QGuiApplication>
#include <QQuickWindow>
#include <QSGRendererInterface>

#include "app.hpp"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("QG"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("QG"));
    QGuiApplication::setOrganizationName(QStringLiteral("QGayze"));
    QGuiApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    // libmpv's Render API and the Qt Quick scene graph must share one OpenGL
    // pipeline (DEC-006). This must be set before the first QQuickWindow.
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

    qg::App qgApp;
    if (!qgApp.initialize()) {
        return 1;
    }
    return qgApp.exec();
}
