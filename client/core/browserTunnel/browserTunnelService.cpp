#include "browserTunnelService.h"
#include <QJsonDocument>
#include <QLocalSocket>

namespace { constexpr auto kBridgeName = "PowVPN.BrowserTunnel.v1"; }
BrowserTunnelService::BrowserTunnelService(QObject *parent) : QObject(parent) {}
QJsonObject BrowserTunnelService::start(const QJsonObject &payload)
{
    const auto sessionId = payload.value("browserSessionId").toString();
    if (sessionId.isEmpty()) return {{"ok", false}, {"error", "Invalid browser tunnel request"}};
    QLocalSocket socket; socket.connectToServer(kBridgeName);
    if (!socket.waitForConnected(2000)) return {{"ok", false}, {"error", "Start Pow VPN Client before using Browser Tunnel"}};
    const QJsonObject location = payload.value("location").toObject();
    const QJsonObject command{{"command", "start"}, {"country", location.value("country").toString()},
                              {"account_token", payload.value("accountToken").toString()}};
    socket.write(QJsonDocument(command).toJson(QJsonDocument::Compact) + '\n'); socket.flush();
    if (!socket.waitForReadyRead(50000)) return {{"ok", false}, {"error", "Pow VPN Client did not connect in time"}};
    const auto answer = QJsonDocument::fromJson(socket.readLine()).object();
    if (!answer.value("ok").toBool()) return answer;
    Session current; current.id = sessionId; current.protocol = answer.value("protocol").toString(); current.port = static_cast<quint16>(answer.value("port").toInt()); m_sessions.insert(sessionId, current);
    return answer;
}
QJsonObject BrowserTunnelService::stop(const QString &browserSessionId) { m_sessions.remove(browserSessionId); return {{"stopped", true}}; }
QJsonObject BrowserTunnelService::status(const QString &browserSessionId) const
{
    // Native hosts are short-lived, so their in-memory session map cannot be
    // the source of truth when Chrome reopens the popup. Ask the running client.
    QLocalSocket socket;
    socket.connectToServer(kBridgeName);
    if (!socket.waitForConnected(1000)) return {{"connected", false}};
    socket.write(QJsonDocument(QJsonObject{{"command", "status"}}).toJson(QJsonDocument::Compact) + '\n');
    socket.flush();
    if (!socket.waitForReadyRead(1500)) return {{"connected", false}};
    const auto answer = QJsonDocument::fromJson(socket.readLine()).object();
    if (!answer.value("ok").toBool()) return {{"connected", false}};
    return {{"connected", answer.value("connected").toBool()},
            {"protocol", answer.value("protocol").toString()},
            {"browserSessionId", browserSessionId}};
}