# Implementing Wayland Protocols in KWin

## Overview

A protocol implementation in KWin has up to four parts:

1. **Protocol class** (`src/wayland/`) — thin wire layer, translates Wayland requests into Qt signals
2. **Window class** (`src/`) — if the protocol creates surfaces, a `WaylandWindow` subclass to represent them
3. **Integration class** (`src/`) — state machine sitting between the protocol layer and the window layer
4. **Wiring** — instantiate in `wayland_server.cpp`, connect signals, expose via getter

Not all protocols need all four. Simple protocols (e.g. `pointerwarp_v1`) are just the protocol class + wiring.

---

## Protocol Class Conventions (`src/wayland/`)

### File naming
- `src/wayland/yourprotocol_v1.h` and `.cpp`
- Include name maps to xml: `ext-session-lock-v1.xml` → `qwayland-server-ext-session-lock-v1.h`

### Class structure
```cpp
// Manager/global: inherit QObject + generated base, use private inheritance
class ExtSessionLockManagerV1Interface : public QObject, private QtWaylandServer::ext_session_lock_manager_v1

// Child resource (created per-client request): no QObject needed unless signals required
class ExtSessionLockSurfaceV1Interface : private QtWaylandServer::ext_session_lock_surface_v1
```

Use **`private`** inheritance from the generated base — Wayland internals are not part of the public API.

### Constructor — registering a global
```cpp
ExtSessionLockManagerV1Interface::ExtSessionLockManagerV1Interface(Display *display, QObject *parent)
    : QObject(parent)
    , QtWaylandServer::ext_session_lock_manager_v1(*display, s_version)  // registers the Wayland global
```

### `s_version` belongs in the `.cpp` only

### Destructor requests
XML requests tagged `type="destructor"` must manually call:
```cpp
wl_resource_destroy(resource->handle);
```
The generated code does not do this automatically.

### Child object lifetime
Child objects (e.g. per-surface resources) are heap-allocated with `new` and self-delete:
```cpp
void YourSurfaceV1::your_surface_v1_destroy_resource(Resource *resource)
{
    delete this;
}
```

### Posting protocol errors
```cpp
wl_resource_post_error(resource->handle, error_some_enum_value, "descriptive message");
```
Error enum values come from the generated header.

### Sending events to clients
```cpp
send_locked(resource()->handle);         // to single resource
send_configure(resource()->handle, serial, width, height);
```

### Forward declarations in headers
Only `#include` what you need the full definition of. Use forward declarations for pointer-only dependencies:
```cpp
// .h — forward declare
class Display;
class SurfaceInterface;

// .cpp — include
#include "display.h"
#include "surface.h"
```

**Do not include window class headers from protocol headers.** Protocol classes in `src/wayland/` have no business knowing about `WaylandWindow` or any `src/` window type — that's a layering violation and can introduce subtle circular dependency issues. If you accidentally add `#include "waylandwindow.h"` to a protocol header, remove it.

### Good reference implementations
- Simple (no child objects): `src/wayland/pointerwarp_v1.cpp`
- Manager + child objects: `src/wayland/alphamodifier_v1.cpp`
- Complex (configure/ack, multi-interface): `src/wayland/layershell_v1.cpp`

---

## CMakeLists.txt

In `src/wayland/CMakeLists.txt`:

1. Add XML to `ecm_add_qtwayland_server_protocol_kde(WaylandProtocols_xml FILES ...)`:
   - Standard upstream: `${WaylandProtocols_DATADIR}/staging/your-protocol/your-protocol-v1.xml`
   - Plasma-specific: `${PLASMA_WAYLAND_PROTOCOLS_DIR}/your-protocol.xml`
   - Not yet released: place XML in `src/wayland/protocols/` and use `${PROJECT_SOURCE_DIR}/src/wayland/protocols/your-protocol-v1.xml`

2. Add `.cpp` to `target_sources(kwin PRIVATE ...)` in the same file.

3. Re-run cmake after editing, then build `WaylandProtocols_xml` target to generate the header before writing implementation code.

**Watch out for duplicate XML entries** — adding the same XML twice causes a cmake "already has a custom rule" error even with a fresh build directory.

---

## Window Class (`src/`)

