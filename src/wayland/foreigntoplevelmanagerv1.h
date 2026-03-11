/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 KWin Authors

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>
#include <memory>

namespace KWin
{

class Display;
class OutputInterface;
class SurfaceInterface;
class Window;

class ForeignToplevelHandleV1Interface;
class ForeignToplevelManagerV1InterfacePrivate;
class ForeignToplevelHandleV1InterfacePrivate;

/**
 * Implements zwlr_foreign_toplevel_manager_v1.
 *
 * Exposes the list of open toplevels to clients such as taskbars and docks.
 * This is the compositor side of the wlr-foreign-toplevel-management protocol.
 */
class ForeignToplevelManagerV1Interface : public QObject
{
    Q_OBJECT

public:
    explicit ForeignToplevelManagerV1Interface(Display *display, QObject *parent = nullptr);
    ~ForeignToplevelManagerV1Interface() override;

    /**
     * Creates a new toplevel handle for the given window and advertises it to all
     * connected clients. The handle is owned by this manager.
     */
    ForeignToplevelHandleV1Interface *createHandle(Window *window);

private:
    friend class ForeignToplevelHandleV1InterfacePrivate;
    std::unique_ptr<ForeignToplevelManagerV1InterfacePrivate> d;
};

/**
 * Represents one open toplevel window in the zwlr_foreign_toplevel_handle_v1 protocol.
 *
 * Each window gets one handle object. That handle is advertised to every client that
 * binds to the manager; each client receives its own wl_resource for the handle.
 */
class ForeignToplevelHandleV1Interface : public QObject
{
    Q_OBJECT

public:
    ~ForeignToplevelHandleV1Interface() override;

    void setTitle(const QString &title);
    void setAppId(const QString &appId);
    void setMinimized(bool minimized);
    void setMaximized(bool maximized);
    void setFullscreen(bool fullscreen);
    void setActive(bool active);
    void setOutputs(const QList<OutputInterface *> &outputs);
    void setParentHandle(ForeignToplevelHandleV1Interface *parent);

    /** Sends the closed event and schedules this handle for deletion. */
    void sendClosed();

Q_SIGNALS:
    void maximizeRequested();
    void unmaximizeRequested();
    void minimizeRequested();
    void unminimizeRequested();
    void fullscreenRequested(KWin::OutputInterface *output);
    void unfullscreenRequested();
    void activateRequested();
    void closeRequested();

private:
    explicit ForeignToplevelHandleV1Interface(ForeignToplevelManagerV1Interface *manager,
                                              Window *window);

    friend class ForeignToplevelManagerV1Interface;
    friend class ForeignToplevelManagerV1InterfacePrivate;
    std::unique_ptr<ForeignToplevelHandleV1InterfacePrivate> d;
};

} // namespace KWin
