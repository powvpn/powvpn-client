#pragma once

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QVariantList>

#include "managedApiClient.h"

class ManagedServiceController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList countries READ countries NOTIFY countriesChanged)
    Q_PROPERTY(QVariantList locations READ locations NOTIFY locationsChanged)
    Q_PROPERTY(QString subscriptionPlan READ subscriptionPlan NOTIFY subscriptionChanged)
    Q_PROPERTY(QString accessMode READ accessMode NOTIFY subscriptionChanged)
    Q_PROPERTY(bool accessAllowed READ accessAllowed NOTIFY subscriptionChanged)
    Q_PROPERTY(qint64 trafficUsedBytes READ trafficUsedBytes NOTIFY subscriptionChanged)
    Q_PROPERTY(qint64 trafficLimitBytes READ trafficLimitBytes NOTIFY subscriptionChanged)
    Q_PROPERTY(qint64 trafficRemainingBytes READ trafficRemainingBytes NOTIFY subscriptionChanged)
    Q_PROPERTY(QString geoTier READ geoTier NOTIFY subscriptionChanged)
    Q_PROPERTY(bool trialAvailable READ trialAvailable NOTIFY subscriptionChanged)
    Q_PROPERTY(bool registered READ registered NOTIFY registeredChanged)
    Q_PROPERTY(QString accountEmail READ accountEmail NOTIFY accountEmailChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    explicit ManagedServiceController(QObject *parent = nullptr);
    void setApiBaseUrl(const QUrl &url);
    void setDevicePublicKey(const QString &publicKey);
    void setPlatformInfo(const QString &platform, const QString &appVersion);

    QVariantList countries() const { return m_countries; }
    QVariantList locations() const { return m_locations; }
    QString subscriptionPlan() const { return m_subscriptionPlan; }
    QString accessMode() const { return m_accessMode; }
    bool accessAllowed() const { return m_accessAllowed; }
    qint64 trafficUsedBytes() const { return m_trafficUsedBytes; }
    qint64 trafficLimitBytes() const { return m_trafficLimitBytes; }
    qint64 trafficRemainingBytes() const { return m_trafficRemainingBytes; }
    QString geoTier() const { return m_geoTier; }
    bool trialAvailable() const { return m_trialAvailable; }
    bool registered() const { return !m_token.isEmpty(); }
    QString accountEmail() const { return m_accountEmail; }
    bool busy() const { return m_busy; }
    QString lastError() const { return m_lastError; }

    // Base64 WireGuard/AmneziaWG private key generated once on this device and
    // never sent to the control plane (only its public counterpart is, via
    // registerDevice()). Used by ManagedConnectionAdapter to build the
    // client-side [Interface] section for AWG candidates.
    QString devicePrivateKey() const { return m_privateKey; }

    Q_INVOKABLE void bootstrap(const QString &email = {}, const QString &accountToken = {});
    Q_INVOKABLE bool importAccountKey(const QString &key);
    Q_INVOKABLE void signIn(const QString &email, const QString &password);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void selectAuto();
    Q_INVOKABLE void selectCountry(const QString &countryCode);
    Q_INVOKABLE void selectCity(const QString &countryCode, const QString &city);
    Q_INVOKABLE void selectLocation(int locationId);
    QString bestAvailableCountry() const;
    Q_INVOKABLE void reportConnectionEvent(const QString &event, int serverId, const QString &protocol,
                                            double rttMs = -1, double jitterMs = -1, double packetLossPct = -1,
                                            qint64 rxBytes = -1, qint64 txBytes = -1);

signals:
    void countriesChanged();
    void locationsChanged();
    void subscriptionChanged();
    void registeredChanged();
    void accountEmailChanged();
    void accountAuthenticated(const QString &email);
    void busyChanged();
    void lastErrorChanged();
    void managedConnectionSetReady(const QJsonObject &allocation, bool connectNow);

private:
    void loadPersistedIdentity();
    void persistIdentity();
    void ensureDeviceKeyPair();
    void syncAllServers(bool connectNow);
    void setBusy(bool value);
    void setError(const QString &message);
    QVariantList jsonArrayToVariant(const QJsonArray &array) const;

    ManagedApiClient m_api;
    QString m_deviceUuid;
    QString m_accountEmail;
    QString m_pendingEmail;
    QString m_token;
    QString m_publicKey;
    QString m_privateKey;
    QString m_platform = "unknown";
    QString m_appVersion;
    QVariantList m_countries;
    QVariantList m_locations;
    QString m_subscriptionPlan = "free";
    QString m_accessMode = "none";
    bool m_accessAllowed = false;
    qint64 m_trafficUsedBytes = 0;
    qint64 m_trafficLimitBytes = 0;
    qint64 m_trafficRemainingBytes = 0;
    QString m_geoTier = "lite";
    bool m_trialAvailable = true;
    bool m_busy = false;
    bool m_connectAfterAllocation = true;
    bool m_syncServersOnStart = false;
    bool m_accountAuthenticationPending = false;
    QString m_lastError;
};
