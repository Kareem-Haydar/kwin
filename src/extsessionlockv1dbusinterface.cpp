/*
    SPDX-FileCopyrightText: 2026 KWin Authors

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "extsessionlockv1dbusinterface.h"
#include "core/session.h"
#include "extsessionlockv1integration.h"
#include "main.h"

#include "sessionlockadaptor.h"

#include <QDBusConnection>

namespace KWin
{

ExtSessionLockV1DBusInterface::ExtSessionLockV1DBusInterface(ExtSessionLockV1Integration *integration, QObject *parent)
    : QObject(parent)
    , m_integration(integration)
{
    new SessionLockAdaptor(this);
    QDBusConnection::sessionBus().registerObject(QStringLiteral("/SessionLock"), this);

    connect(integration, &ExtSessionLockV1Integration::lockStateChanged, this, &ExtSessionLockV1DBusInterface::onLockStateChanged);

    connect(kwinApp()->session(), &Session::aboutToSleep, this, &ExtSessionLockV1DBusInterface::prepareSuspend);
    connect(kwinApp()->session(), &Session::awoke, this, &ExtSessionLockV1DBusInterface::awoke);
}

bool ExtSessionLockV1DBusInterface::isLocked() const
{
    return m_integration->isLocked();
}

bool ExtSessionLockV1DBusInterface::isInhibited() const
{
    return !m_inhibitions.isEmpty();
}

int ExtSessionLockV1DBusInterface::gracePeriodMs() const
{
    return s_gracePeriodMs;
}

void ExtSessionLockV1DBusInterface::lock()
{
    if (isInhibited()) {
        return;
    }
    Q_EMIT lockRequested();
}

void ExtSessionLockV1DBusInterface::notifyUserActivity()
{
    // TODO: emit unlockAuthorized() if within grace period
}

uint ExtSessionLockV1DBusInterface::inhibit(const QString &who, const QString &reason)
{
    Q_UNUSED(reason)
    const uint cookie = m_nextCookie++;
    const bool wasInhibited = isInhibited();
    m_inhibitions.insert(cookie, who);
    if (!wasInhibited) {
        Q_EMIT inhibitionChanged(true);
    }
    return cookie;
}

void ExtSessionLockV1DBusInterface::release(uint cookie)
{
    if (!m_inhibitions.remove(cookie)) {
        return;
    }
    if (m_inhibitions.isEmpty()) {
        Q_EMIT inhibitionChanged(false);
    }
}

void ExtSessionLockV1DBusInterface::onLockStateChanged()
{
    const bool locked = m_integration->isLocked();
    if (locked) {
        m_lockTimer.restart();
    }
    Q_EMIT lockStateChanged(locked);
}

} // namespace KWin

#include "moc_extsessionlockv1dbusinterface.cpp"
