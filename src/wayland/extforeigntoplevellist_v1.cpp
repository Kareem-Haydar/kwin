/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 KWin Authors

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "extforeigntoplevellist_v1.h"
#include "display.h"
#include "window.h"

#include <QUuid>

#include "qwayland-server-ext-foreign-toplevel-list-v1.h"

namespace KWin
{

static const quint32 s_listVersion = 1;

// ─── Handle private ───────────────────────────────────────────────────────────

class ExtForeignToplevelHandleV1InterfacePrivate
    : public QtWaylandServer::ext_foreign_toplevel_handle_v1
{
public:
    ExtForeignToplevelHandleV1InterfacePrivate(ExtForeignToplevelHandleV1Interface *q,
                                               ExtForeignToplevelListV1Interface *list);

    void sendToResource(Resource *resource);

    ExtForeignToplevelHandleV1Interface *q;
    ExtForeignToplevelListV1Interface *list;

protected:
    void ext_foreign_toplevel_handle_v1_destroy(Resource *resource) override;
};

ExtForeignToplevelHandleV1InterfacePrivate::ExtForeignToplevelHandleV1InterfacePrivate(
    ExtForeignToplevelHandleV1Interface *q, ExtForeignToplevelListV1Interface *list)
    : q(q)
    , list(list)
{
}

void ExtForeignToplevelHandleV1InterfacePrivate::sendToResource(Resource *resource)
{
    // identifier is sent first and only once
    send_identifier(resource->handle, q->m_identifier);
    if (!q->m_title.isEmpty()) {
        send_title(resource->handle, q->m_title);
    }
    if (!q->m_appId.isEmpty()) {
        send_app_id(resource->handle, q->m_appId);
    }
    send_done(resource->handle);
}

void ExtForeignToplevelHandleV1InterfacePrivate::ext_foreign_toplevel_handle_v1_destroy(
    Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

// ─── Handle public ────────────────────────────────────────────────────────────

ExtForeignToplevelHandleV1Interface::ExtForeignToplevelHandleV1Interface(
    ExtForeignToplevelListV1Interface *list, Window *window)
    : d(std::make_unique<ExtForeignToplevelHandleV1InterfacePrivate>(this, list))
{
    // Use the window's QUuid as a stable 32-char identifier (hex, no dashes).
    m_identifier = window->internalId().toString(QUuid::Id128);
}

ExtForeignToplevelHandleV1Interface::~ExtForeignToplevelHandleV1Interface() = default;

void ExtForeignToplevelHandleV1Interface::setTitle(const QString &title)
{
    if (m_title == title) {
        return;
    }
    m_title = title;
    const auto resources = d->resourceMap();
    for (auto *resource : resources) {
        d->send_title(resource->handle, title);
        d->send_done(resource->handle);
    }
}

void ExtForeignToplevelHandleV1Interface::setAppId(const QString &appId)
{
    if (m_appId == appId) {
        return;
    }
    m_appId = appId;
    const auto resources = d->resourceMap();
    for (auto *resource : resources) {
        d->send_app_id(resource->handle, appId);
        d->send_done(resource->handle);
    }
}

void ExtForeignToplevelHandleV1Interface::sendClosed()
{
    if (m_closed) {
        return;
    }
    m_closed = true;
    const auto resources = d->resourceMap();
    for (auto *resource : resources) {
        d->send_closed(resource->handle);
    }
    deleteLater();
}

// ─── List private ─────────────────────────────────────────────────────────────

class ExtForeignToplevelListV1InterfacePrivate
    : public QtWaylandServer::ext_foreign_toplevel_list_v1
{
public:
    ExtForeignToplevelListV1InterfacePrivate(ExtForeignToplevelListV1Interface *q,
                                             Display *display);

    ExtForeignToplevelListV1Interface *q;

protected:
    void ext_foreign_toplevel_list_v1_bind_resource(Resource *resource) override;
    void ext_foreign_toplevel_list_v1_stop(Resource *resource) override;
    void ext_foreign_toplevel_list_v1_destroy(Resource *resource) override;
};

ExtForeignToplevelListV1InterfacePrivate::ExtForeignToplevelListV1InterfacePrivate(
    ExtForeignToplevelListV1Interface *q, Display *display)
    : QtWaylandServer::ext_foreign_toplevel_list_v1(*display, s_listVersion)
    , q(q)
{
}

void ExtForeignToplevelListV1InterfacePrivate::ext_foreign_toplevel_list_v1_bind_resource(
    Resource *listResource)
{
    for (ExtForeignToplevelHandleV1Interface *handle : std::as_const(q->m_handles)) {
        auto *handleResource = handle->d->add(listResource->client(), listResource->version());
        send_toplevel(listResource->handle, handleResource->handle);
        handle->d->sendToResource(handleResource);
    }
}

void ExtForeignToplevelListV1InterfacePrivate::ext_foreign_toplevel_list_v1_stop(
    Resource *resource)
{
    send_finished(resource->handle);
    wl_resource_destroy(resource->handle);
}

void ExtForeignToplevelListV1InterfacePrivate::ext_foreign_toplevel_list_v1_destroy(
    Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

// ─── List public ──────────────────────────────────────────────────────────────

ExtForeignToplevelListV1Interface::ExtForeignToplevelListV1Interface(Display *display,
                                                                     QObject *parent)
    : QObject(parent)
    , d(std::make_unique<ExtForeignToplevelListV1InterfacePrivate>(this, display))
{
}

ExtForeignToplevelListV1Interface::~ExtForeignToplevelListV1Interface() = default;

ExtForeignToplevelHandleV1Interface *ExtForeignToplevelListV1Interface::createHandle(
    Window *window)
{
    auto *handle = new ExtForeignToplevelHandleV1Interface(this, window);
    handle->setParent(this);
    m_handles << handle;

    connect(handle, &QObject::destroyed, this, [this, handle]() {
        m_handles.removeOne(handle);
    });

    // Advertise the new handle to all currently connected clients.
    const auto listResources = d->resourceMap();
    for (auto *listResource : listResources) {
        auto *handleResource = handle->d->add(listResource->client(), listResource->version());
        d->send_toplevel(listResource->handle, handleResource->handle);
        handle->d->sendToResource(handleResource);
    }

    return handle;
}

} // namespace KWin

#include "moc_extforeigntoplevellist_v1.cpp"
