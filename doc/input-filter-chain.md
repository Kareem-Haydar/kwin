# KWin Input Filter Chain

## How It Works

Input events pass through a chain of `InputEventFilter` objects registered with `InputRedirection`. Each filter's handler method returns:

- `false` — "I didn't handle this, pass it on"
- `true` — "I consumed this event, stop here"

The iteration loop (`src/input.h:150`):

```cpp
void processFilters(auto method, const auto &...args)
{
    for (const auto filter : std::as_const(m_filters)) {
        if ((filter->*method)(args...)) {
            return;  // consumed — stop iterating
        }
    }
}
```

Filters are sorted by their `InputFilterOrder::Order` weight (lower = earlier). Registering with `installInputEventFilter()` inserts into the sorted list automatically.

## Filter Order and Purpose

Defined in `src/input.h` as `enum InputFilterOrder::Order`. Listed in chain order:

### Hardware / Accessibility

| Filter | File | What it does |
|---|---|---|
| `PlaceholderOutput` | `src/placeholderinputeventfilter.cpp` | Swallows all input when no real outputs exist (all monitors disconnected) |
| `Dpms` | `src/dpmsinputeventfilter.cpp` | Swallows input and wakes screens when displays are powered off |
| `ButtonRebind` | `src/plugins/buttonrebinds/` | Remaps extra mouse buttons to keyboard keys before anything else sees them |
| `SlowKeys` | `src/plugins/slowkeys/` | Adds a hold delay before a key is registered |
| `BounceKeys` | `src/plugins/bouncekeys/` | Debounces keys to prevent double-presses |
| `StickyKeys` | `src/plugins/stickykeys/` | Latches modifier keys so they don't need to be held |
| `MouseKeys` | `src/plugins/mousekeys/` | Drives the pointer from the keyboard numpad |
| `EisInput` | `src/plugins/eis/` | EIS (External Input Source) — remote/virtual input injection |

### Security / Session State

| Filter | File | What it does |
|---|---|---|
| `VirtualTerminal` | `src/input.cpp` | Intercepts Ctrl+Alt+Fn to switch TTYs — must work even on lock screen |
| `LockScreen` | `src/input.cpp` | Blocks everything and routes only to lock surfaces when `isScreenLocked()` returns true |

The `LockScreen` filter pattern:

```cpp
bool pointerMotion(PointerMotionEvent *event) override
{
    if (!waylandServer()->isScreenLocked()) {
        return false;  // not locked — pass through
    }
    // locked: notify locker of user activity, then route only to allowed surfaces
    ScreenLocker::KSldApp::self()->userActivity();
    // ...forward to surface only if surfaceAllowed() returns true
    return true;  // always consume when locked
}
```

`surfaceAllowed()` permits only surfaces where `isLockScreen() || isInputMethod() || isLockScreenOverlay()`. This is why `ExtSessionLockV1Window::isLockScreen()` must return `true`.

### High-Priority Compositor Operations

| Filter | File | What it does |
|---|---|---|
| `ScreenEdge` | `src/input.cpp` | Detects pointer hitting screen edges for hot corners / edge actions |
| `DragAndDrop` | `src/input.cpp` | Handles in-progress Wayland DnD operations |
| `WindowSelector` | `src/input.cpp` | Active when the user is in window-picker mode (e.g. "click a window to apply rule") |
| `TabBox` | `src/input.cpp` | Alt-Tab switcher — grabs all input while active |
| `GlobalShortcut` | `src/input.cpp` | KGlobalAccel shortcuts (Super+key, etc.) |
| `Effects` | `src/input.cpp` | Lets effects intercept input (e.g. Overview, Zoom grab mouse/keyboard) |
| `InteractiveMoveResize` | `src/input.cpp` | In-progress window move/resize drag |
| `Popup` | `src/popup_input_filter.cpp` | Grabs input for open popup menus / context menus |

### Per-Surface Routing

| Filter | File | What it does |
|---|---|---|
| `Decoration` | `src/input.cpp` | Routes clicks/hovers on window title bars and borders |
| `WindowAction` | `src/input.cpp` | Meta+drag to move windows, modifier+scroll for opacity, etc. |
| `XWayland` | `src/xwayland/xwayland.cpp` | Forwards events to XWayland for X11 clients |
| `InternalWindow` | `src/input.cpp` | Routes to KWin's own Qt-rendered internal windows (e.g. logout dialog) |
| `InputMethod` | `src/input.cpp` | Routes to the on-screen keyboard / input method |
| `Forward` | `src/input.cpp` | **Final fallback** — delivers to the Wayland seat, which sends to the focused surface |

The `Forward` filter is where normal input ends up. Every filter above it is an opportunity to intercept first.

## Hit-Testing Under Lock

`InputRedirection::findToplevel()` (`src/input.cpp:3448`) enforces the lock independently of the filter chain:

```cpp
const bool isScreenLocked = waylandServer() && waylandServer()->isScreenLocked();
// ...
if (isScreenLocked) {
    if (!window->isLockScreen() && !window->isInputMethod() && !window->isLockScreenOverlay()) {
        continue;  // skip non-lock windows during hit test
    }
}
```

So even if a filter bug let an event through, hit-testing would still refuse to target a non-lock window.

## Adding a New Filter

```cpp
class MyFilter : public InputEventFilter
{
public:
    MyFilter() : InputEventFilter(InputFilterOrder::MyOrder) {}

    bool pointerButton(PointerButtonEvent *event) override
    {
        if (/* not my concern */) {
            return false;
        }
        // handle it
        return true;
    }
};
```

Add the order value to `InputFilterOrder::Order` in `src/input.h` at the appropriate position. Install with `input()->installInputEventFilter(new MyFilter())`.
