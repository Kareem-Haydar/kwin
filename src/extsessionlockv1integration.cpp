/*
    SPDX-FileCopyrightText: 2025 KWin Authors

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "core/backendoutput.h"
#include "core/output.h"
#include "core/renderloop.h"
#include "extsessionlockv1dbusinterface.h"
#include "extsessionlockv1integration.h"
#include "extsessionlockv1window.h"
#include "wayland/extsessionlock_v1.h"
#include "wayland/output.h"
#include "wayland_server.h"
#include "workspace.h"

#include <QPointer>
#include <QSet>
#include <QTimer>

namespace KWin
{

// Watches per-output render loops and calls sendLocked() once every output has
// presented at least one frame that includes a lock surface.  A 1-second fallback
// timer fires in case an output never presents (e.g. a headless output in tests).
class ExtSessionLockV1Integration::LockPresentationWatcher : public QObject
{
public:
    explicit LockPresentationWatcher(ExtSessionLockV1Interface *lock, QObject *parent = nullptr)
        : QObject(parent)
        , m_lock(lock)
    {
        connect(waylandServer(), &WaylandServer::windowAdded,
                this, &LockPresentationWatcher::handleWindowAdded);

        QTimer::singleShot(1000, this, [this]() {
            sendLockedAndDestroy();
        });
    }

private:
    void handleWindowAdded(Window *window)
    {
        if (!window->isLockScreen()) {
            return;
        }
        connect(window->output()->backendOutput()->renderLoop(), &RenderLoop::framePresented,
                this, [this, windowGuard = QPointer(window)]() {
            if (!windowGuard) {
                return;
            }
            m_signaledOutputs << windowGuard->output();
            if (m_signaledOutputs.size() == workspace()->outputs().size()) {
                sendLockedAndDestroy();
            }
        });
    }

    void sendLockedAndDestroy()
    {
        if (m_lock) {
            m_lock->sendLocked();
        }
        delete this;
    }

    QPointer<ExtSessionLockV1Interface> m_lock;
    QSet<LogicalOutput *> m_signaledOutputs;
};

// --- ExtSessionLockV1Integration ---

ExtSessionLockV1Integration::ExtSessionLockV1Integration(QObject *parent)
    : WaylandShellIntegration(parent)
{
    auto *manager = new ExtSessionLockManagerV1Interface(waylandServer()->display(), this);
    connect(manager, &ExtSessionLockManagerV1Interface::lockRequested,
            this, &ExtSessionLockV1Integration::handleLockRequested);

    new ExtSessionLockV1DBusInterface(this, this);
}

bool ExtSessionLockV1Integration::isLocked() const
{
    return m_lock != nullptr;
}

void ExtSessionLockV1Integration::handleLockRequested(ExtSessionLockV1Interface *lock)
{
    if (m_lock) {
        // A lock is already held; reject this one immediately.
        lock->sendFinished();
        return;
    }

    m_lock = lock;
    Q_EMIT lockStateChanged();
    connect(lock, &ExtSessionLockV1Interface::lockSurfaceRequested,
            this, &ExtSessionLockV1Integration::handleLockSurfaceRequested);
    connect(lock, &ExtSessionLockV1Interface::unlockRequested,
            this, &ExtSessionLockV1Integration::handleUnlockRequested);

    // Start watching for frame presentation.  The watcher lives until it has
    // seen a frame on every output (or the 1-second timeout expires), then
    // self-destructs.
    new LockPresentationWatcher(lock, this);
}

void ExtSessionLockV1Integration::handleLockSurfaceRequested(ExtSessionLockSurfaceV1Interface *surface)
{
    OutputInterface *outputInterface = surface->output();
    if (!outputInterface || outputInterface->isRemoved()) {
        return;
    }
    LogicalOutput *output = outputInterface->handle();
    auto *window = new ExtSessionLockV1Window(surface, output);
    Q_EMIT windowCreated(window);
    window->moveResize(output->geometryF());
}

void ExtSessionLockV1Integration::handleUnlockRequested()
{
    m_lock = nullptr;
    Q_EMIT lockStateChanged();
}

} // namespace KWin

#include "moc_extsessionlockv1integration.cpp"
