#include <QCoreApplication>
#include <QJsonObject>
#include "browserTunnelNativeHost.h"
#include "browserTunnelNativeTransport.h"
int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv); app.setApplicationVersion(QStringLiteral("1.0.0"));
    BrowserTunnelNativeHost host;
    for (;;) { QJsonObject request; QString error; if (!BrowserTunnelNativeTransport::readMessage(&request, &error)) return 0; if (!BrowserTunnelNativeTransport::writeMessage(host.handle(request), &error)) return 1; }
}