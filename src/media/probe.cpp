#include "media/probe.hpp"

#include "persistence/database.hpp"
#include "persistence/repositories.hpp"
#include "workers/priority_pool.hpp"
#include "workers/tasks.hpp"

#include <QFile>

#include <algorithm>
#include <cmath>
#include <memory>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/display.h>
}

namespace qg {
namespace {

struct FormatCloser {
    void operator()(AVFormatContext *context) const
    {
        if (context != nullptr) {
            avformat_close_input(&context);
        }
    }
};

int rotationOf(const AVStream *stream)
{
    const AVCodecParameters *parameters = stream->codecpar;
    if (parameters != nullptr) {
        for (int i = 0; i < parameters->nb_coded_side_data; ++i) {
            const AVPacketSideData &side = parameters->coded_side_data[i];
            if (side.type == AV_PKT_DATA_DISPLAYMATRIX && side.data != nullptr
                && side.size >= static_cast<int>(9 * sizeof(int32_t))) {
                const double degrees =
                    av_display_rotation_get(reinterpret_cast<const int32_t *>(side.data));
                if (!std::isnan(degrees)) {
                    return static_cast<int>(-degrees) % 360;
                }
            }
        }
    }
    const AVDictionaryEntry *tag = av_dict_get(stream->metadata, "rotate", nullptr, 0);
    if (tag != nullptr) {
        return QString::fromUtf8(tag->value).toInt();
    }
    return 0;
}

} // namespace

ProbeResult probeMedia(const QString &path)
{
    ProbeResult result;

    AVFormatContext *rawFormat = nullptr;
    // Bounded probe budget: FFmpeg defaults are already conservative (5 MB /
    // 5 s). Make them explicit so the probe never spends unbounded time on a
    // file, and log the configuration rather than relying on implicit defaults
    // (FIX-RUNTIME-002-B). Pixel format is intentionally not required — QG
    // reads width/height/duration directly from AVCodecParameters.
    AVDictionary *opts = nullptr;
    av_dict_set(&opts, "probesize", "5000000", 0);
    av_dict_set(&opts, "analyzeduration", "5000000", 0);
    // avformat_open_input() consumes the options dictionary per the FFmpeg
    // API contract — do not av_dict_free() it here on either path.
    if (avformat_open_input(&rawFormat, QFile::encodeName(path).constData(), nullptr, &opts) != 0) {
        result.error = Error::make(ErrorCode::UnsupportedMedia,
                                   QStringLiteral("Cannot open media"),
                                   path);
        return result;
    }
    std::unique_ptr<AVFormatContext, FormatCloser> format(rawFormat);

    if (avformat_find_stream_info(format.get(), nullptr) < 0) {
        result.error = Error::make(ErrorCode::UnsupportedMedia,
                                   QStringLiteral("Cannot read stream info"),
                                   path);
        return result;
    }

    const int videoStream = av_find_best_stream(format.get(), AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (videoStream < 0) {
        result.error = Error::make(ErrorCode::UnsupportedMedia,
                                   QStringLiteral("No video stream"),
                                   path);
        return result;
    }

    const AVStream *stream = format->streams[videoStream];
    result.width = stream->codecpar->width;
    result.height = stream->codecpar->height;
    result.rotation = rotationOf(stream);

    if (format->duration > 0) {
        result.durationMs = format->duration / (AV_TIME_BASE / 1000);
    }

    for (unsigned i = 0; i < format->nb_streams; ++i) {
        if (format->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            ++result.audioTracks;
        }
    }

    return result;
}

// ---------------------------------------------------------------------------
// MediaProbeService
// ---------------------------------------------------------------------------

MediaProbeService::MediaProbeService(QObject *parent)
    : QObject(parent)
{
}

ProbeResult MediaProbeService::runProbe(const Entry &entry) const
{
    if (m_prober) {
        return m_prober(entry.path);
    }
    return probeMedia(entry.path);
}

void MediaProbeService::request(const Entry &entry)
{
    if (entry.identity.isEmpty()) {
        return;
    }
    if (m_immediate) {
        process(entry);
        return;
    }
    m_queue.enqueue(entry);
    pump();
}

void MediaProbeService::cancelAll()
{
    ++m_generation;
    m_queue.clear();
}

void MediaProbeService::processAllForTesting()
{
    while (!m_queue.isEmpty()) {
        process(m_queue.dequeue());
    }
}

void MediaProbeService::process(const Entry &entry)
{
    // Serve from cache when the source is unchanged.
    if (m_database != nullptr) {
        PathRepository paths(*m_database);
        QString identity;
        qint64 size = 0;
        qint64 mtimeNs = 0;
        if (paths.pathInfo(entry.path, &identity, &size, &mtimeNs)
            && identity == entry.identity && size == entry.size && mtimeNs == entry.mtimeNs) {
            MediaStateRepository media(*m_database);
            qint64 durationMs = -1;
            int width = 0;
            int height = 0;
            if (media.probeResult(entry.identity, &durationMs, &width, &height)) {
                emit probeReady(entry.identity, durationMs, width, height);
                return;
            }
        }
    }

    const ProbeResult probed = runProbe(entry);
    if (probed.ok()) {
        if (m_database != nullptr) {
            MediaStateRepository media(*m_database);
            media.setProbeResult(entry.identity, probed.durationMs, probed.width, probed.height);
            PathRepository paths(*m_database);
            paths.upsert(entry.path, entry.identity, 0, 0, entry.size, entry.mtimeNs);
        }
        emit probeReady(entry.identity, probed.durationMs, probed.width, probed.height);
    } else {
        emit probeFailed(entry.identity);
    }
}

void MediaProbeService::pump()
{
    while (m_active < m_maxConcurrent && !m_queue.isEmpty()) {
        const Entry entry = m_queue.dequeue();
        ++m_active;
        const quint64 generation = m_generation;

        submitTask(WorkerPools::cpuMedia(),
                   TaskPriority::Background,
                   nullptr,
                   this,
                   [this, entry]() { runProbe(entry); },
                   [this, entry, generation]() {
                       --m_active;
                       if (generation == m_generation) {
                           process(entry);
                       }
                       pump();
                   });
    }
}

} // namespace qg
