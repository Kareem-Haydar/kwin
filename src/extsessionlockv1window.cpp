/*
    SPDX-FileCopyrightText: 2025 KWin Authors

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "extsessionlockv1window.h"
#include "wayland/surface.h"
#include "wayland/display.h"
#include "wayland_server.h"
#include "workspace.h"

namespace KWin
{

ExtSessionLockV1Window::ExtSessionLockV1Window(ExtSessionLockSurfaceV1Interface *surface, LogicalOutput *output)
    : WaylandWindow(surface->surface())
    , m_surface(surface)
{
    setOutput(output);
    setMoveResizeOutput(output);
    setSkipSwitcher(true);
    setSkipPager(true);
    setSkipTaskbar(true);

    connect(surface, &ExtSessionLockSurfaceV1Interface::aboutToBeDestroyed, this, &ExtSessionLockV1Window::handleDestroy);
    connect(surface->surface(), &SurfaceInterface::aboutToBeDestroyed, this, &ExtSessionLockV1Window::handleDestroy);
    connect(surface->surface(), &SurfaceInterface::committed, this, &ExtSessionLockV1Window::handleCommit);
    connect(surface->surface(), &SurfaceInterface::sizeChanged, this, &ExtSessionLockV1Window::handleSizeChanged);
    connect(surface, &ExtSessionLockSurfaceV1Interface::configureAcknowledged, this, &ExtSessionLockV1Window::handleConfigure);
}

WindowType ExtSessionLockV1Window::windowType() const
{
    return WindowType::Override;
}

bool ExtSessionLockV1Window::isLockScreen() const
{
    return true;
}

bool ExtSessionLockV1Window::isPlaceable() const
{
    return false;
}

bool ExtSessionLockV1Window::isCloseable() const
{
    return false;
}

bool ExtSessionLockV1Window::isMovable() const
{
    return false;
}

bool ExtSessionLockV1Window::isMovableAcrossScreens() const
{
    return false;
}

bool ExtSessionLockV1Window::isResizable() const
{
    return false;
}

bool ExtSessionLockV1Window::wantsInput() const
{
    return acceptsFocus() && readyForPainting();
}

void ExtSessionLockV1Window::destroyWindow()
{
    m_surface->disconnect(this);
    m_surface->surface()->disconnect(this);

    markAsDeleted();
    Q_EMIT closed();

    cleanTabBox();
    StackingUpdatesBlocker blocker(workspace());
    cleanGrouping();
    waylandServer()->removeWindow(this);
    unref();
}

Layer ExtSessionLockV1Window::belongsToLayer() const
{
    return OverlayLayer;
}

bool ExtSessionLockV1Window::acceptsFocus() const
{
    return !isDeleted();
}

void ExtSessionLockV1Window::moveResizeInternal(const RectF &rect, MoveResizeMode mode)
{
    const QSize requestedClientSize = nextFrameSizeToClientSize(rect.size()).toSize();

    if (!m_configureEvents.isEmpty()) {
        const ExtSessionLockV1ConfigureEvent &last = m_configureEvents.constLast();
        if (last.size != requestedClientSize) {
            const quint32 serial = waylandServer()->display()->nextSerial();
            m_surface->sendConfigure(serial, requestedClientSize.width(), requestedClientSize.height());
            m_configureEvents.append({serial, requestedClientSize});
        }
    } else if (requestedClientSize != clientSize()) {
        const quint32 serial = waylandServer()->display()->nextSerial();
        m_surface->sendConfigure(serial, requestedClientSize.width(), requestedClientSize.height());
        m_configureEvents.append({serial, requestedClientSize});
    } else {
        updateGeometry(rect);
        return;
    }

    RectF updateRect = m_frameGeometry;
    updateRect.moveTopLeft(rect.topLeft());
    updateGeometry(updateRect);
}

void ExtSessionLockV1Window::doSetNextTargetScale()
{
    if (isDeleted()) {
        return;
    }
    surface()->setPreferredBufferScale(nextTargetScale());
    setTargetScale(nextTargetScale());
}

void ExtSessionLockV1Window::doSetPreferredBufferTransform()
{
    if (isDeleted()) {
        return;
    }
    surface()->setPreferredBufferTransform(preferredBufferTransform());
}

void ExtSessionLockV1Window::doSetPreferredColorDescription()
{
    if (isDeleted()) {
        return;
    }
    surface()->setPreferredColorDescription(preferredColorDescription());
}

void ExtSessionLockV1Window::handleConfigure(quint32 serial)
{
    while (!m_configureEvents.isEmpty()) {
        const ExtSessionLockV1ConfigureEvent head = m_configureEvents.takeFirst();
        if (head.serial == serial) {
            break;
        }
    }
}

void ExtSessionLockV1Window::handleDestroy()
{
    destroyWindow();
}

void ExtSessionLockV1Window::handleCommit()
{
    if (surface()->buffer()) {
        markAsMapped();
    }
}

void ExtSessionLockV1Window::handleSizeChanged()
{
    updateGeometry(RectF(pos(), clientSizeToFrameSize(surface()->size())));
}

} // namespace KWin

#include "moc_extsessionlockv1window.cpp"
