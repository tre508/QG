#include "video_surface.hpp"
#include "mpv_backend.hpp"

#include <mpv/client.h>
#include <mpv/render_gl.h>

#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QPointer>
#include <QMetaObject>
#include <QDebug>

namespace {

constexpr int kFlipY = 1;

// Scene-graph renderer that presents mpv frames (DESIGN.md §19.3).
//
// QQuickFramebufferObject allocates an FBO sized to this item (device pixels),
// binds it, and invokes render() on the render thread inside the Qt Quick
// render pass. Because mpv renders with viewport (0,0,w,h) of the FBO it is
// given, the FBO MUST be the item-sized one — never the window framebuffer.
// Passing the window FBO with the item's dimensions (the previous defect) made
// mpv draw an item-sized rectangle at the WINDOW's bottom-left (the PLAY-VIS-001
// purple region). Qt now composites this FBO at the item's rect, so frames stay
// inside the Stage, and Qt handles scissor/clip/DPR/resize for us.
class VideoRenderer : public QQuickFramebufferObject::Renderer {
public:
    ~VideoRenderer() override = default;

    void synchronize(QQuickFramebufferObject* item) override
    {
        auto* surface = static_cast<VideoSurfaceItem*>(item);
        item_ = surface;
        backend_ = surface->backend();
    }

    void render() override
    {
        if (item_ == nullptr || backend_ == nullptr)
            return;

        QOpenGLContext* current = QOpenGLContext::currentContext();
        if (current == nullptr)
            return; // not an OpenGL render pass

        if (ctx_ == nullptr) {
            // Create mpv's render context on the render thread with the GL
            // context current (the only valid place for it).
            ctx_ = backend_->renderContext();
            if (ctx_ != nullptr) {
                mpv_render_context_set_update_callback(ctx_, onMpvUpdate, item_);
                // Marshaled to the GUI thread: PlaybackManager releases a
                // pending load on this signal (vo=libmpv aborts early loads).
                QMetaObject::invokeMethod(item_, "onRenderContextReady",
                                          Qt::QueuedConnection);
            }
        }
        if (ctx_ == nullptr)
            return;

        QOpenGLFramebufferObject* fbo = framebufferObject();
        if (fbo == nullptr)
            return;
        const int w = fbo->width();
        const int h = fbo->height();
        if (w <= 0 || h <= 0)
            return;

        QOpenGLFunctions* gl = current->functions();

        GLint fboId = 0;
        gl->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fboId);

        mpv_opengl_fbo mpvFbo{};
        mpvFbo.fbo = static_cast<int>(fboId);
        mpvFbo.w = w;   // the FBO's real size (not the window's)
        mpvFbo.h = h;

        int flipY = kFlipY;
        mpv_render_param params[] = {
            {MPV_RENDER_PARAM_OPENGL_FBO, &mpvFbo},
            {MPV_RENDER_PARAM_FLIP_Y, &flipY},
            {MPV_RENDER_PARAM_INVALID, nullptr},
        };
        mpv_render_context_render(ctx_, params);
        // Tell mpv the frame was presented so it can pace/release it.
        mpv_render_context_report_swap(ctx_);
        item_->noteFrameRendered();
    }

private:
    // mpv calls this from its own thread when a new frame is ready. Qt GUI
    // objects must not be touched off the GUI thread, so the repaint is
    // marshaled across (a destroyed item becomes a no-op via QPointer).
    static void onMpvUpdate(void* userData)
    {
        QPointer<VideoSurfaceItem> item(static_cast<VideoSurfaceItem*>(userData));
        if (item)
            QMetaObject::invokeMethod(item.data(), "update", Qt::QueuedConnection);
    }

    VideoSurfaceItem* item_ = nullptr;
    MpvBackend* backend_ = nullptr;
    mpv_render_context* ctx_ = nullptr;
};

} // namespace

VideoSurfaceItem::VideoSurfaceItem(QQuickItem* parent) : QQuickFramebufferObject(parent)
{
    // The FBO must track this item's size so mpv always renders into a
    // correctly sized target (including after resize / DPR changes).
    setTextureFollowsItemSize(true);
}

VideoSurfaceItem::~VideoSurfaceItem() = default;

void VideoSurfaceItem::setBackend(MpvBackend* b)
{
    if (backend_ != b) {
        backend_ = b;
        emit backendChanged();
        update();
    }
}

void VideoSurfaceItem::onRenderContextReady()
{
    if (!videoActive_) {
        videoActive_ = true;
        emit videoActiveChanged();
    }
    emit renderContextReady();
}

QQuickFramebufferObject::Renderer* VideoSurfaceItem::createRenderer() const
{
    return new VideoRenderer();
}
