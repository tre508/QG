#pragma once

// Probe → model wiring (DESIGN.md §18.6, Prompt 32).
//
// Owns a `MediaProbeService` and applies its validated results to the canonical
// model with minimal `dataChanged` (duration/resolution roles). Results are
// delivered on the GUI thread (the service probes on the CPU/media pool, never
// the GUI thread). A small debounce collapses a burst of probe completions into
// a single `resortRequested` so a duration sort re-orders once, not per probe.

#include "core/types.hpp"
#include "media/probe.hpp"

#include <QHash>
#include <QObject>
#include <QTimer>

namespace qg {

class Database;
class FolderModel;

class ProbeController : public QObject {
    Q_OBJECT

public:
    explicit ProbeController(QObject *parent = nullptr);

    void setModel(FolderModel *model) { m_model = model; }
    void setDatabase(Database *database) { m_service.setDatabase(database); }
    void setProber(MediaProbeService::Prober prober) { m_service.setProber(std::move(prober)); }
    void setImmediateProcessing(bool immediate) { m_service.setImmediateProcessing(immediate); }

    // New navigation: cancel outstanding probes and drop in-flight results.
    void beginGeneration();
    void cancelAll() { beginGeneration(); }

    // Request probes for rows in [firstIndex, lastIndex] (videos only).
    // QML-facing: the ContentView reports its viewport range and Main.qml routes
    // it here. Rows already known (duration present) or already requested this
    // generation are skipped, so scrolling does not re-probe settled rows.
    Q_INVOKABLE void requestVisible(int firstIndex, int lastIndex);

    int pendingCount() const { return m_service.pendingCount(); }
    int activeCount() const { return m_service.activeCount(); }

    // Deterministic synchronous drain for tests.
    void processAllForTesting() { m_service.processAllForTesting(); }

signals:
    void probeApplied(const QString &identity);
    // Single coalesced resort request (§20.2): debounced so a probe burst
    // yields one re-sort, not one per result.
    void resortRequested();

private:
    void onProbeReady(const QString &identity, qint64 durationMs, int width, int height);

    MediaProbeService m_service;
    FolderModel *m_model = nullptr;
    // identity -> source (mtime_ns, size) captured at request time, for
    // source-change validation on delivery.
    QHash<QString, QPair<qint64, qint64>> m_requestedSource;
    QTimer m_resortTimer;
};

} // namespace qg