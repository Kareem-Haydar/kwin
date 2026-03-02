/*
    SPDX-FileCopyrightText: 2025 KWin Authors

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QObject>

namespace KWin
{

class ExtSessionLockV1Integration;

class ExtSessionLockV1DBusInterface : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.KWin.SessionLock")
    Q_PROPERTY(bool locked READ isLocked NOTIFY lockStateChanged)
    Q_PROPERTY(bool inhibited READ isInhibited NOTIFY inhibitionChanged)
    Q_PROPERTY(int gracePeriodMs READ gracePeriodMs CONSTANT)

public:
    explicit ExtSessionLockV1DBusInterface(ExtSessionLockV1Integration *integration, QObject *parent = nullptr);

    bool isLocked() const;
    bool isInhibited() const;
    int gracePeriodMs() const;

public Q_SLOTS:
    void lock();
    void notifyUserActivity();
    uint inhibit(const QString &who, const QString &reason);
    void release(uint cookie);

Q_SIGNALS:
    void lockRequested();
    void lockStateChanged(bool locked);
    void unlockAuthorized();
    void inhibitionChanged(bool inhibited);
    void prepareSuspend();
    void awoke();

private:
    void onLockStateChanged();

    ExtSessionLockV1Integration *m_integration;
    QElapsedTimer m_lockTimer;
    QHash<uint, QString> m_inhibitions; // cookie → who
    uint m_nextCookie = 1;
    int m_gracePeriodMs = 5000; // TODO: read from config
};

} // namespace KWin
