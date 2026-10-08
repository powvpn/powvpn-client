#pragma once
#include <QObject>
#include <QJsonObject>
#include "browserTunnelService.h"

class BrowserTunnelNativeHost final : public QObject {
    Q_OBJECT
public:
    explicit BrowserTunnelNativeHost(QObject *parent = nullptr);
    QJsonObject handle(const QJsonObject &request);
private:
    BrowserTunnelService m_service;
    QJsonObject response(const QString &id, bool ok, const QJsonObject &result = {}, const QString &error = {}) const;
};