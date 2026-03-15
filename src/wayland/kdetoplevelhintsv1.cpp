/*
    SPDX-FileCopyrightText: 2026 KWin Authors

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "kdetoplevelhintsv1.h"
#include "display.h"
#include "utils/resource.h"
#include "xdgshell.h"
#include "xdgshell_p.h"

#include "qwayland-server-kde-toplevel-hints-v1.h"

#include <QPointer>

namespace KWin
{

static constexpr uint32_t s_version = 1;

// Per-toplevel hints object. Lives until the client destroys it.
class KdeToplevelHintsV1 : public QtWaylandServer::kde_toplevel_hints_v1
{
public:
    KdeToplevelHintsV1(XdgToplevelInterface *toplevel, ::wl_resource *resource)
        : kde_toplevel_hints_v1(resource)
        , m_toplevel(toplevel)
    {
        XdgToplevelInterfacePrivate::get(toplevel)->hasHints = true;
    }

protected:
    void kde_toplevel_hints_v1_destroy(Resource *resource) override
    {
        wl_resource_destroy(resource->handle);
    }

    void kde_toplevel_hints_v1_destroy_resource(Resource *) override
    {
        if (auto *priv = privateOrNull()) {
            if (priv->skipTaskbar) {
                priv->skipTaskbar = false;
                Q_EMIT priv->q->skipTaskbarChanged();
            }
            if (priv->skipSwitcher) {
                priv->skipSwitcher = false;
                Q_EMIT priv->q->skipSwitcherChanged();
            }
            priv->hasHints = false;
        }
        delete this;
    }

    void kde_toplevel_hints_v1_set_skip_taskbar(Resource *, uint32_t skip) override
    {
        auto *priv = privateOrNull();
        if (!priv) {
            return;
        }
        const bool value = skip != 0;
        if (priv->skipTaskbar == value) {
            return;
        }
        priv->skipTaskbar = value;
        Q_EMIT priv->q->skipTaskbarChanged();
    }

    void kde_toplevel_hints_v1_set_skip_switcher(Resource *, uint32_t skip) override
    {
        auto *priv = privateOrNull();
        if (!priv) {
            return;
        }
        const bool value = skip != 0;
        if (priv->skipSwitcher == value) {
            return;
        }
        priv->skipSwitcher = value;
        Q_EMIT priv->q->skipSwitcherChanged();
    }

private:
    XdgToplevelInterfacePrivate *privateOrNull() const
    {
        return m_toplevel ? XdgToplevelInterfacePrivate::get(m_toplevel) : nullptr;
    }

    QPointer<XdgToplevelInterface> m_toplevel;
};

class KdeToplevelHintsManagerV1Private : public QtWaylandServer::kde_toplevel_hints_manager_v1
{
public:
    explicit KdeToplevelHintsManagerV1Private(Display *display)
        : kde_toplevel_hints_manager_v1(*display, s_version)
    {
    }

protected:
    void kde_toplevel_hints_manager_v1_destroy(Resource *resource) override
    {
        wl_resource_destroy(resource->handle);
    }

    void kde_toplevel_hints_manager_v1_get_hints(Resource *resource, uint32_t id, ::wl_resource *toplevel) override
    {
        XdgToplevelInterfacePrivate *priv = XdgToplevelInterfacePrivate::get(toplevel);
        if (!priv) {
            return;
        }

        if (priv->hasHints) {
            wl_resource_post_error(resource->handle, error_already_constructed, "a hints object already exists for this toplevel");
            return;
        }

        wl_resource *hintsResource = wl_resource_create(resource->client(), &kde_toplevel_hints_v1_interface, resource->version(), id);
        if (!hintsResource) {
            wl_resource_post_no_memory(resource->handle);
            return;
        }

        new KdeToplevelHintsV1(priv->q, hintsResource);
    }
};

KdeToplevelHintsManagerV1::KdeToplevelHintsManagerV1(Display *display, QObject *parent)
    : QObject(parent)
    , d(std::make_unique<KdeToplevelHintsManagerV1Private>(display))
{
}

KdeToplevelHintsManagerV1::~KdeToplevelHintsManagerV1() = default;

} // namespace KWin
