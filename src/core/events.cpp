#include "core/events.hpp"

#include <QMetaObject>
#include <QThread>

namespace qg {

EventBus::EventBus(QObject *parent)
    : QObject(parent)
{
    // Allow AppEvent to travel through queued connections.
    qRegisterMetaType<AppEvent>("qg::AppEvent");
    qRegisterMetaType<AppEventType>("qg::AppEventType");
}

void EventBus::post(const AppEvent &event)
{
    if (QThread::currentThread() == thread()) {
        deliver(event);
        return;
    }

    QMetaObject::invokeMethod(
        this,
        [this, event]() { deliver(event); },
        Qt::QueuedConnection);
}

void EventBus::deliver(const AppEvent &event)
{
    emit announced(event);
}

} // namespace qg
