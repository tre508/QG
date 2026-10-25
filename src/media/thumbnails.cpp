#include "media/thumbnails.hpp"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUrl>

#include <algorithm>
#include <memory>

#include <sys/stat.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

namespace qg {
namespace {

QString md5Hex(const QByteArray &data)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(data, QCryptographicHash::Md5).toHex());
}

QString bucketDirectory(int bucket)
{
    switch (bucket) {
    case 512:
        return QStringLiteral("x-large");
    case 256:
        return QStringLiteral("large");
    default:
        return QStringLiteral("normal");
    }
}

} // namespace

QString encodeThumbnailKey(const ThumbnailKey &key)
{
    return QStringLiteral("v%1|%2|%3|%4")
        .arg(key.version)
        .arg(key.mtimeNs)
        .arg(key.size)
        .arg(key.identity);
}

QString qgThumbnailPath(const QString &qgCacheDir, const ThumbnailKey &key)
{
    const QString fileName = md5Hex(encodeThumbnailKey(key).toUtf8()) + QStringLiteral(".png");
    return QDir(QDir(qgCacheDir).filePath(QStringLiteral("thumbs"))).filePath(fileName);
}

QString freedesktopUri(const QString &sourcePath)
{
    return QUrl::fromLocalFile(sourcePath).toString(QUrl::FullyEncoded);
}

QString freedesktopCacheKey(const QString &uri)
{
    return md5Hex(uri.toUtf8());
}

QString freedesktopThumbnailPath(const QString &cacheHome, const QString &sourcePath, int bucket)
{
    const QString stem = freedesktopCacheKey(freedesktopUri(sourcePath));
    return QDir(cacheHome)
        .filePath(QStringLiteral("thumbnails/%1/%2.png").arg(bucketDirectory(bucket), stem));
}

FreedesktopThumbnail findFreedesktopThumbnail(const QString &cacheHome,
                                              const QString &sourcePath,
                                              qint64 sourceMtimeNs,
                                              const QList<int> &buckets)
{
    FreedesktopThumbnail result;

    const QString uri = freedesktopUri(sourcePath);
    const QString mtimeSeconds = QString::number(sourceMtimeNs / 1000000000LL);

    for (int bucket : buckets) {
        const QString path = freedesktopThumbnailPath(cacheHome, sourcePath, bucket);
        if (!QFileInfo::exists(path)) {
            continue;
        }

        QImage image;
        if (!image.load(path)) {
            continue; // unreadable/malformed: ignore, never crash
        }

        if (image.text(QStringLiteral("Thumb::URI")) != uri) {
            continue;
        }
        if (image.text(QStringLiteral("Thumb::MTime")) != mtimeSeconds) {
            continue;
        }

        result.valid = true;
        result.path = path;
        result.image = image;
        return result;
    }

    return result;
}

namespace {

qint64 mtimeNsOf(const struct stat &st)
{
#if defined(__linux__)
    return static_cast<qint64>(st.st_mtim.tv_sec) * 1000000000LL
           + static_cast<qint64>(st.st_mtim.tv_nsec);
#else
    return static_cast<qint64>(st.st_mtime) * 1000000000LL;
#endif
}

struct FormatCloser {
    void operator()(AVFormatContext *context) const
    {
        if (context != nullptr) {
            avformat_close_input(&context);
        }
    }
};

struct CodecCloser {
    void operator()(AVCodecContext *context) const
    {
        if (context != nullptr) {
            avcodec_free_context(&context);
        }
    }
};

struct PacketDeleter {
    void operator()(AVPacket *packet) const
    {
        if (packet != nullptr) {
            av_packet_free(&packet);
        }
    }
};

struct FrameDeleter {
    void operator()(AVFrame *frame) const
    {
        if (frame != nullptr) {
            av_frame_free(&frame);
        }
    }
};

struct SwsCloser {
    void operator()(SwsContext *context) const
    {
        if (context != nullptr) {
            sws_freeContext(context);
        }
    }
};

} // namespace

