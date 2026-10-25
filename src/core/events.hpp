#pragma once

// Typed app-wide announcement bridge (DESIGN.md §14.4, §16).
//
// A single EventBus lets services/workers report user-facing events (info,
// success, warning, error, progress) without depending on any UI type. Delivery
// always happens on the bus's owning thread, so QML can bind safely.

#include <QMetaType>
#include <QObject>
#include <QString>

namespace qg {

enum class AppEventType {
    Info,
    Success,
    Warning,
    Error,
    Progress,
};

struct AppEvent {
    AppEventType type = AppEventType::Info;
    QString message;
    QString detail;
};

class EventBus : public QObject {
    Q_OBJECT

public:
    explicit EventBus(QObject *parent = nullptr);

    // Thread-safe. May be called from any thread; the signal is emitted on the
    // bus's owning thread.
    void post(const AppEvent &event);

signals:
    void announced(const qg::AppEvent &event);

private:
    void deliver(const AppEvent &event);
};

} // namespace qg

Q_DECLARE_METATYPE(qg::AppEvent)
Q_DECLARE_METATYPE(qg::AppEventType)
