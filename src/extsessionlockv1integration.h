/*
    SPDX-FileCopyrightText: 2025 KWin Authors

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "waylandshellintegration.h"

namespace KWin
{

class ExtSessionLockManagerV1Interface;
class ExtSessionLockV1Interface;
class ExtSessionLockSurfaceV1Interface;

class ExtSessionLockV1Integration : public WaylandShellIntegration
{
    Q_OBJECT

public:
    explicit ExtSessionLockV1Integration(QObject *parent = nullptr);

    bool isLocked() const;

Q_SIGNALS:
    void lockStateChanged();

private:
    void handleLockRequested(ExtSessionLockV1Interface *lock);
    void handleLockSurfaceRequested(ExtSessionLockSurfaceV1Interface *surface);
    void handleUnlockRequested();

    class LockPresentationWatcher;

    ExtSessionLockV1Interface *m_lock = nullptr;
};

} // namespace KWin
