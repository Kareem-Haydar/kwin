/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 KWin Authors

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "foreigntoplevelmanagerv1.h"
#include "display.h"
#include "output.h"

#include "qwayland-server-wlr-foreign-toplevel-management-unstable-v1.h"

#include <QPointer>

namespace KWin
{

static const quint32 s_managerVersion = 3;

// ─── Handle private ──────────────────────────────────────────────────────────

class ForeignToplevelHandleV1InterfacePrivate : public QtWaylandServer::zwlr_foreign_toplevel_handle_v1
{
public:
    ForeignToplevelHandleV1InterfacePrivate(ForeignToplevelHandleV1Interface *q,
                                            ForeignToplevelManagerV1Interface *manager);

    void sendToResource(Resource *resource);
    void sendStateToResource(Resource *resource);
    void broadcastState();

    ForeignToplevelHandleV1Interface *q;
    ForeignToplevelManagerV1Interface *manager;

    QString title;
    QString appId;
    bool minimized = false;
    bool maximized = false;
    bool fullscreen = false;
    bool active = false;
    QList<OutputInterface *> outputs;
    QPointer<ForeignToplevelHandleV1Interface> parentHandle;
    bool closed = false;

protected:
    void zwlr_foreign_toplevel_handle_v1_set_maximized(Resource *resource) override;
    void zwlr_foreign_toplevel_handle_v1_unset_maximized(Resource *resource) override;
    void zwlr_foreign_toplevel_handle_v1_set_minimized(Resource *resource) override;
    void zwlr_foreign_toplevel_handle_v1_unset_minimized(Resource *resource) override;
    void zwlr_foreign_toplevel_handle_v1_activate(Resource *resource, wl_resource *seat) override;
    void zwlr_foreign_toplevel_handle_v1_close(Resource *resource) override;
    void zwlr_foreign_toplevel_handle_v1_set_rectangle(Resource *resource,
                                                       wl_resource *surface,
                                                       int32_t x, int32_t y,
                                                       int32_t width, int32_t height) override;
    void zwlr_foreign_toplevel_handle_v1_destroy(Resource *resource) override;
    void zwlr_foreign_toplevel_handle_v1_set_fullscreen(Resource *resource,
                                                        wl_resource *output) override;
    void zwlr_foreign_toplevel_handle_v1_unset_fullscreen(Resource *resource) override;
};

ForeignToplevelHandleV1InterfacePrivate::ForeignToplevelHandleV1InterfacePrivate(
    ForeignToplevelHandleV1Interface *q, ForeignToplevelManagerV1Interface *manager)
    : q(q)
    , manager(manager)
{
}

void ForeignToplevelHandleV1InterfacePrivate::sendStateToResource(Resource *resource)
{
    // Build a wl_array of uint32_t state values
    QList<uint32_t> stateList;
    if (maximized) {
        stateList << state_maximized;
    }
    if (minimized) {
        stateList << state_minimized;
    }
    if (active) {
        stateList << state_activated;
    }
    if (fullscreen && resource->version() >= 2) {
        stateList << state_fullscreen;
    }

    QByteArray stateData(reinterpret_cast<const char *>(stateList.constData()),
                         stateList.size() * sizeof(uint32_t));
    send_state(resource->handle, stateData);
}

void ForeignToplevelHandleV1InterfacePrivate::sendToResource(Resource *resource)
{
    if (!title.isEmpty()) {
        send_title(resource->handle, title);
    }
    if (!appId.isEmpty()) {
        send_app_id(resource->handle, appId);
    }
    for (OutputInterface *output : std::as_const(outputs)) {
        if (auto *outputResource = output->clientResources(resource->client()).value(0)) {
            send_output_enter(resource->handle, outputResource);
        }
    }
    sendStateToResource(resource);
    send_done(resource->handle);
}

void ForeignToplevelHandleV1InterfacePrivate::broadcastState()
{
    const auto resourceList = resourceMap();
    for (auto *resource : resourceList) {
        sendStateToResource(resource);
        send_done(resource->handle);
    }
}

void ForeignToplevelHandleV1InterfacePrivate::zwlr_foreign_toplevel_handle_v1_set_maximized(Resource *)
{
    Q_EMIT q->maximizeRequested();
}

void ForeignToplevelHandleV1InterfacePrivate::zwlr_foreign_toplevel_handle_v1_unset_maximized(Resource *)
{
    Q_EMIT q->unmaximizeRequested();
}

void ForeignToplevelHandleV1InterfacePrivate::zwlr_foreign_toplevel_handle_v1_set_minimized(Resource *)
{
    Q_EMIT q->minimizeRequested();
}

void ForeignToplevelHandleV1InterfacePrivate::zwlr_foreign_toplevel_handle_v1_unset_minimized(Resource *)
{
    Q_EMIT q->unminimizeRequested();
}

void ForeignToplevelHandleV1InterfacePrivate::zwlr_foreign_toplevel_handle_v1_activate(Resource *, wl_resource *)
{
    Q_EMIT q->activateRequested();
}

void ForeignToplevelHandleV1InterfacePrivate::zwlr_foreign_toplevel_handle_v1_close(Resource *)
{
    Q_EMIT q->closeRequested();
}

void ForeignToplevelHandleV1InterfacePrivate::zwlr_foreign_toplevel_handle_v1_set_rectangle(
    Resource *, wl_resource *, int32_t, int32_t, int32_t, int32_t)
{
    // Hint only — no action needed in the compositor currently.
}

void ForeignToplevelHandleV1InterfacePrivate::zwlr_foreign_toplevel_handle_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void ForeignToplevelHandleV1InterfacePrivate::zwlr_foreign_toplevel_handle_v1_set_fullscreen(
    Resource *, wl_resource *outputResource)
{
    OutputInterface *output = nullptr;
    if (outputResource) {
        output = OutputInterface::get(outputResource);
    }
    Q_EMIT q->fullscreenRequested(output);
}

void ForeignToplevelHandleV1InterfacePrivate::zwlr_foreign_toplevel_handle_v1_unset_fullscreen(Resource *)
{
    Q_EMIT q->unfullscreenRequested();
}

// ─── Handle public ────────────────────────────────────────────────────────────

ForeignToplevelHandleV1Interface::ForeignToplevelHandleV1Interface(
    ForeignToplevelManagerV1Interface *manager, Window *window)
    : d(std::make_unique<ForeignToplevelHandleV1InterfacePrivate>(this, manager))
{
    Q_UNUSED(window)
}

ForeignToplevelHandleV1Interface::~ForeignToplevelHandleV1Interface() = default;

void ForeignToplevelHandleV1Interface::setTitle(const QString &title)
{
    if (d->title == title) {
        return;
    }
    d->title = title;
    const auto resources = d->resourceMap();
    for (auto *resource : resources) {
        d->send_title(resource->handle, title);
        d->send_done(resource->handle);
    }
}

void ForeignToplevelHandleV1Interface::setAppId(const QString &appId)
{
    if (d->appId == appId) {
        return;
    }
    d->appId = appId;
    const auto resources = d->resourceMap();
    for (auto *resource : resources) {
        d->send_app_id(resource->handle, appId);
        d->send_done(resource->handle);
    }
}

void ForeignToplevelHandleV1Interface::setMinimized(bool minimized)
{
    if (d->minimized == minimized) {
        return;
    }
    d->minimized = minimized;
    d->broadcastState();
}

void ForeignToplevelHandleV1Interface::setMaximized(bool maximized)
{
    if (d->maximized == maximized) {
        return;
    }
    d->maximized = maximized;
    d->broadcastState();
}

void ForeignToplevelHandleV1Interface::setFullscreen(bool fullscreen)
{
    if (d->fullscreen == fullscreen) {
        return;
    }
    d->fullscreen = fullscreen;
    d->broadcastState();
}

void ForeignToplevelHandleV1Interface::setActive(bool active)
{
    if (d->active == active) {
        return;
    }
    d->active = active;
    d->broadcastState();
}

void ForeignToplevelHandleV1Interface::setOutputs(const QList<OutputInterface *> &outputs)
{
    const QList<OutputInterface *> old = d->outputs;
    d->outputs = outputs;

    const auto resources = d->resourceMap();
    for (auto *resource : resources) {
        // Send output_leave for removed outputs
        for (OutputInterface *output : old) {
            if (!outputs.contains(output)) {
                if (auto *r = output->clientResources(resource->client()).value(0)) {
                    d->send_output_leave(resource->handle, r);
                }
            }
        }
        // Send output_enter for added outputs
        for (OutputInterface *output : outputs) {
            if (!old.contains(output)) {
                if (auto *r = output->clientResources(resource->client()).value(0)) {
                    d->send_output_enter(resource->handle, r);
                }
            }
        }
        d->send_done(resource->handle);
    }
}

void ForeignToplevelHandleV1Interface::setParentHandle(ForeignToplevelHandleV1Interface *parent)
{
    if (d->parentHandle == parent) {
        return;
    }
    d->parentHandle = parent;

    const auto resources = d->resourceMap();
    for (auto *resource : resources) {
        if (resource->version() < 3) {
            continue;
        }
        wl_resource *parentResource = nullptr;
        if (parent) {
            if (auto *pr = parent->d->resourceMap().value(resource->client())) {
                parentResource = pr->handle;
            }
        }
        d->send_parent(resource->handle, parentResource);
        d->send_done(resource->handle);
    }
}

void ForeignToplevelHandleV1Interface::sendClosed()
{
    if (d->closed) {
        return;
    }
    d->closed = true;
    const auto resources = d->resourceMap();
    for (auto *resource : resources) {
        d->send_closed(resource->handle);
    }
    deleteLater();
}

// ─── Manager private ──────────────────────────────────────────────────────────

class ForeignToplevelManagerV1InterfacePrivate : public QtWaylandServer::zwlr_foreign_toplevel_manager_v1
{
public:
    ForeignToplevelManagerV1InterfacePrivate(ForeignToplevelManagerV1Interface *q, Display *display);

