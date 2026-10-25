#pragma once
#include <QObject>
#include <QOpenGLContext>
#include <QString>
#include <QVariant>
#include <memory>

struct mpv_handle;
struct mpv_render_context;

// MpvBackend: the sole owner of the libmpv handle and mpv_render_context.
// DESIGN.md §19.1, §19.3 — one handle, one render context, no --wid embedding.
// Only this TU may create/destroy handles or call libmpv directly.

class MpvBackend : public QObject {
    Q_OBJECT
public:
    explicit MpvBackend(QObject* parent = nullptr);
    ~MpvBackend() override;

    // Returns the single handle (lazily created on first call).
    mpv_handle* handle();

    // Returns the mpv_render_context. Created once with the proc-address
    // callback; subsequent calls return the same instance.
    mpv_render_context* renderContext();
    // Non-creating query: true only once the surface has created the context.
    bool hasRenderContext() const { return renderCtx_ != nullptr; }

    // True only after handle() has been called successfully.
    bool initialized() const;

    // Explicit teardown (also called from destructor).
    void shutdown();

signals:
    // Emitted when mpv reports a new file has loaded (FILE_LOADED).
    void fileLoaded();
    // Emitted when mpv ends a file. `reason` is an mpv_end_file_reason value
    // (0 EOF, 2 stop, 3 quit, 4 error); `error` is the mpv error code.
    void endFile(int reason, int error);
    // Emitted for each observed property change (name + typed value). The value
    // is invalid when mpv reports the property as unavailable.
    void propertyChanged(const QString& name, const QVariant& value);

protected:
    // Marshals mpv wakeup events into the Qt event loop.
    bool event(QEvent* ev) override;

private:
    void createHandle();
    static void wakeupCallback(void* userData);

    mpv_handle* handle_ = nullptr;
    mpv_render_context* renderCtx_ = nullptr;
    bool renderCtxCreated_ = false;
};