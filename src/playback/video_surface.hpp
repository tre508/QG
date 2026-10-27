#pragma once
#include <QPointer>
#include <QQuickFramebufferObject>

#include <atomic>
#include <memory>

struct mpv_render_context;
struct MpvUpdateTarget;
class MpvBackend;

class VideoSurfaceItem : public QQuickFramebufferObject {
    Q_OBJECT
    Q_PROPERTY(MpvBackend* backend READ backend WRITE setBackend NOTIFY backendChanged)
    Q_PROPERTY(bool videoActive READ videoActive NOTIFY videoActiveChanged)
    Q_PROPERTY(int framesRendered READ framesRendered)

public:
    explicit VideoSurfaceItem(QQuickItem* parent = nullptr);
    ~VideoSurfaceItem() override;

    MpvBackend* backend() const { return backend_.data(); }
    void setBackend(MpvBackend* backend);
    bool videoActive() const { return videoActive_; }
    std::shared_ptr<MpvUpdateTarget> updateTarget() const { return updateTarget_; }
    std::shared_ptr<std::atomic<bool>> renderContextFreePending() const {
        return renderContextFreePending_;
    }

    // Hand the render context back to the render thread for freeing.
    // The creating OpenGL context must be current.
    void scheduleRenderContextFree(mpv_render_context* ctx);

    int framesRendered() const { return framesRendered_.load(); }
    void noteFrameRendered() { framesRendered_.fetch_add(1); }

signals:
    void backendChanged();
    void videoActiveChanged();
    void renderContextReady();
    // Emitted on the GUI thread after render-thread cleanup completes.
    void renderContextFreed();

protected:
    Renderer* createRenderer() const override;

private slots:
    void onRenderContextReady();
    void onRenderContextFreed();

private:
    friend class VideoRenderer;
    friend class ContextFreeJob;

    QPointer<MpvBackend> backend_;
    std::shared_ptr<MpvUpdateTarget> updateTarget_;
    std::shared_ptr<std::atomic<bool>> renderContextFreePending_;
    bool videoActive_ = false;
    std::atomic<int> framesRendered_{0};
};
