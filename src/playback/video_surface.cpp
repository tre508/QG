#include "video_surface.hpp"
#include "mpv_backend.hpp"

#include <mpv/client.h>
#include <mpv/render_gl.h>

#include <QDebug>
#include <QMetaObject>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QQuickWindow>
#include <QRunnable>

#include <mutex>
#include <utility>

namespace {

constexpr int kFlipY = 1;

class VideoRenderer : public QQuickFramebufferObject::Renderer {
public:
    void synchronize(QQuickFramebufferObject* item) override
    {
        auto* surface = static_cast<VideoSurfaceItem*>(item);
        surface_ = surface;
        backend_ = surface->backend();
        updateTarget_ = surface->updateTarget();
        freePending_ = surface->renderContextFreePending();
    }

    void render() override
    {
        if (freePending_ && freePending_->load()) {
            ctx_ = nullptr;
            return;
        }
        if (surface_.isNull() || backend_.isNull())
            return;

        QOpenGLContext* current = QOpenGLContext::currentContext();
        if (current == nullptr)
            return;

        if (ctx_ == nullptr) {
            ctx_ = backend_->renderContext();
            if (ctx_ != nullptr) {
                backend_->setRenderGlContext(current);
                mpv_render_context_set_update_callback(ctx_, onMpvUpdate,
                                                       updateTarget_.get());
                QMetaObject::invokeMethod(surface_.data(), "onRenderContextReady",
                                          Qt::QueuedConnection);
            }
        }
        if (ctx_ == nullptr || (freePending_ && freePending_->load()))
            return;

        QOpenGLFramebufferObject* fbo = framebufferObject();
        if (fbo == nullptr || fbo->width() <= 0 || fbo->height() <= 0)
            return;

        GLint fboId = 0;
        current->functions()->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fboId);
        mpv_opengl_fbo mpvFbo{};
        mpvFbo.fbo = static_cast<int>(fboId);
        mpvFbo.w = fbo->width();
        mpvFbo.h = fbo->height();
        int flipY = kFlipY;
        mpv_render_param params[] = {
            {MPV_RENDER_PARAM_OPENGL_FBO, &mpvFbo},
            {MPV_RENDER_PARAM_FLIP_Y, &flipY},
            {MPV_RENDER_PARAM_INVALID, nullptr},
        };
        mpv_render_context_render(ctx_, params);
        mpv_render_context_report_swap(ctx_);
        surface_->noteFrameRendered();
    }

private:
    static void onMpvUpdate(void* userData)
    {
        auto* target = static_cast<MpvUpdateTarget*>(userData);
        if (target == nullptr)
            return;
        std::lock_guard<std::mutex> lock(target->mutex);
        if (target->surface)
            QMetaObject::invokeMethod(target->surface.data(), "update",
                                      Qt::QueuedConnection);
    }

    QPointer<VideoSurfaceItem> surface_;
    QPointer<MpvBackend> backend_;
    std::shared_ptr<MpvUpdateTarget> updateTarget_;
    std::shared_ptr<std::atomic<bool>> freePending_;
    mpv_render_context* ctx_ = nullptr;
};

class ContextFreeJob : public QRunnable {
public:
    ContextFreeJob(mpv_render_context* ctx, QPointer<VideoSurfaceItem> item,
                   QPointer<MpvBackend> backend,
                   std::shared_ptr<MpvUpdateTarget> updateTarget,
                   std::shared_ptr<std::atomic<bool>> freePending,
                   QOpenGLContext* ownerContext)
        : ctx_(ctx), item_(item), backend_(backend),
          updateTarget_(std::move(updateTarget)), freePending_(std::move(freePending)),
          ownerContext_(ownerContext) {}

    void run() override
    {
        if (ownerContext_ == nullptr || QOpenGLContext::currentContext() != ownerContext_) {
            qWarning("mpv: creating GL context is not current; render context left to process exit");
            return;
        }

        freePending_->store(true);
        mpv_render_context_set_update_callback(ctx_, nullptr, nullptr);
        mpv_render_context_free(ctx_);
        if (!backend_.isNull())
            backend_->contextFreed(ctx_);

        if (!item_.isNull())
            QMetaObject::invokeMethod(item_.data(), "onRenderContextFreed",
                                      Qt::QueuedConnection);
        if (!backend_.isNull())
            QMetaObject::invokeMethod(backend_.data(), "finishShutdown",
                                      Qt::QueuedConnection);
    }

private:
    mpv_render_context* ctx_ = nullptr;
    QPointer<VideoSurfaceItem> item_;
    QPointer<MpvBackend> backend_;
    std::shared_ptr<MpvUpdateTarget> updateTarget_;
    std::shared_ptr<std::atomic<bool>> freePending_;
    QOpenGLContext* ownerContext_ = nullptr;
};

} // namespace

VideoSurfaceItem::VideoSurfaceItem(QQuickItem* parent)
    : QQuickFramebufferObject(parent),
      updateTarget_(std::make_shared<MpvUpdateTarget>()),
      renderContextFreePending_(std::make_shared<std::atomic<bool>>(false))
{
    setTextureFollowsItemSize(true);
}

VideoSurfaceItem::~VideoSurfaceItem()
{
    if (updateTarget_) {
        std::lock_guard<std::mutex> lock(updateTarget_->mutex);
        updateTarget_->surface.clear();
    }
    if (backend_ && backend_->surface() == this)
        backend_->setSurface(nullptr);
}

void VideoSurfaceItem::setBackend(MpvBackend* backend)
{
    if (backend_ == backend)
        return;

    if (updateTarget_) {
        std::lock_guard<std::mutex> lock(updateTarget_->mutex);
        if (updateTarget_->surface == this)
            updateTarget_->surface.clear();
    }

    if (backend_ && backend_->surface() == this)
        backend_->setSurface(nullptr);

    backend_ = backend;
    if (backend_) {
        updateTarget_ = backend_->updateTarget();
        {
            std::lock_guard<std::mutex> lock(updateTarget_->mutex);
            updateTarget_->surface = this;
        }
        backend_->setSurface(this);
    } else {
        updateTarget_ = std::make_shared<MpvUpdateTarget>();
    }

    emit backendChanged();
    update();
}

void VideoSurfaceItem::scheduleRenderContextFree(mpv_render_context* ctx)
{
    if (ctx == nullptr)
        return;

    renderContextFreePending_->store(true);
    mpv_render_context_set_update_callback(ctx, nullptr, nullptr);

    QQuickWindow* window = this->window();
    if (window == nullptr) {
        qWarning("VideoSurfaceItem: no window; cannot free the mpv render context");
        return;
    }

    QPointer<MpvBackend> backend = backend_;
    QOpenGLContext* ownerContext = backend ? backend->renderGlContext() : nullptr;
    window->scheduleRenderJob(
        new ContextFreeJob(ctx, QPointer<VideoSurfaceItem>(this), backend,
                           updateTarget_, renderContextFreePending_, ownerContext),
        QQuickWindow::BeforeSynchronizingStage);
}

void VideoSurfaceItem::onRenderContextReady()
{
    if (!videoActive_) {
        videoActive_ = true;
        emit videoActiveChanged();
    }
    emit renderContextReady();
}

void VideoSurfaceItem::onRenderContextFreed()
{
    videoActive_ = false;
    emit videoActiveChanged();
    emit renderContextFreed();
}

QQuickFramebufferObject::Renderer* VideoSurfaceItem::createRenderer() const
{
    return new VideoRenderer();
}
