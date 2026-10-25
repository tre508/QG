#pragma once

// Metadata probing (DESIGN.md §18.6): duration, coded/display resolution,
// rotation and track counts, obtained in-process with FFmpeg libraries — never
// through the playback instance. Results are cached by identity + source
// mtime/size; probing is lower priority than thumbnails and bounded.

#include "core/errors.hpp"
#include "core/types.hpp"

#include <QObject>
#include <QQueue>
#include <QString>

#include <functional>

namespace qg {

class Database;

struct ProbeResult {
    qint64 durationMs = -1;
    int width = 0;
    int height = 0;
    int rotation = 0;
    int audioTracks = 0;
    Error error;

    bool ok() const noexcept { return !error.isError() && durationMs >= 0; }
};

// In-process probe (stream parameters first; a decoder is opened only when
// needed). Returns a typed error for malformed input.
ProbeResult probeMedia(const QString &path);

class MediaProbeService : public QObject {
    Q_OBJECT

public:
    using Prober = std::function<ProbeResult(const QString &)>;

    explicit MediaProbeService(QObject *parent = nullptr);

    void setDatabase(Database *database) { m_database = database; }
    void setProber(Prober prober) { m_prober = std::move(prober); }
    void setImmediateProcessing(bool immediate) { m_immediate = immediate; }
    void setMaxConcurrency(int maximum) { m_maxConcurrent = maximum > 0 ? maximum : 1; }

    // Request a probe for an entry (cached when the source is unchanged).
    void request(const Entry &entry);
    void cancelAll();

    int pendingCount() const { return static_cast<int>(m_queue.size()); }
    int activeCount() const { return m_active; }

    // Deterministic synchronous drain for tests.
    void processAllForTesting();

signals:
    void probeReady(const QString &identity, qint64 durationMs, int width, int height);
    void probeFailed(const QString &identity);

private:
    ProbeResult runProbe(const Entry &entry) const;
    void process(const Entry &entry);
    void pump();

    Database *m_database = nullptr;
    Prober m_prober;
    QQueue<Entry> m_queue;
    int m_active = 0;
    int m_maxConcurrent = 2;
    bool m_immediate = false;
    quint64 m_generation = 0;
};

} // namespace qg
