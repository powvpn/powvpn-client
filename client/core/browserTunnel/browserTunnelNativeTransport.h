#pragma once
#include <QJsonObject>
class BrowserTunnelNativeTransport final {
public:
    static bool readMessage(QJsonObject *out, QString *error);
    static bool writeMessage(const QJsonObject &message, QString *error);
};