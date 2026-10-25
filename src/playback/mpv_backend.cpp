#include "mpv_backend.hpp"

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

MpvBackend::~MpvBackend() {
    shutdown();
}

// OpenGL proc-address callback required by mpv_render_context_create.
static void* qgMpvGlGetProcAddress(void*, const char* name) {
    QOpenGLContext* ctx = QOpenGLContext::currentContext();
    if (!ctx) return nullptr;
    return reinterpret_cast<void*>(ctx->getProcAddress(QByteArray(name)));
}

void MpvBackend::createHandle() {
    if (handle_) return;

    // libmpv requires the C numeric locale: mpv_create() returns NULL when
    // LC_NUMERIC is not "C" (e.g. en_US.UTF-8), and Qt sets the process locale
    // from the environment on startup. Force LC_NUMERIC=C before creating the
    // handle (DESIGN.md §19.1 one-handle policy; see DECISIONS.md 2026-10-09).
    std::setlocale(LC_NUMERIC, "C");

    handle_ = mpv_create();
    if (!handle_) {
        qWarning() << "MpvBackend: mpv_create() returned null";
        return;
    }

    // Essential options
    mpv_set_option_string(handle_, "vo", "libmpv");
    mpv_set_option_string(handle_, "hwdec", "auto-safe");
    mpv_set_option_string(handle_, "keep-open", "yes");
    mpv_set_option_string(handle_, "ytdl", "no");

    // Wakeup callback marshaled to Qt thread
    mpv_set_wakeup_callback(handle_, wakeupCallback, this);

    if (mpv_initialize(handle_) < 0) {
        qWarning() << "MpvBackend: mpv_initialize() failed";
        mpv_terminate_destroy(handle_);
        handle_ = nullptr;
        return;
    }

    // Observe the properties the TransportBar/session mirror (DESIGN.md §8.2,
    // §19.2). Property-change events are routed to Qt signals in event() below.
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
    };
    for (const ObserveSpec& spec : kObserved) {
        mpv_observe_property(handle_, 0, spec.name, spec.format);
    }
}

void MpvBackend::wakeupCallback(void* userData) {
    auto* self = static_cast<MpvBackend*>(userData);
    QCoreApplication::postEvent(self, new MpvIdleEvent());
}

bool MpvBackend::event(QEvent* ev) {
    if (ev->type() == MpvIdleEvent::kType) {
        // Drain mpv events on the Qt thread (rendering itself is scene-graph
        // driven; here we only publish state to the session — DESIGN.md §19.2).
        while (handle_) {
            mpv_event* e = mpv_wait_event(handle_, 0);
            if (e->event_id == MPV_EVENT_NONE) break;

            switch (e->event_id) {
            case MPV_EVENT_PROPERTY_CHANGE: {
                auto* prop = static_cast<mpv_event_property*>(e->data);
                if (prop == nullptr || prop->name == nullptr) break;
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
                        if (raw != nullptr) value = QString::fromUtf8(raw);
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
                const int reason = end != nullptr ? end->reason : 0;
                const int error = end != nullptr ? end->error : 0;
                emit endFile(reason, error);
                break;
            }
            default:
                break;
            }
        }
        return true;
    }
    return QObject::event(ev);
}

mpv_handle* MpvBackend::handle() {
    if (!handle_) {
        createHandle();
    }
    return handle_;
}

mpv_render_context* MpvBackend::renderContext() {
    if (!handle_) return nullptr;
    if (renderCtxCreated_) return renderCtx_;

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

bool MpvBackend::initialized() const {
    return handle_ != nullptr;
}

void MpvBackend::shutdown() {
    if (renderCtx_) {
        mpv_render_context_free(renderCtx_);
        renderCtx_ = nullptr;
        renderCtxCreated_ = false;
    }
    if (handle_) {
        mpv_terminate_destroy(handle_);
        handle_ = nullptr;
    }
}