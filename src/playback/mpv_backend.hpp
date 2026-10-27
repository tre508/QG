#pragma once
#include <QObject>
#include <QOpenGLContext>
#include <QPointer>
#include <QString>
#include <QVariant>

#include <atomic>
#include <memory>
#include <mutex>

struct mpv_handle;
struct mpv_render_context;
class VideoSurfaceItem;

struct MpvUpdateTarget {
    std::mutex mutex;
    QPointer<VideoSurfaceItem> surface;
};

// MpvBackend owns the single libmpv handle and render context.
class MpvBackend : public QObject {
    Q_OBJECT
public:
    explicit MpvBackend(QObject* parent = nullptr);
    ~MpvBackend() override;

    mpv_handle* handle();
    mpv_render_context* renderContext();
    bool hasRenderContext() const { return renderCtx_ != nullptr; }
    bool initialized() const;
    bool isShuttingDown() const { return shuttingDown_; }

    void shutdown();
    void setSurface(VideoSurfaceItem* surface);
    VideoSurfaceItem* surface() const;
    void setRenderGlContext(QOpenGLContext* context) { renderGlContext_.store(context); }
    QOpenGLContext* renderGlContext() const { return renderGlContext_.load(); }
    std::shared_ptr<MpvUpdateTarget> updateTarget() const { return updateTarget_; }
    void contextFreed(mpv_render_context* ctx) {
        if (transferredRenderCtx_.load() == ctx)
            transferredRenderCtx_.store(nullptr);
    }

private slots:
    void finishShutdown();

signals:
    void fileLoaded();
    void endFile(int reason, int error);
    void propertyChanged(const QString& name, const QVariant& value);

protected:
    bool event(QEvent* ev) override;

private:
    void createHandle();
    static void wakeupCallback(void* userData);

    mpv_handle* handle_ = nullptr;
    mpv_render_context* renderCtx_ = nullptr;
    std::atomic<mpv_render_context*> transferredRenderCtx_{nullptr};
    bool renderCtxCreated_ = false;
    bool handleInitialized_ = false;
    bool shuttingDown_ = false;
    bool shutdownPending_ = false;
    QPointer<VideoSurfaceItem> surface_;
    std::atomic<QOpenGLContext*> renderGlContext_{nullptr};
    std::shared_ptr<MpvUpdateTarget> updateTarget_ = std::make_shared<MpvUpdateTarget>();
};