    ForeignToplevelManagerV1Interface *q;
    QList<ForeignToplevelHandleV1Interface *> handles;

protected:
    void zwlr_foreign_toplevel_manager_v1_bind_resource(Resource *resource) override;
    void zwlr_foreign_toplevel_manager_v1_stop(Resource *resource) override;
    void zwlr_foreign_toplevel_manager_v1_destroy(Resource *resource) override;
};

ForeignToplevelManagerV1InterfacePrivate::ForeignToplevelManagerV1InterfacePrivate(
    ForeignToplevelManagerV1Interface *q, Display *display)
    : QtWaylandServer::zwlr_foreign_toplevel_manager_v1(*display, s_managerVersion)
    , q(q)
{
}

void ForeignToplevelManagerV1InterfacePrivate::zwlr_foreign_toplevel_manager_v1_bind_resource(
    Resource *managerResource)
{
    // Advertise all existing handles to the newly connected client
    for (ForeignToplevelHandleV1Interface *handle : std::as_const(handles)) {
        auto *handleResource = handle->d->add(managerResource->client(), managerResource->version());
        send_toplevel(managerResource->handle, handleResource->handle);
        handle->d->sendToResource(handleResource);
    }
}

void ForeignToplevelManagerV1InterfacePrivate::zwlr_foreign_toplevel_manager_v1_stop(
    Resource *resource)
{
    if (resource->version() >= 2) {
        send_finished(resource->handle);
    }
    wl_resource_destroy(resource->handle);
}

void ForeignToplevelManagerV1InterfacePrivate::zwlr_foreign_toplevel_manager_v1_destroy(
    Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

// ─── Manager public ───────────────────────────────────────────────────────────

ForeignToplevelManagerV1Interface::ForeignToplevelManagerV1Interface(Display *display,
                                                                     QObject *parent)
    : QObject(parent)
    , d(std::make_unique<ForeignToplevelManagerV1InterfacePrivate>(this, display))
{
}

ForeignToplevelManagerV1Interface::~ForeignToplevelManagerV1Interface() = default;

ForeignToplevelHandleV1Interface *ForeignToplevelManagerV1Interface::createHandle(Window *window)
{
    auto *handle = new ForeignToplevelHandleV1Interface(this, window);
    handle->setParent(this);
    d->handles << handle;

    connect(handle, &QObject::destroyed, this, [this, handle]() {
        d->handles.removeOne(handle);
    });

    // Advertise the new handle to all currently connected clients
    const auto managerResources = d->resourceMap();
    for (auto *managerResource : managerResources) {
        auto *handleResource = handle->d->add(managerResource->client(), managerResource->version());
        d->send_toplevel(managerResource->handle, handleResource->handle);
        handle->d->sendToResource(handleResource);
    }

    return handle;
}

} // namespace KWin

#include "moc_foreigntoplevelmanagerv1.cpp"
