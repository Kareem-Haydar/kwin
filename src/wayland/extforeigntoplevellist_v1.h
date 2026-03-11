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
class Window;
class ExtForeignToplevelListV1InterfacePrivate;
class ExtForeignToplevelHandleV1InterfacePrivate;
class ExtForeignToplevelHandleV1Interface;

/**
 * Implements ext_foreign_toplevel_list_v1.
 *
 * Read-only enumeration of mapped toplevels for clients such as taskbars.
 * Intentionally minimal: no state (maximize/minimize/active) and no control
 * requests. Designed to be extended by other protocols.
 */
class ExtForeignToplevelListV1Interface : public QObject
{
    Q_OBJECT

public:
    explicit ExtForeignToplevelListV1Interface(Display *display, QObject *parent = nullptr);
    ~ExtForeignToplevelListV1Interface() override;

    /**
     * Creates a new handle for the given window and advertises it to all
     * connected clients. The handle is owned by this list.
     */
    ExtForeignToplevelHandleV1Interface *createHandle(Window *window);

private:
    friend class ExtForeignToplevelHandleV1InterfacePrivate;
    friend class ExtForeignToplevelListV1InterfacePrivate;
    std::unique_ptr<ExtForeignToplevelListV1InterfacePrivate> d;
    QList<ExtForeignToplevelHandleV1Interface *> m_handles;
};

/**
 * Represents one mapped toplevel in the ext_foreign_toplevel_handle_v1 protocol.
 *
 * Provides title, app_id, and a stable unique identifier. All fields are
 * read-only from the client's perspective.
 */
class ExtForeignToplevelHandleV1Interface : public QObject
{
    Q_OBJECT

public:
    ~ExtForeignToplevelHandleV1Interface() override;

    void setTitle(const QString &title);
    void setAppId(const QString &appId);

    /** Sends the closed event and schedules this handle for deletion. */
    void sendClosed();

private:
    explicit ExtForeignToplevelHandleV1Interface(ExtForeignToplevelListV1Interface *list,
                                                 Window *window);

    friend class ExtForeignToplevelListV1Interface;
    friend class ExtForeignToplevelListV1InterfacePrivate;
    friend class ExtForeignToplevelHandleV1InterfacePrivate;
    std::unique_ptr<ExtForeignToplevelHandleV1InterfacePrivate> d;
    QString m_title;
    QString m_appId;
    QString m_identifier;
    bool m_closed = false;
};

} // namespace KWin
