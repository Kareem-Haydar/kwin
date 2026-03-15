#include "idlemanager.h"

#include <KConfigGroup>
#include <KDirWatch>
#include <QProcess>

namespace KWin
{
IdleManager::IdleManager(QObject *parent)
    : QObject(parent)
    , m_config(QStringLiteral("kwinidlerc"))
{
    m_loginDBus = new QDBusInterface("org.freedesktop.login1", "/org/freedesktop/login1", "org.freedesktop.login1.Manager", QDBusConnection::systemBus(), this);
    refreshListeners();

    KDirWatch::self()->addFile(m_config.name());
    connect(KDirWatch::self(), &KDirWatch::dirty, this, [this](const QString &path) {
        if (path == m_config.name()) {
            m_config.reparseConfiguration();
            refreshListeners();
        }
    });
}

void IdleManager::registerListener(std::chrono::milliseconds timeout, ActionType type)
{
    switch (type) {
    case ActionType::Suspend: {
        auto *detector = new IdleDetector(timeout, IdleDetector::OperatingMode::IgnoresInhibitors, this);
        connect(detector, &IdleDetector::idle, this, [this]() {
            m_loginDBus->call("Suspend", false);
        });
        m_listeners.append(detector);

        break;
    }
    case ActionType::Lock: {
        auto *detector = new IdleDetector(timeout, IdleDetector::OperatingMode::FollowsInhibitors, this);
        connect(detector, &IdleDetector::idle, this, [this]() {
            QProcess::startDetached(m_lockCmd);
        });
        m_listeners.append(detector);

        break;
    }
    }
}

void IdleManager::refreshListeners()
{
    qDeleteAll(m_listeners);
    m_listeners.clear();

    KConfigGroup generalGrp = m_config.group("General");
    m_lockCmd = generalGrp.readEntry("LockCmd");

    for (const auto &group : m_config.groupList()) {
        if (group == "General") {
            continue;
        }

        KConfigGroup configGroup(&m_config, group);
        QString action = configGroup.readEntry("Action");
        int timeout = configGroup.readEntry("Timeout").toInt();
        auto timeoutMs = std::chrono::milliseconds(timeout * 1000);

        if (action == "Suspend") {
            registerListener(timeoutMs, ActionType::Suspend);
        } else if (action == "Lock") {
            registerListener(timeoutMs, ActionType::Lock);
        }
    }
}
}
