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
class ExtWorkspaceManagerV1InterfacePrivate;
class ExtWorkspaceGroupHandleV1InterfacePrivate;
class ExtWorkspaceHandleV1InterfacePrivate;
class ExtWorkspaceGroupHandleV1Interface;
class ExtWorkspaceHandleV1Interface;

/**
 * Implements ext_workspace_manager_v1.
 *
 * Exposes the list of virtual desktops (workspaces) to clients such as taskbars
 * and pagers. This is the compositor side of the ext-workspace-v1 protocol.
 *
 * KWin uses a single workspace group containing all outputs.
 */
class ExtWorkspaceManagerV1Interface : public QObject
{
    Q_OBJECT

public:
    explicit ExtWorkspaceManagerV1Interface(Display *display, QObject *parent = nullptr);
    ~ExtWorkspaceManagerV1Interface() override;

    /**
     * Creates a new workspace handle and advertises it to all connected clients.
     * The handle is owned by this manager.
     */
    ExtWorkspaceHandleV1Interface *createWorkspace();

    /**
     * Removes the workspace handle, sending workspace_leave and removed events.
     * Does NOT call done(); the caller must do so.
     */
    void removeWorkspace(ExtWorkspaceHandleV1Interface *handle);

    /**
     * Notifies all clients that an output joined the workspace group.
     * Does NOT call done(); the caller must do so.
     */
    void outputAdded(OutputInterface *output);

    /**
     * Notifies all clients that an output left the workspace group.
     * Does NOT call done(); the caller must do so.
     */
    void outputRemoved(OutputInterface *output);

    /**
     * Sends the done event to all connected manager resources, signalling that
     * the current batch of changes is complete.
     */
    void done();

    /** Returns the single workspace group (spans all outputs). */
    ExtWorkspaceGroupHandleV1Interface *extWorkspaceGroup() const
    {
        return m_group;
    }

private:
    friend class ExtWorkspaceManagerV1InterfacePrivate;
    friend class ExtWorkspaceGroupHandleV1InterfacePrivate;
    std::unique_ptr<ExtWorkspaceManagerV1InterfacePrivate> d;
    ExtWorkspaceGroupHandleV1Interface *m_group = nullptr;
    QList<ExtWorkspaceHandleV1Interface *> m_handles;
};

/**
 * Represents the single workspace group in ext_workspace_group_handle_v1.
 *
 * KWin has one group that spans all outputs.
 */
class ExtWorkspaceGroupHandleV1Interface : public QObject
{
    Q_OBJECT

public:
    ~ExtWorkspaceGroupHandleV1Interface() override;

Q_SIGNALS:
    void createWorkspaceRequested(const QString &name);

private:
    explicit ExtWorkspaceGroupHandleV1Interface(ExtWorkspaceManagerV1Interface *manager);
    void outputAdded(OutputInterface *output);
    void outputRemoved(OutputInterface *output);

    friend class ExtWorkspaceManagerV1Interface;
    friend class ExtWorkspaceManagerV1InterfacePrivate;
    friend class ExtWorkspaceGroupHandleV1InterfacePrivate;
    std::unique_ptr<ExtWorkspaceGroupHandleV1InterfacePrivate> d;
    QList<OutputInterface *> m_outputs;
};

/**
 * Represents one virtual desktop in ext_workspace_handle_v1.
 */
class ExtWorkspaceHandleV1Interface : public QObject
{
    Q_OBJECT

public:
    ~ExtWorkspaceHandleV1Interface() override;

    void setName(const QString &name);
    void setId(const QString &id);
    /** Coordinates as a flat array of uint32_t values (e.g. [col, row] for 2D). */
    void setCoordinates(const QList<uint32_t> &coords);
    /** State bitmask: active=1, urgent=2, hidden=4. */
    void setState(uint32_t state);

Q_SIGNALS:
    void activateRequested();
    void deactivateRequested();
    void removeRequested();

private:
    explicit ExtWorkspaceHandleV1Interface(ExtWorkspaceManagerV1Interface *manager);

    friend class ExtWorkspaceManagerV1Interface;
    friend class ExtWorkspaceManagerV1InterfacePrivate;
    friend class ExtWorkspaceHandleV1InterfacePrivate;
    friend class ExtWorkspaceGroupHandleV1InterfacePrivate;
    std::unique_ptr<ExtWorkspaceHandleV1InterfacePrivate> d;
    QString m_name;
    QString m_id;
    QList<uint32_t> m_coords;
    uint32_t m_state = 0;
};

} // namespace KWin
