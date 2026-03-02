# Implementing a Window Class in KWin

## Inheritance Chain

All Wayland window types follow this hierarchy:

```
QObject → Window → WaylandWindow → YourWindow
```

- `Window` (`src/window.h`) — base class for all window types; defines the full virtual interface
- `WaylandWindow` (`src/waylandwindow.h`) — Wayland-specific base; handles `isLockScreen()`, pid, caption, surface assignment
- `YourWindow` — protocol-specific subclass

## Files to Create

- `src/yourprotocolwindow.h`
- `src/yourprotocolwindow.cpp`
- Optionally `src/yourprotocolintegration.h/.cpp` — if you need a state machine between the protocol and window layer

Add both `.cpp` files to `target_sources(kwin PRIVATE ...)` in `src/CMakeLists.txt`.

## Minimum Constructor Pattern

```cpp
YourWindow::YourWindow(YourSurfaceInterface *shellSurface, LogicalOutput *output)
    : WaylandWindow(shellSurface->surface())   // passes surface to WaylandWindow
    , m_shellSurface(shellSurface)
{
    setOutput(output);
    setMoveResizeOutput(output);
    setSkipSwitcher(true);
    setSkipPager(true);
    setSkipTaskbar(true);

    connect(shellSurface, &YourSurfaceInterface::aboutToBeDestroyed,
            this, &YourWindow::destroyWindow);
    connect(shellSurface->surface(), &SurfaceInterface::aboutToBeDestroyed,
            this, &YourWindow::destroyWindow);
    connect(shellSurface->surface(), &SurfaceInterface::committed,
            this, &YourWindow::handleCommitted);
    connect(shellSurface->surface(), &SurfaceInterface::sizeChanged,
            this, &YourWindow::handleSizeChanged);
    connect(shellSurface, &YourSurfaceInterface::configureAcknowledged,
            this, &YourWindow::handleConfigureAcknowledged);
}
```

## Virtual Methods to Override

These are the most commonly overridden methods. Check `src/window.h` for the full list.

### Identity / Classification

| Method | Notes |
|---|---|
| `windowType()` | Returns `WindowType::Normal` for most protocol surfaces |
| `isLockScreen()` | Return `true` for lock surfaces — gates all input in `ScreenLockerFilter` |
| `isPlaceable()` | `false` if the compositor controls position, not the user |
| `isCloseable()` | Whether the window can be closed by the user |
| `isMovable()` / `isMovableAcrossScreens()` | `false` for compositor-managed surfaces |
| `isResizable()` | `false` for compositor-managed surfaces |

### Input / Focus

| Method | Notes |
|---|---|
| `wantsInput()` | Typically `acceptsFocus() && readyForPainting()` |
| `acceptsFocus()` | `!isDeleted() && <protocol says yes>` |

### Stacking

| Method | Notes |
|---|---|
| `belongsToLayer()` | Controls Z-order. Available layers (from `src/effect/globals.h`): |
| | `DesktopLayer`, `BelowLayer`, `NormalLayer`, `AboveLayer`, `ActiveLayer`, `OverlayLayer` |
| | Lock screens should use `OverlayLayer` or `ActiveLayer` |

### Geometry

| Method | Notes |
|---|---|
| `moveResizeInternal(rect, mode)` | Called by the compositor to resize the window. Send configure to client if size changed, then call `updateGeometry()` |

### Lifecycle

| Method | Notes |
|---|---|
| `destroyWindow()` | Disconnect signals, call `markAsDeleted()`, emit `closed()`, call `waylandServer()->removeWindow(this)`, then `unref()` |
| `closeWindow()` | Send a close/finished event to the client (does not destroy immediately) |

### Output hints (optional but recommended)

| Method | Notes |
|---|---|
| `doSetNextTargetScale()` | Forward to `surface()->setPreferredBufferScale(nextTargetScale())` |
| `doSetPreferredBufferTransform()` | Forward to `surface()->setPreferredBufferTransform(...)` |
| `doSetPreferredColorDescription()` | Forward to `surface()->setPreferredColorDescription(...)` |

## Standard destroyWindow() Pattern

Every window class uses this same teardown sequence:

```cpp
void YourWindow::destroyWindow()
{
    m_shellSurface->disconnect(this);
    m_shellSurface->surface()->disconnect(this);

    markAsDeleted();
    Q_EMIT closed();

    cleanTabBox();
    StackingUpdatesBlocker blocker(workspace());
    cleanGrouping();
    waylandServer()->removeWindow(this);
    unref();
}
```

## Configure / Ack Pattern

For protocols with a configure/ack_configure handshake (like layer shell, xdg shell, ext-session-lock surfaces):

```cpp
// Pending configures tracked as a queue
struct YourConfigureEvent {
    quint32 serial;
    QSize size;
};
QList<YourConfigureEvent> m_configureEvents;

// In moveResizeInternal — send configure, enqueue serial
void YourWindow::moveResizeInternal(const RectF &rect, MoveResizeMode mode)
{
    const QSize requestedSize = nextFrameSizeToClientSize(rect.size()).toSize();
    if (requestedSize != clientSize()) {
        const quint32 serial = m_shellSurface->sendConfigure(serial, requestedSize.width(), requestedSize.height());
        m_configureEvents.append({serial, requestedSize});
    } else {
        updateGeometry(rect);
    }
}

// In handleConfigureAcknowledged — drain queue up to the acked serial
void YourWindow::handleConfigureAcknowledged(quint32 serial)
{
    while (!m_configureEvents.isEmpty()) {
        const YourConfigureEvent head = m_configureEvents.takeFirst();
        if (head.serial == serial) {
            break;
        }
    }
}
```

## First Buffer / markAsMapped

A window isn't visible until it has a buffer. Call `markAsMapped()` when the first buffer is committed:

```cpp
void YourWindow::handleCommitted()
{
    if (surface()->buffer()) {
        markAsMapped();
    }
}
```

## Geometry After Buffer Arrival

`moveResizeInternal` updates position immediately but **not** size — it sends a configure with the requested dimensions and then only updates the frame geometry's top-left corner. The full geometry (with the correct size) is only known once the client commits a buffer of that size.

Connect `SurfaceInterface::sizeChanged` to update geometry when the buffer arrives:

```cpp
void YourWindow::handleSizeChanged()
{
    updateGeometry(RectF(pos(), clientSizeToFrameSize(surface()->size())));
}
```

Without this, the window has correct position but zero/stale size. The scene graph tracks damage against the wrong rect, causing holes in the rendered output and no automatic repaint when the first buffer lands. This is the same pattern used by `LayerShellV1Window::handleSizeChanged`.

## isLockScreen() Note

`WaylandWindow::isLockScreen()` returns `true` if the surface's client matches `screenLockerClientConnection()` — that's the old KScreenLocker path. If your window is always a lock screen (e.g. ext-session-lock surfaces), override it directly:

```cpp
bool YourWindow::isLockScreen() const
{
    return true;
}
```

This makes the `ScreenLockerFilter` in `src/input.cpp` automatically block input to all other windows.

## Integration Class

When protocol surfaces need a state machine (e.g. tracking lock state, managing per-output windows), create a separate integration class:

```cpp
class YourIntegration : public WaylandShellIntegration
{
    Q_OBJECT
public:
    explicit YourIntegration(QObject *parent = nullptr);
};
```

`WaylandShellIntegration` provides the `windowCreated(Window*)` signal. Emit it when you construct a new window — `Workspace` connects to this to add the window to its list:

```cpp
Q_EMIT windowCreated(new YourWindow(shellSurface, output));
```

The integration is typically instantiated from `wayland_server.cpp` or wired in via a shell integration loader.

## CMakeLists.txt

Add your `.cpp` files to `src/CMakeLists.txt`:

```cmake
target_sources(kwin PRIVATE
    ...
    yourprotocolwindow.cpp
    yourprotocolintegration.cpp
    ...
)
```

## Reference Implementations

| File | What it shows |
|---|---|
| `src/layershellv1window.cpp/h` | Per-output window, configure/ack, `belongsToLayer()`, geometry management |
| `src/layershellv1integration.cpp/h` | Integration pattern, `windowCreated`, per-output rearrange |
| `src/waylandwindow.cpp/h` | Base class — `isLockScreen()`, `markAsMapped()`, `destroyWindow()` helpers |
| `src/window.h` | Full virtual interface — read this to know what you can override |
