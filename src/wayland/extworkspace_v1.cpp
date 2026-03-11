/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 KWin Authors

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "extworkspace_v1.h"
#include "display.h"
#include "output.h"

#include "qwayland-server-ext-workspace-v1.h"

namespace KWin
{

static const quint32 s_managerVersion = 1;

// ─── Handle private ───────────────────────────────────────────────────────────

class ExtWorkspaceHandleV1InterfacePrivate : public QtWaylandServer::ext_workspace_handle_v1
{
public:
    ExtWorkspaceHandleV1InterfacePrivate(ExtWorkspaceHandleV1Interface *q,
                                         ExtWorkspaceManagerV1Interface *manager);

    void sendToResource(Resource *resource);

    ExtWorkspaceHandleV1Interface *q;
    ExtWorkspaceManagerV1Interface *manager;

protected:
    void ext_workspace_handle_v1_destroy(Resource *resource) override;
    void ext_workspace_handle_v1_activate(Resource *resource) override;
    void ext_workspace_handle_v1_deactivate(Resource *resource) override;
    void ext_workspace_handle_v1_assign(Resource *resource, wl_resource *workspace_group) override;
    void ext_workspace_handle_v1_remove(Resource *resource) override;
};

ExtWorkspaceHandleV1InterfacePrivate::ExtWorkspaceHandleV1InterfacePrivate(
    ExtWorkspaceHandleV1Interface *q, ExtWorkspaceManagerV1Interface *manager)
    : q(q)
    , manager(manager)
{
}

void ExtWorkspaceHandleV1InterfacePrivate::sendToResource(Resource *resource)
{
    if (!q->m_id.isEmpty()) {
        send_id(resource->handle, q->m_id);
    }
    if (!q->m_name.isEmpty()) {
        send_name(resource->handle, q->m_name);
    }
    if (!q->m_coords.isEmpty()) {
        QByteArray coordData(reinterpret_cast<const char *>(q->m_coords.constData()),
                             q->m_coords.size() * sizeof(uint32_t));
        send_coordinates(resource->handle, coordData);
    }
    send_state(resource->handle, q->m_state);
    send_capabilities(resource->handle,
                      workspace_capabilities_activate
                          | workspace_capabilities_deactivate
                          | workspace_capabilities_remove);
}

void ExtWorkspaceHandleV1InterfacePrivate::ext_workspace_handle_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void ExtWorkspaceHandleV1InterfacePrivate::ext_workspace_handle_v1_activate(Resource *)
{
    Q_EMIT q->activateRequested();
}

void ExtWorkspaceHandleV1InterfacePrivate::ext_workspace_handle_v1_deactivate(Resource *)
{
    Q_EMIT q->deactivateRequested();
}

void ExtWorkspaceHandleV1InterfacePrivate::ext_workspace_handle_v1_assign(Resource *,
                                                                          wl_resource *)
{
    // Single-group compositor — assigning to the only group is a no-op.
}

void ExtWorkspaceHandleV1InterfacePrivate::ext_workspace_handle_v1_remove(Resource *)
{
    Q_EMIT q->removeRequested();
}

// ─── Handle public ────────────────────────────────────────────────────────────

ExtWorkspaceHandleV1Interface::ExtWorkspaceHandleV1Interface(ExtWorkspaceManagerV1Interface *manager)
    : d(std::make_unique<ExtWorkspaceHandleV1InterfacePrivate>(this, manager))
{
}

ExtWorkspaceHandleV1Interface::~ExtWorkspaceHandleV1Interface() = default;

void ExtWorkspaceHandleV1Interface::setName(const QString &name)
{
    if (m_name == name) {
        return;
    }
    m_name = name;
    const auto resources = d->resourceMap();
    for (auto *resource : resources) {
        d->send_name(resource->handle, name);
    }
}

void ExtWorkspaceHandleV1Interface::setId(const QString &id)
{
    if (m_id == id) {
        return;
    }
    m_id = id;
    const auto resources = d->resourceMap();
    for (auto *resource : resources) {
        d->send_id(resource->handle, id);
    }
}

void ExtWorkspaceHandleV1Interface::setCoordinates(const QList<uint32_t> &coords)
{
    if (m_coords == coords) {
        return;
    }
    m_coords = coords;
    QByteArray coordData(reinterpret_cast<const char *>(coords.constData()),
                         coords.size() * sizeof(uint32_t));
    const auto resources = d->resourceMap();
    for (auto *resource : resources) {
        d->send_coordinates(resource->handle, coordData);
    }
}

void ExtWorkspaceHandleV1Interface::setState(uint32_t state)
{
    if (m_state == state) {
        return;
    }
    m_state = state;
    const auto resources = d->resourceMap();
    for (auto *resource : resources) {
        d->send_state(resource->handle, state);
    }
}

// ─── Group private ────────────────────────────────────────────────────────────

class ExtWorkspaceGroupHandleV1InterfacePrivate : public QtWaylandServer::ext_workspace_group_handle_v1
{
public:
    ExtWorkspaceGroupHandleV1InterfacePrivate(ExtWorkspaceGroupHandleV1Interface *q,
                                              ExtWorkspaceManagerV1Interface *manager);

