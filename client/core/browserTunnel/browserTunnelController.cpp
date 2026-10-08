#include "browserTunnelController.h"
#include "core/managed/managedConnectionAdapter.h"

#include <QHostAddress>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QRandomGenerator>

namespace {
constexpr auto kIpcName = "PowVPN.BrowserTunnel.v1";
QString secret() { return QString::number(QRandomGenerator::global()->generate64(), 16) + QString::number(QRandomGenerator::global()->generate64(), 16); }
QByteArray authChallenge() { return "HTTP/1.1 407 Proxy Authentication Required\r\nProxy-Authenticate: Basic realm=\"PowVPN\"\r\nConnection: close\r\n\r\n"; }
QByteArray badGateway() { return "HTTP/1.1 502 Bad Gateway\r\nConnection: close\r\n\r\n"; }
bool localDestination(const QString &host) {
    const QString h = host.trimmed().toLower();
    return h == "localhost" || h.endsWith(".localhost") || h == "::1" || h.startsWith("127.");
}
}

BrowserTunnelController::BrowserTunnelController(ManagedConnectionAdapter *connection, QObject *parent)
    : QObject(parent), m_connection(connection)
{
    m_pendingTimer.setSingleShot(true);
    connect(&m_pendingTimer, &QTimer::timeout, this, [this] { failPendingStart(QStringLiteral("PowVPN Client did not connect in time")); });
    if (m_connection) {
        connect(m_connection, &ManagedConnectionAdapter::stateChanged, this, &BrowserTunnelController::onManagedStateChanged);
        connect(m_connection, &ManagedConnectionAdapter::connectionFailed, this, [this](const QString &error) { failPendingStart(error); });
    }
}

bool BrowserTunnelController::start()
{
    m_ipc.setSocketOptions(QLocalServer::UserAccessOption);
    if (!m_ipc.listen(kIpcName)) {
        QLocalServer::removeServer(kIpcName);
        if (!m_ipc.listen(kIpcName)) return false;
    }
    connect(&m_ipc, &QLocalServer::newConnection, this, &BrowserTunnelController::onIpcConnection);
    return true;
}

void BrowserTunnelController::onIpcConnection()
{
    while (auto *socket = m_ipc.nextPendingConnection()) {
        connect(socket, &QLocalSocket::readyRead, this, [this, socket] { handleIpc(socket); });
        connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
    }
}

