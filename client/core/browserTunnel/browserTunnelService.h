#pragma once
#include <QObject>
#include <QJsonObject>
#include <QHash>

class BrowserTunnelService : public QObject {
    Q_OBJECT
public:
    explicit BrowserTunnelService(QObject *parent = nullptr);
    QJsonObject start(const QJsonObject &payload);
    QJsonObject stop(const QString &browserSessionId);
    QJsonObject status(const QString &browserSessionId) const;
private:
    struct Session { QString id; QString protocol; QString serverId; quint16 port = 0; };
    QHash<QString, Session> m_sessions;
};