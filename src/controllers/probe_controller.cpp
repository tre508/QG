#include "controllers/probe_controller.hpp"

#include "models/folder_model.hpp"

#include <algorithm>
#include <optional>

namespace qg {

ProbeController::ProbeController(QObject *parent)
    : QObject(parent)
{
    connect(&m_service, &MediaProbeService::probeReady, this, &ProbeController::onProbeReady);

    // Debounce: restarting a single-shot timer collapses a burst of probe
    // completions into one resort request (DESIGN.md §20.2).
    m_resortTimer.setSingleShot(true);
    m_resortTimer.setInterval(200);
    connect(&m_resortTimer, &QTimer::timeout, this, &ProbeController::resortRequested);
}

void ProbeController::beginGeneration()
{
    m_requestedSource.clear();
    m_service.cancelAll();
    m_resortTimer.stop();
}

void ProbeController::requestVisible(int firstIndex, int lastIndex)
{
    if (m_model == nullptr || m_model->rowCount() == 0) {
        return;
    }

    const int count = m_model->rowCount();
    const int first = qMax(0, firstIndex);
    const int last = qMin(count - 1, lastIndex);
    if (last < first) {
        return;
    }

    for (int index = first; index <= last; ++index) {
        const Entry *entry = m_model->entryAt(index);
        if (entry == nullptr || entry->identity.isEmpty() || !entry->isVideo()) {
            continue;
        }
        // Skip rows whose duration is already known, and rows already requested
        // this generation: re-requesting on every scroll would re-probe settled
        // items (DESIGN §18.6). m_requestedSource is cleared on navigation.
        if (entry->durationMs.has_value() || m_requestedSource.contains(entry->identity)) {
            continue;
        }
        m_requestedSource.insert(entry->identity, {entry->mtimeNs, entry->size});
        m_service.request(*entry);
    }
}

void ProbeController::onProbeReady(const QString &identity,
                                   qint64 durationMs,
                                   int width,
                                   int height)
{
    if (m_model == nullptr) {
        return;
    }

    const int row = m_model->indexOfIdentity(identity);
    if (row < 0) {
        return; // entry removed after the request was made
    }

    const Entry *entry = m_model->entryAt(row);
    auto it = m_requestedSource.constFind(identity);
    if (it != m_requestedSource.constEnd() && entry != nullptr
        && (entry->mtimeNs != it->first || entry->size != it->second)) {
        return; // source changed while the probe ran: drop stale metadata
    }

    m_model->updateProbe(identity,
                         std::optional<qint64>(durationMs),
                         std::optional<int>(width),
                         std::optional<int>(height));
    emit probeApplied(identity);
    m_resortTimer.start();
}

} // namespace qg