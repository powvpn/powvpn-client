#pragma once

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QStringList>
#include <QUrl>

class QNetworkReply;

class ManagedApiClient : public QObject
{
    Q_OBJECT
public:
    explicit ManagedApiClient(QObject *parent = nullptr);

    void setBaseUrl(const QUrl &url);
    void setBearerToken(const QString &token);

    void login(const QString &email, const QString &password);

    void registerDevice(const QString &deviceUuid,
                        const QString &platform,
                        const QString &appVersion,
                        const QString &publicKey,
                        const QString &email = {},
                        const QString &accountToken = {});
    void fetchLocations();
    void fetchCountries();
    void fetchSubscription();
    void allocateLocation(int locationId,
                          const QStringList &preferredProtocols,
                          int backups,
                          const QString &networkType = {});
    void allocateCountry(const QString &countryCode,
                         const QStringList &preferredProtocols,
                         int backups,
                         const QString &city = {},
                         const QString &networkType = {});
    void allocateAll(const QStringList &preferredProtocols,
                     const QString &networkType = {});
    void fetchAssignment(int assignmentId);
    void sendTelemetry(const QJsonObject &event);

signals:
    void accountLoginSucceeded(const QString &email, const QString &accountToken);
    void deviceRegistered(int deviceId, int userId, const QString &token, const QString &email);
    void locationsReady(const QJsonArray &locations);
    void countriesReady(const QJsonArray &countries);
    void subscriptionReady(const QJsonObject &subscription);
    void allocationReady(const QJsonObject &allocation);
    void assignmentReady(const QJsonObject &assignment);
    void telemetryAccepted();
    void requestFailed(const QString &operation, int httpStatus, const QString &message);

private:
    enum class Operation {
        Login,
        Register,
        Locations,
        Countries,
        Subscription,
        Allocate,
        Assignment,
        Telemetry
    };

    QNetworkRequest request(const QString &path) const;
    void postJson(Operation op, const QString &path, const QJsonObject &payload);
    void getJson(Operation op, const QString &path);
    void handleReply(Operation op, QNetworkReply *reply);

    QUrl m_baseUrl;
    QString m_token;
    QNetworkAccessManager m_network;
};