    void sendToResource(Resource *resource, const QList<ExtWorkspaceHandleV1Interface *> &handles);

    ExtWorkspaceGroupHandleV1Interface *q;
    ExtWorkspaceManagerV1Interface *manager;

protected:
    void ext_workspace_group_handle_v1_create_workspace(Resource *resource,
                                                        const QString &workspace) override;
    void ext_workspace_group_handle_v1_destroy(Resource *resource) override;
};

ExtWorkspaceGroupHandleV1InterfacePrivate::ExtWorkspaceGroupHandleV1InterfacePrivate(
    ExtWorkspaceGroupHandleV1Interface *q, ExtWorkspaceManagerV1Interface *manager)
    : q(q)
    , manager(manager)
{
}

void ExtWorkspaceGroupHandleV1InterfacePrivate::sendToResource(
    Resource *resource, const QList<ExtWorkspaceHandleV1Interface *> &handles)
{
    send_capabilities(resource->handle, group_capabilities_create_workspace);

    for (OutputInterface *output : std::as_const(q->m_outputs)) {
        if (auto *outputRes = output->clientResources(resource->client()).value(0)) {
            send_output_enter(resource->handle, outputRes);
        }
    }

    for (ExtWorkspaceHandleV1Interface *handle : handles) {
        auto *wsRes = handle->d->resourceMap().value(resource->client());
        if (wsRes) {
            send_workspace_enter(resource->handle, wsRes->handle);
        }
    }
}

void ExtWorkspaceGroupHandleV1InterfacePrivate::ext_workspace_group_handle_v1_create_workspace(
    Resource *, const QString &name)
{
    Q_EMIT q->createWorkspaceRequested(name);
}

void ExtWorkspaceGroupHandleV1InterfacePrivate::ext_workspace_group_handle_v1_destroy(
    Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

// ─── Group public ─────────────────────────────────────────────────────────────

ExtWorkspaceGroupHandleV1Interface::ExtWorkspaceGroupHandleV1Interface(
    ExtWorkspaceManagerV1Interface *manager)
    : d(std::make_unique<ExtWorkspaceGroupHandleV1InterfacePrivate>(this, manager))
{
}

ExtWorkspaceGroupHandleV1Interface::~ExtWorkspaceGroupHandleV1Interface() = default;

void ExtWorkspaceGroupHandleV1Interface::outputAdded(OutputInterface *output)
{
    m_outputs << output;
    const auto groupResources = d->resourceMap();
    for (auto *groupRes : groupResources) {
        if (auto *outputRes = output->clientResources(groupRes->client()).value(0)) {
            d->send_output_enter(groupRes->handle, outputRes);
        }
    }
}

void ExtWorkspaceGroupHandleV1Interface::outputRemoved(OutputInterface *output)
{
    m_outputs.removeOne(output);
    const auto groupResources = d->resourceMap();
    for (auto *groupRes : groupResources) {
        if (auto *outputRes = output->clientResources(groupRes->client()).value(0)) {
            d->send_output_leave(groupRes->handle, outputRes);
        }
    }
}

// ─── Manager private ──────────────────────────────────────────────────────────

class ExtWorkspaceManagerV1InterfacePrivate : public QtWaylandServer::ext_workspace_manager_v1
{
public:
    ExtWorkspaceManagerV1InterfacePrivate(ExtWorkspaceManagerV1Interface *q, Display *display);

    ExtWorkspaceManagerV1Interface *q;

protected:
    void ext_workspace_manager_v1_bind_resource(Resource *resource) override;
    void ext_workspace_manager_v1_commit(Resource *resource) override;
    void ext_workspace_manager_v1_stop(Resource *resource) override;
};

ExtWorkspaceManagerV1InterfacePrivate::ExtWorkspaceManagerV1InterfacePrivate(
    ExtWorkspaceManagerV1Interface *q, Display *display)
    : QtWaylandServer::ext_workspace_manager_v1(*display, s_managerVersion)
    , q(q)
{
}

void ExtWorkspaceManagerV1InterfacePrivate::ext_workspace_manager_v1_bind_resource(
    Resource *managerRes)
{
    // 1. Create workspace resources and send initial state for each.
    //    Workspace resources must exist before workspace_enter can reference them.
    for (ExtWorkspaceHandleV1Interface *handle : std::as_const(q->m_handles)) {
        auto *wsRes = handle->d->add(managerRes->client(), managerRes->version());
        send_workspace(managerRes->handle, wsRes->handle);
        handle->d->sendToResource(wsRes);
    }

    // 2. Create the group resource, send the workspace_group event, then group state.
    auto *groupRes = q->m_group->d->add(managerRes->client(), managerRes->version());
    send_workspace_group(managerRes->handle, groupRes->handle);
    q->m_group->d->sendToResource(groupRes, q->m_handles);

    // 3. Signal that the initial state dump is complete.
    send_done(managerRes->handle);
}

void ExtWorkspaceManagerV1InterfacePrivate::ext_workspace_manager_v1_commit(Resource *)
{
    // Requests are processed immediately; nothing to do here.
}

void ExtWorkspaceManagerV1InterfacePrivate::ext_workspace_manager_v1_stop(Resource *resource)
{
    // finished is a destructor event — it destroys the resource after sending.
    send_finished(resource->handle);
}

// ─── Manager public ───────────────────────────────────────────────────────────

ExtWorkspaceManagerV1Interface::ExtWorkspaceManagerV1Interface(Display *display, QObject *parent)
    : QObject(parent)
    , d(std::make_unique<ExtWorkspaceManagerV1InterfacePrivate>(this, display))
{
    m_group = new ExtWorkspaceGroupHandleV1Interface(this);
    m_group->setParent(this);
}

ExtWorkspaceManagerV1Interface::~ExtWorkspaceManagerV1Interface() = default;

ExtWorkspaceHandleV1Interface *ExtWorkspaceManagerV1Interface::createWorkspace()
{
    auto *handle = new ExtWorkspaceHandleV1Interface(this);
    handle->setParent(this);
    m_handles << handle;

    connect(handle, &QObject::destroyed, this, [this, handle]() {
        m_handles.removeOne(handle);
    });

    // Advertise the new workspace to all connected clients.
    const auto managerResources = d->resourceMap();
    for (auto *managerRes : managerResources) {
        auto *wsRes = handle->d->add(managerRes->client(), managerRes->version());
        d->send_workspace(managerRes->handle, wsRes->handle);
        handle->d->sendToResource(wsRes);

        // Inform the group that this workspace now belongs to it.
        auto *groupRes = m_group->d->resourceMap().value(managerRes->client());
        if (groupRes) {
            m_group->d->send_workspace_enter(groupRes->handle, wsRes->handle);
        }
    }

    return handle;
}

void ExtWorkspaceManagerV1Interface::removeWorkspace(ExtWorkspaceHandleV1Interface *handle)
{
    const auto managerResources = d->resourceMap();
    for (auto *managerRes : managerResources) {
        auto *wsRes = handle->d->resourceMap().value(managerRes->client());
        auto *groupRes = m_group->d->resourceMap().value(managerRes->client());

        // Protocol requires workspace_leave before workspace.removed.
        if (groupRes && wsRes) {
            m_group->d->send_workspace_leave(groupRes->handle, wsRes->handle);
        }
        if (wsRes) {
            handle->d->send_removed(wsRes->handle);
        }
    }

    handle->deleteLater();
}

void ExtWorkspaceManagerV1Interface::outputAdded(OutputInterface *output)
{
    m_group->outputAdded(output);
}

void ExtWorkspaceManagerV1Interface::outputRemoved(OutputInterface *output)
{
    m_group->outputRemoved(output);
}

void ExtWorkspaceManagerV1Interface::done()
{
    const auto managerResources = d->resourceMap();
    for (auto *managerRes : managerResources) {
        d->send_done(managerRes->handle);
    }
}

} // namespace KWin

#include "moc_extworkspace_v1.cpp"