ThumbnailExtraction extractThumbnail(const QString &path,
                                     int targetWidth,
                                     int targetHeight,
                                     qint64 posterMs)
{
    ThumbnailExtraction result;

    struct stat status {};
    if (::stat(QFile::encodeName(path).constData(), &status) == 0) {
        result.sourceSize = static_cast<qint64>(status.st_size);
        result.sourceMtimeNs = mtimeNsOf(status);
    }

    const QByteArray encodedPath = QFile::encodeName(path);

    AVFormatContext *rawFormat = nullptr;
    if (avformat_open_input(&rawFormat, encodedPath.constData(), nullptr, nullptr) != 0) {
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

    const AVCodecParameters *parameters = format->streams[videoStream]->codecpar;
    const AVCodec *decoder = avcodec_find_decoder(parameters->codec_id);
    if (decoder == nullptr) {
        result.error = Error::make(ErrorCode::UnsupportedMedia,
                                   QStringLiteral("No decoder for stream"),
                                   path);
        return result;
    }

    std::unique_ptr<AVCodecContext, CodecCloser> codec(avcodec_alloc_context3(decoder));
    if (codec == nullptr || avcodec_parameters_to_context(codec.get(), parameters) < 0
        || avcodec_open2(codec.get(), decoder, nullptr) < 0) {
        result.error = Error::make(ErrorCode::OperationFailed,
                                   QStringLiteral("Cannot open decoder"),
                                   path);
        return result;
    }

    const qint64 targetUs = posterMs >= 0
                                ? posterMs * 1000
                                : (format->duration > 0 ? format->duration / 10 : 0);
    av_seek_frame(format.get(), -1, targetUs, AVSEEK_FLAG_BACKWARD);

    std::unique_ptr<AVPacket, PacketDeleter> packet(av_packet_alloc());
    std::unique_ptr<AVFrame, FrameDeleter> frame(av_frame_alloc());
    if (packet == nullptr || frame == nullptr) {
        result.error = Error::make(ErrorCode::OperationFailed,
                                   QStringLiteral("Cannot allocate FFmpeg buffers"),
                                   path);
        return result;
    }

    bool decoded = false;
    while (av_read_frame(format.get(), packet.get()) >= 0) {
        if (packet->stream_index == videoStream) {
            if (avcodec_send_packet(codec.get(), packet.get()) == 0
                && avcodec_receive_frame(codec.get(), frame.get()) == 0) {
                decoded = true;
            }
        }
        av_packet_unref(packet.get());
        if (decoded) {
            break;
        }
    }

    if (!decoded) {
        result.error = Error::make(ErrorCode::OperationFailed,
                                   QStringLiteral("No decodable frame"),
                                   path);
        return result;
    }

    const int width = std::max(1, targetWidth);
    const int height = std::max(1, targetHeight);
    std::unique_ptr<SwsContext, SwsCloser> scaler(
        sws_getContext(frame->width,
                       frame->height,
                       static_cast<AVPixelFormat>(frame->format),
                       width,
                       height,
                       AV_PIX_FMT_RGBA,
                       SWS_BILINEAR,
                       nullptr,
                       nullptr,
                       nullptr));
    if (scaler == nullptr) {
        result.error = Error::make(ErrorCode::OperationFailed,
                                   QStringLiteral("Cannot create scaler"),
                                   path);
        return result;
    }

    QImage image(width, height, QImage::Format_RGBA8888);
    uint8_t *destination[4] = {image.bits(), nullptr, nullptr, nullptr};
    int destinationLinesize[4] = {static_cast<int>(image.bytesPerLine()), 0, 0, 0};
    sws_scale(scaler.get(),
              frame->data,
              frame->linesize,
              0,
              frame->height,
              destination,
              destinationLinesize);

    result.image = image;
    return result;
}

// ---------------------------------------------------------------------------
// ThumbnailScheduler
// ---------------------------------------------------------------------------

void ThumbnailScheduler::request(int index, const ThumbnailRequest &request)
{
    if (index < 0) {
        return;
    }
    Entry entry;
    entry.request = request;
    entry.request.index = index;
    entry.request.identity = request.identity;
    entry.sequence = m_sequence++;
    entry.priority = Priority::Normal;
    m_pending.insert(index, entry);
    recomputePriorities(60);
}

void ThumbnailScheduler::setViewport(int firstVisible, int lastVisible, int totalCount, int horizon)
{
    Q_UNUSED(totalCount);
    m_viewFirst = firstVisible;
    m_viewLast = lastVisible;
    recomputePriorities(horizon);
}

void ThumbnailScheduler::recomputePriorities(int horizon)
{
    if (m_viewFirst < 0 || m_viewLast < m_viewFirst) {
        return;
    }

    const int span = std::max(1, m_viewLast - m_viewFirst + 1);
    QList<int> dropped;

    for (auto it = m_pending.begin(); it != m_pending.end(); ++it) {
        const int index = it.key();
        if (index >= m_viewFirst && index <= m_viewLast) {
            it->priority = Priority::Interactive;
            continue;
        }
        const int distance = index < m_viewFirst ? (m_viewFirst - index) : (index - m_viewLast);
        if (distance <= span) {
            it->priority = Priority::Normal;
        } else if (distance <= horizon) {
            it->priority = Priority::Background;
        } else {
            dropped.append(index);
        }
    }

    for (int index : dropped) {
        m_pending.remove(index);
    }
}

void ThumbnailScheduler::clear()
{
    m_pending.clear();
    m_viewFirst = -1;
    m_viewLast = -1;
}

int ThumbnailScheduler::priorityOf(int index) const
{
    const auto it = m_pending.constFind(index);
    if (it == m_pending.constEnd()) {
        return -1;
    }
    return static_cast<int>(it->priority);
}

std::optional<ThumbnailRequest> ThumbnailScheduler::takeNext()
{
    if (m_pending.isEmpty()) {
        return std::nullopt;
    }

    auto best = m_pending.end();
    for (auto it = m_pending.begin(); it != m_pending.end(); ++it) {
        if (best == m_pending.end()
            || static_cast<int>(it->priority) < static_cast<int>(best->priority)
            || (it->priority == best->priority && it->sequence < best->sequence)) {
            best = it;
        }
    }

    const ThumbnailRequest request = best->request;
    m_pending.erase(best);
    return request;
}

void ThumbnailScheduler::setDecodedCapacity(int count)
{
    m_capacity = std::max(1, count);
    while (m_lru.size() > m_capacity) {
        const QString victim = m_lru.takeLast();
        m_decoded.remove(victim);
    }
}

void ThumbnailScheduler::storeDecoded(const QString &identity, const QImage &image)
{
    if (image.isNull()) {
        return;
    }
    m_lru.removeAll(identity);
    m_lru.prepend(identity);
    m_decoded.insert(identity, image);
    while (m_lru.size() > m_capacity) {
        const QString victim = m_lru.takeLast();
        m_decoded.remove(victim);
    }
}

bool ThumbnailScheduler::hasDecoded(const QString &identity) const
{
    return m_decoded.contains(identity);
}

QImage ThumbnailScheduler::decoded(const QString &identity) const
{
    return m_decoded.value(identity);
}

ThumbnailAction decideThumbnail(const ThumbnailCacheState &cache,
                                qint64 sourceMtimeNs,
                                qint64 sourceSize,
                                int maxRetries,
                                bool manualRetry)
{
    if (!cache.present) {
        return ThumbnailAction::Generate;
    }

    const bool sourceChanged =
        cache.sourceMtimeNs != sourceMtimeNs || cache.sourceSize != sourceSize;

    if (cache.state == QLatin1String("ok")) {
        return sourceChanged ? ThumbnailAction::Generate : ThumbnailAction::Serve;
    }

    // Failed marker.
    if (sourceChanged || manualRetry) {
        return ThumbnailAction::Retry;
    }
    if (cache.failCount <= maxRetries) {
        return ThumbnailAction::Retry;
    }
    return ThumbnailAction::Terminal;
}

} // namespace qg
