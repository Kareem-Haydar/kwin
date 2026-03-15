#pragma once

#include <KConfig>
#include <QDBusInterface>

#include "idledetector.h"

namespace KWin
{
class IdleManager : public QObject
{
    Q_OBJECT

public:
    enum class ActionType {
        Lock,
        Suspend,
        // Dim, TODO: implement later
    };

    explicit IdleManager(QObject *parent = nullptr);

private:
    void refreshListeners();
    void registerListener(std::chrono::milliseconds timeout, ActionType type);

    QString m_lockCmd;
    QDBusInterface *m_loginDBus;
    QList<IdleDetector *> m_listeners;
    KConfig m_config;
};
}
