# Implementing an Integration Class in KWin

## What an Integration Class Is

An integration class sits between the protocol layer (`src/wayland/`) and the window layer (`src/`). It owns the state machine for a protocol: deciding when to create windows, when to send events back to the client, and how to respond to compositor events (output changes, render loop signals, etc.).

Not every protocol needs one. If all a protocol does is create a window per surface with no sequencing logic, the integration is trivial. If the protocol has multi-step handshakes, per-output coordination, or timing requirements (e.g. "only send X after a frame is presented"), the integration is where that logic lives.

## Inheritance

```cpp
class YourIntegration : public WaylandShellIntegration
{
    Q_OBJECT
public:
    explicit YourIntegration(QObject *parent = nullptr);
};
```

`WaylandShellIntegration` (`src/waylandshellintegration.h`) is a thin base providing one signal:

```cpp
Q_SIGNALS:
    void windowCreated(Window *window);
```

`Workspace` (via `WaylandServer::initWorkspace()`) connects to this signal to register new windows. Emit it whenever a new window should enter the scene.

## Files to Create

- `src/yourintegration.h`
- `src/yourintegration.cpp`

Add both to `target_sources(kwin PRIVATE ...)` in `src/CMakeLists.txt`.

## Minimum Pattern

The integration typically creates the protocol manager object itself (like `LayerShellV1Integration` does), rather than receiving it externally:

```cpp
YourIntegration::YourIntegration(QObject *parent)
    : WaylandShellIntegration(parent)
{
    auto *manager = new YourManagerInterface(waylandServer()->display(), this);
    connect(manager, &YourManagerInterface::surfaceCreated,
            this, &YourIntegration::handleSurfaceCreated);
}

void YourIntegration::handleSurfaceCreated(YourSurfaceInterface *surface)
{
    LogicalOutput *output = resolveOutput(surface);
    auto *window = new YourWindow(surface, output);
    Q_EMIT windowCreated(window);
    window->moveResize(output->geometryF());  // send initial configure
}
```

**Always call `moveResize` after `windowCreated` for compositor-managed surfaces.** `windowCreated` registers the window with `WaylandServer` (adds it to `m_windows`, sets up ref-counting). Once that returns, calling `moveResize` is safe. This triggers `moveResizeInternal` → `sendConfigure(serial, width, height)` → the client knows what size to render at.

If you skip this, the client never receives a configure, submits a zero-size or garbage-size buffer, and KWin's GL renderer fails with `GL_OUT_OF_MEMORY` when trying to create a texture from it.

## Wiring in `wayland_server.cpp`

In `WaylandServer::initWorkspace()`, instantiate the integration and connect its `windowCreated` signal:

```cpp
auto *yourIntegration = new YourIntegration(this);
connect(yourIntegration, &YourIntegration::windowCreated,
        this, &WaylandServer::registerWindow);
```

Use `registerXdgGenericWindow` instead of `registerWindow` only for XDG shell windows (which get extra decoration/state machinery).

## Resolving Outputs

When a surface is associated with a specific `OutputInterface *`, resolve it to a `LogicalOutput *` for the window constructor:

```cpp
OutputInterface *outputIface = surface->output();
if (!outputIface || outputIface->isRemoved()) {
    return;  // or send a close/finished event
}
LogicalOutput *output = outputIface->handle();
```

If no preferred output is specified, fall back to `workspace()->activeOutput()`.

## State Machine Pattern

For protocols with exclusive state (e.g. only one lock can be held at a time), store the current active object and guard against duplicates:

```cpp
class YourIntegration : public WaylandShellIntegration
{
    ...
private:
    YourLockInterface *m_lock = nullptr;  // null = not locked
};

void YourIntegration::handleLockRequested(YourLockInterface *lock)
{
    if (m_lock) {
        lock->sendFinished();  // reject
        return;
    }
    m_lock = lock;
    connect(lock, &YourLockInterface::unlockRequested,
            this, &YourIntegration::handleUnlockRequested);
}

void YourIntegration::handleUnlockRequested()
{
    m_lock = nullptr;
}
```

## Frame Presentation Watcher

Some protocols require waiting for a frame to be presented on all outputs before sending an event (e.g. ext-session-lock must not send `locked` before the lock UI is on-screen).

Use a short-lived helper object that self-destructs after its job is done:

```cpp
class YourIntegration::PresentationWatcher : public QObject
{
public:
    explicit PresentationWatcher(YourLockInterface *lock, QObject *parent = nullptr)
        : QObject(parent)
        , m_lock(lock)
    {
        connect(waylandServer(), &WaylandServer::windowAdded,
                this, &PresentationWatcher::handleWindowAdded);

        // Fallback: fire anyway after 1 second in case an output never presents.
        QTimer::singleShot(1000, this, [this]() {
            sendAndDestroy();
        });
    }

private:
    void handleWindowAdded(Window *window)
    {
        if (!window->isLockScreen()) {
            return;
        }
        connect(window->output()->backendOutput()->renderLoop(), &RenderLoop::framePresented,
                this, [this, windowGuard = QPointer(window)]() {
                    if (!windowGuard) {
                        return;
                    }
                    m_signaledOutputs << windowGuard->output();
                    if (m_signaledOutputs.size() == workspace()->outputs().size()) {
                        sendAndDestroy();
                    }
                });
    }

    void sendAndDestroy()
    {
        if (m_lock) {
            m_lock->sendLocked();
        }
        delete this;
    }

    QPointer<YourLockInterface> m_lock;
    QSet<LogicalOutput *> m_signaledOutputs;
};
```

Key points:
- `WaylandServer::windowAdded` fires only after `readyForPainting()` (i.e. the window has a buffer). This is the right time to start watching frame presentation.
- `RenderLoop::framePresented(RenderLoop *loop, std::chrono::nanoseconds timestamp, PresentationMode mode)` fires per output when the GPU has actually shown a frame on screen.
- `window->output()->backendOutput()->renderLoop()` — chain to get the per-output render loop. Requires `#include "core/backendoutput.h"` and `#include "core/renderloop.h"`.
- Always use `QPointer<Window>` in the lambda capture — the window could be destroyed before a frame presents.
- Always add a timeout fallback — headless outputs in tests and certain hardware configs may never emit `framePresented`.

## Exposing Lock State to Input Filtering

If the integration controls a screen lock, `WaylandServer::isScreenLocked()` must return `true` while the lock is held. Expose an `isLocked()` method:

```cpp
bool YourIntegration::isLocked() const
{
    return m_lock != nullptr;
}
```

Then in `WaylandServer::isScreenLocked()` (in `src/wayland_server.cpp`), add:

```cpp
if (m_yourIntegration && m_yourIntegration->isLocked()) {
    return true;
}
```

This makes the `ScreenLockerFilter` in `src/input.cpp` block input to all non-lock windows automatically, because `isLockScreen()` on your window class returns `true`.

## Reference Implementations

| File | What it shows |
|---|---|
| `src/layershellv1integration.cpp/h` | Canonical simple integration — surface created → window created, per-output rearrange |
| `src/inputpanelv1integration.cpp/h` | Minimal integration — single window type, no state |
| `src/extsessionlockv1integration.cpp/h` | State machine (exclusive lock), presentation watcher, `isLocked()` |
| `src/wayland_server.cpp` | `initWorkspace()` — where all integrations are instantiated and connected; `LockScreenPresentationWatcher` — original presentation watcher for KScreenLocker |
