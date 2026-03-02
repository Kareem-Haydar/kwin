/*
    SPDX-FileCopyrightText: 2025 KWin Authors

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "extsessionlock_v1.h"

#include "display.h"
#include "output.h"
#include "surface.h"

namespace KWin
{

static constexpr uint32_t s_version = 1;

// --- ExtSessionLockManagerV1Interface ---

ExtSessionLockManagerV1Interface::ExtSessionLockManagerV1Interface(Display *display, QObject *parent)
    : QObject(parent)
    , QtWaylandServer::ext_session_lock_manager_v1(*display, s_version)
{
}

void ExtSessionLockManagerV1Interface::ext_session_lock_manager_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void ExtSessionLockManagerV1Interface::ext_session_lock_manager_v1_lock(Resource *resource, uint32_t id)
{
    auto *lock = new ExtSessionLockV1Interface(resource->client(), id, resource->version(), this);
    Q_EMIT lockRequested(lock);
}

// --- ExtSessionLockV1Interface ---

ExtSessionLockV1Interface::ExtSessionLockV1Interface(wl_client *client, uint32_t id, uint32_t version, QObject *parent)
    : QObject(parent)
    , QtWaylandServer::ext_session_lock_v1(client, id, version)
{
}

void ExtSessionLockV1Interface::sendLocked()
{
    m_locked = true;
    send_locked(resource()->handle);
}

void ExtSessionLockV1Interface::sendFinished()
{
    send_finished(resource()->handle);
}

void ExtSessionLockV1Interface::ext_session_lock_v1_destroy(Resource *resource)
{
    if (m_locked) {
        wl_resource_post_error(resource->handle, error_invalid_destroy,
                               "destroying a locked ext_session_lock_v1 is not allowed");
        return;
    }
    wl_resource_destroy(resource->handle);
}

void ExtSessionLockV1Interface::ext_session_lock_v1_get_lock_surface(Resource *resource, uint32_t id,
                                                                     ::wl_resource *surface, ::wl_resource *output)
{
    auto *lockSurface = new ExtSessionLockSurfaceV1Interface(resource->client(), id, resource->version(),
                                                             SurfaceInterface::get(surface),
                                                             OutputInterface::get(output));
    Q_EMIT lockSurfaceRequested(lockSurface);
}

void ExtSessionLockV1Interface::ext_session_lock_v1_unlock_and_destroy(Resource *resource)
{
    if (!m_locked) {
        wl_resource_post_error(resource->handle, error_invalid_unlock,
                               "unlock requested but locked event was never sent");
        return;
    }
    Q_EMIT unlockRequested();
    wl_resource_destroy(resource->handle);
}

// --- ExtSessionLockSurfaceV1Interface ---

ExtSessionLockSurfaceV1Interface::ExtSessionLockSurfaceV1Interface(wl_client *client, uint32_t id, uint32_t version,
                                                                   SurfaceInterface *surface, OutputInterface *output)
    : QtWaylandServer::ext_session_lock_surface_v1(client, id, version)
    , m_surface(surface)
    , m_output(output)
{
}

SurfaceInterface *ExtSessionLockSurfaceV1Interface::surface() const
{
    return m_surface;
}

OutputInterface *ExtSessionLockSurfaceV1Interface::output() const
{
    return m_output;
}

void ExtSessionLockSurfaceV1Interface::sendConfigure(uint32_t serial, uint32_t width, uint32_t height)
{
    send_configure(resource()->handle, serial, width, height);
}

void ExtSessionLockSurfaceV1Interface::ext_session_lock_surface_v1_destroy_resource(Resource *resource)
{
    delete this;
}

void ExtSessionLockSurfaceV1Interface::ext_session_lock_surface_v1_destroy(Resource *resource)
{
    Q_EMIT aboutToBeDestroyed();
    wl_resource_destroy(resource->handle);
}

void ExtSessionLockSurfaceV1Interface::ext_session_lock_surface_v1_ack_configure(Resource *resource, uint32_t serial)
{
    // TODO: validate serial before emitting signal

    Q_EMIT configureAcknowledged(serial);
}

} // namespace KWin
