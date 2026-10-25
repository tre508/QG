#pragma once
#include <QQuickFramebufferObject>

#include <atomic>

struct mpv_render_context;

// VideoSurfaceItem: presents mpv frames inside the Qt Quick scene graph
// (DESIGN.md §19.3). It is a QQuickFramebufferObject so Qt allocates an FBO
// sized to THIS item (in device pixels) and composites it at the item's own
// rect. That is what keeps mpv frames inside the Stage instead of at the
// window origin: mpv always renders with viewport (0,0,w,h) of the FBO it is
// given, so it must be given an item-sized FBO, never the window FBO.
//
// This is the only place mpv frames enter the Qt Quick scene graph.

class MpvBackend;

class VideoSurfaceItem : public QQuickFramebufferObject {
    Q_OBJECT
    Q_PROPERTY(MpvBackend* backend READ backend WRITE setBackend NOTIFY backendChanged)
    Q_PROPERTY(bool videoActive READ videoActive NOTIFY videoActiveChanged)
    Q_PROPERTY(int framesRendered READ framesRendered)

public:
    explicit VideoSurfaceItem(QQuickItem* parent = nullptr);
    ~VideoSurfaceItem() override;

    MpvBackend* backend() const { return backend_; }
    void setBackend(MpvBackend* b);

    bool videoActive() const { return videoActive_; }

    // Diagnostic: number of mpv frames presented to the scene graph.
    int framesRendered() const { return framesRendered_.load(); }
    // Called from the render thread.
    void noteFrameRendered() { framesRendered_.fetch_add(1); }

signals:
    void backendChanged();
    void videoActiveChanged();
    // Emitted on the GUI thread once mpv's render context exists; mpv must not
    // be asked to play before this (vo=libmpv ends the file otherwise).
    void renderContextReady();

protected:
    Renderer* createRenderer() const override;

private slots:
    // Marshaled from the render thread when the render context first exists.
    void onRenderContextReady();

private:
    friend class VideoRenderer;

    MpvBackend* backend_ = nullptr;
    bool videoActive_ = false;
    std::atomic<int> framesRendered_{0};
};
