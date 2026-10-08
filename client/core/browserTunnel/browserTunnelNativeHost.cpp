#include "browserTunnelNativeHost.h"
#include <QCoreApplication>
#include <QJsonArray>
#include <QLocalSocket>

BrowserTunnelNativeHost::BrowserTunnelNativeHost(QObject *parent) : QObject(parent) {}
QJsonObject BrowserTunnelNativeHost::response(const QString &id, bool ok, const QJsonObject &result, const QString &error) const {
    QJsonObject out{{"id", id}, {"ok", ok}};
    if (ok) out.insert("result", result); else out.insert("error", error);
    return out;
}
QJsonObject BrowserTunnelNativeHost::handle(const QJsonObject &request) {
    const auto id = request.value("id").toString();
    const auto command = request.value("command").toString();
    if (id.isEmpty() || command.isEmpty()) return response(id, false, {}, QStringLiteral("Invalid native message"));
    if (command == QStringLiteral("hello")) {
        // The native host is a separate process from the GUI client, so report
        // availability based on whether the client's browser-tunnel bridge is
        // actually reachable -- otherwise the extension cannot tell the user to
        // start Pow VPN Client.
        QLocalSocket probe;
        probe.connectToServer(QStringLiteral("PowVPN.BrowserTunnel.v1"));
        const bool clientRunning = probe.waitForConnected(400);
        probe.abort();
        return response(id, true, {{"available", clientRunning}, {"version", QCoreApplication::applicationVersion()},
                                   {"browserTunnelSupported", clientRunning},
                                   {"protocols", QJsonArray{"awg31", "wireguard", "vless-reality"}}});
    }
    if (command == QStringLiteral("startBrowserTunnel")) { const auto result = m_service.start(request.value("payload").toObject()); return response(id, result.value("ok").toBool(), result, result.value("error").toString()); }
    if (command == QStringLiteral("stopBrowserTunnel")) return response(id, true, m_service.stop(request.value("payload").toObject().value("browserSessionId").toString()));
    if (command == QStringLiteral("getBrowserTunnelStatus")) return response(id, true, m_service.status(request.value("payload").toObject().value("browserSessionId").toString()));
    if (command == QStringLiteral("ping")) return response(id, true, {});
    return response(id, false, {}, QStringLiteral("Unsupported native command"));
}