void BrowserTunnelController::sendIpc(QLocalSocket *socket, const QJsonObject &object)
{
    if (!socket) return;
    socket->write(QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n');
    socket->flush();
}

void BrowserTunnelController::handleIpc(QLocalSocket *socket)
{
    const auto request = QJsonDocument::fromJson(socket->readLine(64 * 1024).trimmed()).object();
    const QString command = request.value("command").toString();
    if (command == "status") {
        sendIpc(socket, {{"ok", true}, {"connected", m_connection && m_connection->isConnected()},
                         {"protocol", m_connection ? m_connection->activeProtocol() : QString()}});
        return;
    }
    if (command != "start") { sendIpc(socket, {{"ok", false}, {"error", "Unsupported browser bridge command"}}); return; }
    if (!m_connection) { sendIpc(socket, {{"ok", false}, {"error", "PowVPN Client is unavailable"}}); return; }
    if (m_pendingSocket) { sendIpc(socket, {{"ok", false}, {"error", "A PowVPN Client connection is already being prepared"}}); return; }

    QString country = request.value("country").toString().trimmed().toUpper();
    if (country == QStringLiteral("AUTO")) country.clear();
    if (m_connection->isConnected() && (country.isEmpty() || country == m_connection->activeCountryCode())) {
        completePendingStart();
        ensureProxy();
        sendIpc(socket, {{"ok", true}, {"host", "127.0.0.1"}, {"port", static_cast<int>(m_proxy.serverPort())},
                         {"username", m_user}, {"password", m_password}, {"protocol", m_connection->activeProtocol()}});
        return;
    }

    m_pendingSocket = socket;
    m_pendingCountry = country;
    m_pendingTimer.start(45000);
    m_connection->connectCountry(country, request.value("account_token").toString());
}

void BrowserTunnelController::onManagedStateChanged()
{
    if (!m_pendingSocket || !m_connection) return;
    if (m_connection->isConnected() && (m_pendingCountry.isEmpty() || m_pendingCountry == m_connection->activeCountryCode())) completePendingStart();
}

void BrowserTunnelController::completePendingStart()
{
    if (!m_pendingSocket || !m_connection || !m_connection->isConnected()) return;
    ensureProxy();
    if (!m_proxy.isListening()) { failPendingStart(QStringLiteral("Could not start local browser proxy")); return; }
    m_pendingTimer.stop();
    sendIpc(m_pendingSocket, {{"ok", true}, {"host", "127.0.0.1"}, {"port", static_cast<int>(m_proxy.serverPort())},
                              {"username", m_user}, {"password", m_password}, {"protocol", m_connection->activeProtocol()}});
    m_pendingSocket = nullptr;
    m_pendingCountry.clear();
}

void BrowserTunnelController::failPendingStart(const QString &error)
{
    if (!m_pendingSocket) return;
    m_pendingTimer.stop();
    sendIpc(m_pendingSocket, {{"ok", false}, {"error", error}});
    m_pendingSocket = nullptr;
    m_pendingCountry.clear();
}

void BrowserTunnelController::ensureProxy()
{
    if (m_proxy.isListening()) return;
    m_user = "pow";
    m_password = secret();
    if (m_proxy.listen(QHostAddress::LocalHost, 0)) connect(&m_proxy, &QTcpServer::newConnection, this, &BrowserTunnelController::onProxyConnection);
}

void BrowserTunnelController::onProxyConnection()
{
    while (auto *client = m_proxy.nextPendingConnection()) {
        m_relays.insert(client, Relay{client});
        connect(client, &QTcpSocket::readyRead, this, [this, client] { onClientData(client); });
        connect(client, &QTcpSocket::disconnected, this, [this, client] { closeRelay(client); });
    }
}

void BrowserTunnelController::onClientData(QTcpSocket *client)
{
    auto it = m_relays.find(client); if (it == m_relays.end()) return;
    Relay &relay = it.value();
    if (relay.established) { if (relay.upstream) relay.upstream->write(client->readAll()); return; }
    relay.request += client->readAll();
    const int end = relay.request.indexOf("\r\n\r\n"); if (end < 0) { if (relay.request.size() > 16 * 1024) client->disconnectFromHost(); return; }
    const QList<QByteArray> lines = relay.request.left(end).split('\n');
    const QList<QByteArray> first = lines.value(0).trimmed().split(' ');
    QByteArray supplied;
    for (const auto &line : lines) if (line.trimmed().toLower().startsWith("proxy-authorization:")) supplied = line.trimmed().mid(line.indexOf(':') + 1).trimmed();
    const QByteArray expected = "Basic " + (m_user + ':' + m_password).toUtf8().toBase64();
    if (first.size() < 3 || first[0] != "CONNECT" || supplied != expected) { client->write(authChallenge()); client->disconnectFromHost(); return; }
    const QByteArray authority = first[1]; const int sep = authority.lastIndexOf(':');
    bool validPort = false; const quint16 port = authority.mid(sep + 1).toUShort(&validPort);
    const QString host = QString::fromUtf8(authority.left(sep));
    if (sep <= 0 || !validPort || port == 0 || host.isEmpty() || localDestination(host)) { client->write(badGateway()); client->disconnectFromHost(); return; }
    relay.upstream = new QTcpSocket(this); relay.established = true;
    connect(relay.upstream, &QTcpSocket::connected, this, [client] { client->write("HTTP/1.1 200 Connection Established\r\n\r\n"); });
    connect(relay.upstream, &QTcpSocket::readyRead, client, [client, upstream = relay.upstream] { client->write(upstream->readAll()); });
    connect(relay.upstream, &QTcpSocket::errorOccurred, client, [client] { if (client->state() == QAbstractSocket::ConnectedState) { client->write(badGateway()); client->disconnectFromHost(); } });
    relay.upstream->connectToHost(host, port);
}

void BrowserTunnelController::closeRelay(QTcpSocket *client)
{
    auto it = m_relays.find(client); if (it == m_relays.end()) return;
    if (it->upstream) { it->upstream->disconnectFromHost(); it->upstream->deleteLater(); }
    m_relays.erase(it);
}