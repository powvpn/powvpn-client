#include "browserTunnelNativeTransport.h"
#include <QDataStream>
#include <QJsonDocument>
#include <QIODevice>
#include <cstdio>

bool BrowserTunnelNativeTransport::readMessage(QJsonObject *out, QString *error) {
    quint32 size = 0;
    if (std::fread(&size, sizeof(size), 1, stdin) != 1 || size == 0 || size > 1024 * 1024) { if (error) *error = QStringLiteral("Invalid native message length"); return false; }
    QByteArray bytes(static_cast<int>(size), Qt::Uninitialized);
    if (std::fread(bytes.data(), 1, size, stdin) != size) { if (error) *error = QStringLiteral("Truncated native message"); return false; }
    QJsonParseError parse; const auto document = QJsonDocument::fromJson(bytes, &parse);
    if (parse.error != QJsonParseError::NoError || !document.isObject()) { if (error) *error = QStringLiteral("Invalid native JSON"); return false; }
    *out = document.object(); return true;
}
bool BrowserTunnelNativeTransport::writeMessage(const QJsonObject &message, QString *error) {
    const auto bytes = QJsonDocument(message).toJson(QJsonDocument::Compact); const quint32 size = static_cast<quint32>(bytes.size());
    if (std::fwrite(&size, sizeof(size), 1, stdout) != 1 || std::fwrite(bytes.constData(), 1, size, stdout) != size || std::fflush(stdout) != 0) { if (error) *error = QStringLiteral("Failed to write native message"); return false; }
    return true;
}