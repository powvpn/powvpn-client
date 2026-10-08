#include "managedApiClient.h"

#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>

ManagedApiClient::ManagedApiClient(QObject *parent)
    : QObject(parent)
{
}

void ManagedApiClient::setBaseUrl(const QUrl &url) { m_baseUrl = url; }
void ManagedApiClient::setBearerToken(const QString &token) { m_token = token; }

QNetworkRequest ManagedApiClient::request(const QString &path) const
{
    QUrl url = m_baseUrl;
    QString base = url.path();
    if (base.endsWith('/')) base.chop(1);
    url.setPath(base + path);
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Accept", "application/json");
    if (!m_token.isEmpty()) req.setRawHeader("Authorization", QByteArray("Bearer ") + m_token.toUtf8());
    return req;
}

void ManagedApiClient::postJson(Operation op, const QString &path, const QJsonObject &payload)
{
    auto *reply = m_network.post(request(path), QJsonDocument(payload).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, op, reply] { handleReply(op, reply); });
}

void ManagedApiClient::getJson(Operation op, const QString &path)
{
    auto *reply = m_network.get(request(path));
    connect(reply, &QNetworkReply::finished, this, [this, op, reply] { handleReply(op, reply); });
}

void ManagedApiClient::login(const QString &email, const QString &password)
{
    postJson(Operation::Login, "/api/v1/web/login", {{"email", email.trimmed()}, {"password", password}});
}

void ManagedApiClient::registerDevice(const QString &deviceUuid,
                                      const QString &platform,
                                      const QString &appVersion,
                                      const QString &publicKey,
                                      const QString &email,
                                      const QString &accountToken)
{
    QJsonObject o{{"device_uuid", deviceUuid}, {"platform", platform}, {"app_version", appVersion}};
    if (!publicKey.isEmpty()) o.insert("public_key", publicKey);
    if (!email.isEmpty()) o.insert("email", email);
    if (!accountToken.isEmpty()) o.insert("account_token", accountToken);
    postJson(Operation::Register, "/api/v1/device/register", o);
}

void ManagedApiClient::fetchLocations() { getJson(Operation::Locations, "/api/v1/client/locations"); }
void ManagedApiClient::fetchCountries() { getJson(Operation::Countries, "/api/v1/client/countries"); }
void ManagedApiClient::fetchSubscription() { getJson(Operation::Subscription, "/api/v1/client/subscription"); }

void ManagedApiClient::allocateLocation(int locationId, const QStringList &preferredProtocols, int backups, const QString &networkType)
{
    QJsonArray protocols; for (const auto &p : preferredProtocols) protocols.append(p);
    QJsonObject o{{"location_id", locationId}, {"preferred_protocols", protocols}, {"backups", backups}};
    if (!networkType.isEmpty()) o.insert("network_type", networkType);
    postJson(Operation::Allocate, "/api/v1/client/allocate", o);
}

void ManagedApiClient::allocateCountry(const QString &countryCode, const QStringList &preferredProtocols, int backups,
                                       const QString &city, const QString &networkType)
{
    QJsonArray protocols; for (const auto &p : preferredProtocols) protocols.append(p);
    QJsonObject o{{"country_code", countryCode.toUpper()}, {"preferred_protocols", protocols}, {"backups", backups}};
    if (!city.isEmpty()) o.insert("city", city);
    if (!networkType.isEmpty()) o.insert("network_type", networkType);
    postJson(Operation::Allocate, "/api/v1/client/allocate", o);
}

void ManagedApiClient::allocateAll(const QStringList &preferredProtocols, const QString &networkType)
{
    QJsonArray protocols; for (const auto &p : preferredProtocols) protocols.append(p);
    QJsonObject o{{"all_locations", true}, {"preferred_protocols", protocols}};
    if (!networkType.isEmpty()) o.insert("network_type", networkType);
    postJson(Operation::Allocate, "/api/v1/client/allocate", o);
}

void ManagedApiClient::fetchAssignment(int assignmentId)
{
    getJson(Operation::Assignment, QString("/api/v1/client/assignment/%1").arg(assignmentId));
}

void ManagedApiClient::sendTelemetry(const QJsonObject &event)
{
    postJson(Operation::Telemetry, "/api/v1/client/telemetry", event);
}

void ManagedApiClient::handleReply(Operation op, QNetworkReply *reply)
{
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray body = reply->readAll();
    reply->deleteLater();
    QJsonParseError err{};
    const auto doc = QJsonDocument::fromJson(body, &err);
    if (status < 200 || status >= 300 || err.error != QJsonParseError::NoError) {
        emit requestFailed(op == Operation::Login ? QStringLiteral("login") : QString::number(static_cast<int>(op)), status,
                           QString::fromUtf8(body.isEmpty() ? QByteArray("request failed") : body));
        return;
    }
    switch (op) {
    case Operation::Login: {
        const auto o = doc.object();
        emit accountLoginSucceeded(o.value("email").toString(), o.value("token").toString());
        break;
    }
    case Operation::Register: {
        const auto o = doc.object();
        emit deviceRegistered(o.value("device_id").toInt(), o.value("user_id").toInt(), o.value("token").toString(), o.value("email").toString());
        break;
    }
    case Operation::Locations: emit locationsReady(doc.array()); break;
    case Operation::Countries: emit countriesReady(doc.array()); break;
    case Operation::Subscription: emit subscriptionReady(doc.object()); break;
    case Operation::Allocate: emit allocationReady(doc.object()); break;
    case Operation::Assignment: emit assignmentReady(doc.object()); break;
    case Operation::Telemetry: emit telemetryAccepted(); break;
    }
}
