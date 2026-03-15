# KWin Codebase Reading List

Ordered from foundation to specialised. Read headers unless noted otherwise.

---

## 1. Architecture Overview

**`src/wayland/DESIGN.md`** (82 lines)
Start here. Explains the pimpl pattern used throughout the Wayland server layer
and how `QtWaylandServer`-generated classes are wrapped.

---

## 2. Core Types

**`src/effect/globals.h`**
Enums used everywhere: `Layer`, `WindowType`, `MaximizeMode`, `ElectricBorder`,
`ElectricBorderAction`, `StrutArea`, etc. Essential vocabulary before reading anything else.

**`src/core/output.h`**
`LogicalOutput` class. Understand this before touching anything output-related.

**`src/virtualdesktops.h`**
`VirtualDesktop` and `VirtualDesktopManager`. Important for workspace protocol work.

---

## 3. The Window Model

**`src/window.h`** (2196 lines — skim, don't read linearly)
The massive base class for all window types. Focus on: property signals,
`Layer`/stacking methods, `output()`, `isLockScreen()`, `isPlaceable()`,
`acceptsFocus()`, `skipSwitcher()`, and the foreign toplevel handle members.

**`src/waylandshellintegration.h`**
The tiny base class all shell integrations inherit from. Shows the pattern.

**`src/waylandwindow.h`**
Wayland-specific window base, sits between `Window` and concrete types like
`XdgToplevelWindow`.

**`src/xdgshellwindow.h`**
The most complete concrete window type. Good reference for how to implement
the full window lifecycle.

---

## 4. The Compositor / Scene

**`src/compositor.h`**
Ties together the render loop and scene. Understand `start()` and `scene()`.

**`src/layers.cpp`** (read the whole file)
How windows are sorted into stacking order. Directly relevant to
`belongsToLayer()` returning `OverlayLayer`.

**`src/scene/workspacescene.h`**
The scene that composites everything. Shows the rendering pipeline entry point.

**`src/core/renderloop.h`**
`framePresented` signal — used directly in `LockPresentationWatcher`.

---

## 5. The Server Glue

**`src/wayland_server.h`**
The central hub. Know what it creates, what it exposes, and what signals it emits.

**`src/wayland/display.h`**
The `Display` wrapper around `wl_display`. Used to register globals and get serials.

**`src/wayland/xdgshell.h`**
The most mature and complete protocol implementation in the codebase. Best
reference for how to write a new protocol server object using the
pimpl/`QtWaylandServer` pattern.

---

## 6. Input

**`src/input.h`**
`InputRedirection` and the `InputEventFilter` chain. Understanding this is needed
for gating input behind the lock screen, implementing input grabs, or intercepting
events at a specific priority level.

---

## 7. Scripting / Plugin System (for Lua redesign)

**`src/scripting/workspace_wrapper.h`**
The full JS scripting API surface: window list, stacking order, virtual desktops,
screen geometry, `raiseWindow()`, electric border and shortcut registration.
This is the baseline the Lua workspace API needs to cover.

**`src/scripting/scripting.h`**
The script engine itself — how JS scripts are loaded, sandboxed, and connected
to the workspace wrapper.

**`src/scripting/scriptedeffect.h`**
The bridge between JS scripts and the C++ effects API: animations, window
thumbnails, input events. The harder half of the Lua API surface.

**`src/effect/effecthandler.h`** (1132 lines — skim)
The full C++ effect API. Everything `scriptedeffect.h` wraps comes from here.
Essential reading before designing the Lua animation/rendering API.

**`src/tabbox/tabbox.h`**
The window switcher — the primary thing that becomes a Lua plugin once input
grabbing exists. Shows what compositor-side state a switcher plugin needs.

---

## 8. Our Implementations (read in order)

### Session Lock (`ext-session-lock-v1`)
1. `src/wayland/extsessionlock_v1.h/.cpp` — Wayland protocol layer
2. `src/extsessionlockv1integration.h/.cpp` — KWin integration, `LockPresentationWatcher`
3. `src/extsessionlockv1window.h/.cpp` — lock screen window type

### Foreign Toplevel & Workspace
4. `src/wayland/foreigntoplevelmanagerv1.h/.cpp` — `zwlr-foreign-toplevel-management-v1`
5. `src/wayland/extforeigntoplevellist_v1.h/.cpp` — `ext-foreign-toplevel-list-v1`
6. `src/wayland/extworkspace_v1.h/.cpp` — `ext-workspace-v1`
