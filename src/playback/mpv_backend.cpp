#include "mpv_backend.hpp"
#include "video_surface.hpp"

#include <clocale>
#include <mpv/client.h>
#include <mpv/render_gl.h>
#include <QCoreApplication>
#include <QEvent>
#include <QMetaObject>
#include <QVariant>
#include <QDebug>

// Event sent from the mpv wakeup callback to the Qt thread.
class MpvIdleEvent : public QEvent {
public:
    static const QEvent::Type kType = static_cast<QEvent::Type>(QEvent::User + 1);
    MpvIdleEvent() : QEvent(kType) {}
    ~MpvIdleEvent() override = default;
};

MpvBackend::MpvBackend(QObject* parent) : QObject(parent) {}

MpvBackend::~MpvBackend()
{
    shutdown();
}

static void* qgMpvGlGetProcAddress(void*, const char* name)
{
    QOpenGLContext* ctx = QOpenGLContext::currentContext();
    if (!ctx)
        return nullptr;
    return reinterpret_cast<void*>(ctx->getProcAddress(QByteArray(name)));
}

void MpvBackend::createHandle()
{
    if (handle_ || shuttingDown_)
        return;

    std::setlocale(LC_NUMERIC, "C");
    handle_ = mpv_create();
    if (!handle_) {
        qWarning() << "MpvBackend: mpv_create() returned null";
        return;
    }

    mpv_set_option_string(handle_, "vo", "libmpv");
    mpv_set_option_string(handle_, "hwdec", "auto-safe");
    mpv_set_option_string(handle_, "keep-open", "yes");
    mpv_set_option_string(handle_, "ytdl", "no");
    mpv_set_wakeup_callback(handle_, wakeupCallback, this);

    if (mpv_initialize(handle_) < 0) {
        qWarning() << "MpvBackend: mpv_initialize() failed";
        mpv_terminate_destroy(handle_);
        handle_ = nullptr;
        return;
    }
    handleInitialized_ = true;

    struct ObserveSpec { const char* name; mpv_format format; };
    static const ObserveSpec kObserved[] = {
        {"pause", MPV_FORMAT_FLAG},
        {"duration", MPV_FORMAT_DOUBLE},
        {"time-pos", MPV_FORMAT_DOUBLE},
        {"seekable", MPV_FORMAT_FLAG},
        {"eof-reached", MPV_FORMAT_FLAG},
        {"idle-active", MPV_FORMAT_FLAG},
        {"volume", MPV_FORMAT_DOUBLE},
        {"mute", MPV_FORMAT_FLAG},
        // Buffered end for the timeline's "buffered range, when supported"
        // (§3.1). mpv returns -1 when the demuxer has no cache.
        {"demuxer-cache-time", MPV_FORMAT_DOUBLE},
    };
    for (const ObserveSpec& spec : kObserved)
        mpv_observe_property(handle_, 0, spec.name, spec.format);
}

void MpvBackend::wakeupCallback(void* userData)
{
    auto* self = static_cast<MpvBackend*>(userData);
    QCoreApplication::postEvent(self, new MpvIdleEvent());
}

