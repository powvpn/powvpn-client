#pragma once

#include <QObject>
#include <QHash>
#include <QJsonObject>
#include <QLocalServer>
#include <QPointer>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

class ManagedConnectionAdapter;
class QLocalSocket;

class BrowserTunnelController final : public QObject {
    Q_OBJECT
public:
    explicit BrowserTunnelController(ManagedConnectionAdapter *connection, QObject *parent = nullptr);
    bool start();
private:
    struct Relay { QTcpSocket *client = nullptr; QTcpSocket *upstream = nullptr; QByteArray request; bool established = false; };
    void onIpcConnection();
    void handleIpc(QLocalSocket *socket);
    void onManagedStateChanged();
    void completePendingStart();
    void failPendingStart(const QString &error);
    void ensureProxy();
    void onProxyConnection();
    void onClientData(QTcpSocket *client);
    void closeRelay(QTcpSocket *client);
    void sendIpc(QLocalSocket *socket, const QJsonObject &object);

    ManagedConnectionAdapter *m_connection = nullptr;
    QLocalServer m_ipc;
    QTcpServer m_proxy;
    QHash<QTcpSocket *, Relay> m_relays;
    QString m_user;
    QString m_password;
    QPointer<QLocalSocket> m_pendingSocket;
    QString m_pendingCountry;
    QTimer m_pendingTimer;
};