Needed when the protocol creates surfaces that KWin renders and routes input to.

### Inheritance chain
```
Window → WaylandWindow → YourProtocolWindow
```

### Key methods to override

| Method | Notes |
|---|---|
| `isLockScreen()` | Return `true` for lock surfaces — gates all input filtering in `input.cpp` |
| `windowType()` | Usually `WindowType::Normal` unless a dedicated type applies |
| `isPlaceable()` | `false` if compositor controls position |
| `isCloseable()`, `isMovable()`, `isResizable()` | `false` for compositor-managed surfaces |
| `wantsInput()` | `true` for lock screens |
| `belongsToLayer()` | Controls stacking; use the highest layer for lock screens |
| `acceptsFocus()` | `true` for lock screens |
| `destroyWindow()` | Clean up and remove from workspace |
| `moveResizeInternal()` | Call `updateGeometry()` |

### `isLockScreen()` in `WaylandWindow`
`WaylandWindow::isLockScreen()` returns true if the surface's client matches `screenLockerClientConnection()` — that's the old KScreenLocker path. For ext-session-lock, override it in your subclass to unconditionally return `true`.

### Constructor pattern
```cpp
YourWindow::YourWindow(YourSurfaceInterface *shellSurface, ...)
    : WaylandWindow(shellSurface->surface())
{
    setOutput(output);
    connect(shellSurface->surface(), &SurfaceInterface::committed, this, &YourWindow::handleCommitted);
    connect(shellSurface->surface(), &SurfaceInterface::aboutToBeDestroyed, this, &YourWindow::destroyWindow);
    connect(shellSurface, &YourSurfaceInterface::configureAcknowledged, this, &YourWindow::handleConfigureAcknowledged);
}
```

### Reference: `src/layershellv1window.cpp/h`
Closest analog — per-output surface, configure/ack semantics, compositor-controlled geometry.

---

## Integration Class (`src/`)

Sits between the protocol layer and the window layer. Manages state and drives timing.

Inherits `WaylandShellIntegration` which provides the `windowCreated(Window*)` signal that `Workspace` listens to for adding windows.

```cpp
class ExtSessionLockV1Integration : public WaylandShellIntegration
{
    Q_OBJECT
public:
    explicit ExtSessionLockV1Integration(QObject *parent = nullptr);
};
```

For ext-session-lock specifically, the integration is responsible for:
- Checking if a lock is already active when `lockRequested` fires
- Setting lock state so `isScreenLocked()` returns `true` immediately
- Creating window objects when `lockSurfaceRequested` fires
- Watching `RenderLoop::framePresented` on each output and calling `sendLocked()` only once all outputs have presented a locked frame (see `LockScreenPresentationWatcher` in `wayland_server.cpp`)
- Calling `sendFinished()` to reject a lock if one is already held
- Reversing everything on `unlockRequested`

---

## Wiring in `wayland_server.cpp/h`

Integrations are instantiated in `WaylandServer::initWorkspace()` (not `init()`). Add a forward declaration and member to `wayland_server.h`:

```cpp
// forward declaration alongside other protocol classes at the top
class YourIntegration;

// member variable
YourIntegration *m_yourIntegration = nullptr;
```

Store the integration as a member (not a local variable) if other subsystems need to query its state — e.g. `isScreenLocked()` needs to call `m_yourIntegration->isLocked()`.

In `wayland_server.cpp`, include the integration header and instantiate in `initWorkspace()`:
```cpp
#include "yourintegration.h"

// in initWorkspace():
m_yourIntegration = new YourIntegration(this);
connect(m_yourIntegration, &YourIntegration::windowCreated,
        this, &WaylandServer::registerWindow);
```

---

## Reading Order for Understanding KWin

Before implementing anything substantial, read these files in order:

1. `src/wayland_server.cpp` — how all protocols are instantiated and connected
2. `src/input.cpp` — `ScreenLockerFilter` — how `isScreenLocked()` and `isLockScreen()` gate input
3. `src/layershellv1window.cpp/h` — closest analog to a lock surface window
4. `src/window.h` — base class virtual methods you'll override
5. `src/waylandwindow.h/cpp` — intermediate base, where `isLockScreen()` currently lives