bool MpvBackend::event(QEvent* ev)
{
    if (ev->type() != MpvIdleEvent::kType)
        return QObject::event(ev);

    while (handle_) {
        mpv_event* e = mpv_wait_event(handle_, 0);
        if (e->event_id == MPV_EVENT_NONE)
            break;

        switch (e->event_id) {
        case MPV_EVENT_PROPERTY_CHANGE: {
            auto* prop = static_cast<mpv_event_property*>(e->data);
            if (prop == nullptr || prop->name == nullptr)
                break;
            QVariant value;
            if (prop->data != nullptr) {
                switch (prop->format) {
                case MPV_FORMAT_FLAG:
                    value = (*static_cast<int*>(prop->data) != 0);
                    break;
                case MPV_FORMAT_DOUBLE:
                    value = *static_cast<double*>(prop->data);
                    break;
                case MPV_FORMAT_INT64:
                    value = static_cast<qlonglong>(*static_cast<int64_t*>(prop->data));
                    break;
                case MPV_FORMAT_STRING: {
                    const char* raw = *static_cast<char**>(prop->data);
                    if (raw != nullptr)
                        value = QString::fromUtf8(raw);
                    break;
                }
                default:
                    break;
                }
            }
            emit propertyChanged(QString::fromUtf8(prop->name), value);
            break;
        }
        case MPV_EVENT_FILE_LOADED:
            emit fileLoaded();
            break;
        case MPV_EVENT_END_FILE: {
            auto* end = static_cast<mpv_event_end_file*>(e->data);
            emit endFile(end != nullptr ? end->reason : 0,
                         end != nullptr ? end->error : 0);
            break;
        }
        default:
            break;
        }
    }
    return true;
}

mpv_handle* MpvBackend::handle()
{
    if (!handle_)
        createHandle();
    return handle_;
}

mpv_render_context* MpvBackend::renderContext()
{
    if (shuttingDown_ || !handle_)
        return nullptr;
    if (renderCtxCreated_)
        return renderCtx_;

    mpv_opengl_init_params glParams = {
        .get_proc_address = qgMpvGlGetProcAddress,
    };
    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_API_TYPE, const_cast<char*>(MPV_RENDER_API_TYPE_OPENGL)},
        {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &glParams},
        {MPV_RENDER_PARAM_INVALID, nullptr},
    };

    if (mpv_render_context_create(&renderCtx_, handle_, params) < 0) {
        qWarning() << "MpvBackend: mpv_render_context_create() failed";
        renderCtx_ = nullptr;
        return nullptr;
    }
    renderCtxCreated_ = true;
    return renderCtx_;
}

bool MpvBackend::initialized() const
{
    return handle_ != nullptr && handleInitialized_;
}

void MpvBackend::setSurface(VideoSurfaceItem* surface)
{
    surface_ = surface;
}

VideoSurfaceItem* MpvBackend::surface() const
{
    return surface_.data();
}

void MpvBackend::shutdown()
{
    if (shutdownPending_ || shuttingDown_)
        return;
    shuttingDown_ = true;

    if (renderCtx_ == nullptr) {
        // No render context depends on the handle, so immediate teardown is safe.
        finishShutdown();
        return;
    }

    mpv_render_context_set_update_callback(renderCtx_, nullptr, nullptr);

    VideoSurfaceItem* surface = surface_.data();
    if (surface != nullptr && surface->window() != nullptr) {
        mpv_render_context* ctx = renderCtx_;
        transferredRenderCtx_.store(ctx);
        renderCtx_ = nullptr;
        shutdownPending_ = true;
        QObject::connect(surface, &VideoSurfaceItem::renderContextFreed,
                         this, &MpvBackend::finishShutdown,
                         Qt::UniqueConnection);
        surface->scheduleRenderContextFree(ctx);
        return;
    }

    qWarning("MpvBackend: no render thread available; deferring mpv teardown to exit");
    // Preserve the handle/context. Freeing without the creating GL context or
    // terminating the handle first violates libmpv's lifecycle contract.
}

void MpvBackend::finishShutdown()
{
    if (renderCtx_ != nullptr || transferredRenderCtx_.load() != nullptr) {
        qWarning("MpvBackend: refusing to terminate mpv before render-context cleanup completes");
        return;
    }

    if (handle_ != nullptr) {
        if (handleInitialized_)
            mpv_set_wakeup_callback(handle_, nullptr, nullptr);
        mpv_terminate_destroy(handle_);
        handle_ = nullptr;
    }
    handleInitialized_ = false;
    renderCtxCreated_ = false;
    renderGlContext_.store(nullptr);
    shutdownPending_ = false;
    shuttingDown_ = false;
}
