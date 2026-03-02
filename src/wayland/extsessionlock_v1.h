/*
    SPDX-FileCopyrightText: 2025 KWin Authors

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include "kwin_export.h"
#include "qwayland-server-ext-session-lock-v1.h"

#include <QObject>
#include <QPointer>

namespace KWin
{

class Display;
class SurfaceInterface;
class OutputInterface;
class ExtSessionLockV1Interface;
class ExtSessionLockSurfaceV1Interface;

class KWIN_EXPORT ExtSessionLockManagerV1Interface : public QObject, private QtWaylandServer::ext_session_lock_manager_v1
{
    Q_OBJECT
public:
    explicit ExtSessionLockManagerV1Interface(Display *display, QObject *parent = nullptr);

Q_SIGNALS:
    void lockRequested(ExtSessionLockV1Interface *lock);

private:
    void ext_session_lock_manager_v1_destroy(Resource *resource) override;
    void ext_session_lock_manager_v1_lock(Resource *resource, uint32_t id) override;
};

class KWIN_EXPORT ExtSessionLockV1Interface : public QObject, private QtWaylandServer::ext_session_lock_v1
{
    Q_OBJECT
public:
    explicit ExtSessionLockV1Interface(wl_client *client, uint32_t id, uint32_t version, QObject *parent = nullptr);

    void sendLocked();
    void sendFinished();

Q_SIGNALS:
    void lockSurfaceRequested(ExtSessionLockSurfaceV1Interface *surface);
    void unlockRequested();

private:
    void ext_session_lock_v1_destroy(Resource *resource) override;
    void ext_session_lock_v1_get_lock_surface(Resource *resource, uint32_t id, ::wl_resource *surface, ::wl_resource *output) override;
    void ext_session_lock_v1_unlock_and_destroy(Resource *resource) override;

    bool m_locked = false;
};

class ExtSessionLockSurfaceV1Interface : public QObject, private QtWaylandServer::ext_session_lock_surface_v1
{
    Q_OBJECT
public:
    explicit ExtSessionLockSurfaceV1Interface(wl_client *client, uint32_t id, uint32_t version,
                                              SurfaceInterface *surface, OutputInterface *output);

    SurfaceInterface *surface() const;
    OutputInterface *output() const;

    void sendConfigure(uint32_t serial, uint32_t width, uint32_t height);

Q_SIGNALS:
    void aboutToBeDestroyed();
    void configureAcknowledged(uint32_t serial);

private:
    void ext_session_lock_surface_v1_destroy_resource(Resource *resource) override;
    void ext_session_lock_surface_v1_destroy(Resource *resource) override;
    void ext_session_lock_surface_v1_ack_configure(Resource *resource, uint32_t serial) override;

    SurfaceInterface *m_surface;
    OutputInterface *m_output;
};

} // namespace KWin
