#include "managedServiceController.h"

#include <QJsonValue>
#include <QSettings>
#include <QUrlQuery>
#include <QUuid>

#include "core/configurators/wireguardConfigurator.h"

ManagedServiceController::ManagedServiceController(QObject *parent) : QObject(parent)
{
    loadPersistedIdentity();
    ensureDeviceKeyPair();
    connect(&m_api, &ManagedApiClient::accountLoginSucceeded, this, [this](const QString &email, const QString &accountToken) {
        m_pendingEmail = email.trimmed().toLower();
        m_accountAuthenticationPending = true;
        m_api.registerDevice(m_deviceUuid, m_platform, m_appVersion, m_publicKey, m_pendingEmail, accountToken);
    });
    connect(&m_api, &ManagedApiClient::deviceRegistered, this, [this](int, int, const QString &token, const QString &email) {
        const bool wasRegistered = registered();
        const bool accountAuthenticationCompleted = m_accountAuthenticationPending;
        const QString normalizedEmail = (email.isEmpty() ? m_pendingEmail : email).trimmed().toLower();
        const bool emailChanged = m_accountEmail != normalizedEmail;
        m_accountEmail = normalizedEmail;
        m_pendingEmail.clear();
        m_token = token; m_api.setBearerToken(token); persistIdentity(); setBusy(false);
        if (emailChanged) emit accountEmailChanged();
        if (!wasRegistered) emit registeredChanged();
        if (accountAuthenticationCompleted) emit accountAuthenticated(m_accountEmail);
        m_accountAuthenticationPending = false;
        m_syncServersOnStart = true;
        refresh();
    });
    connect(&m_api, &ManagedApiClient::countriesReady, this, [this](const QJsonArray &rows) {
        m_countries = jsonArrayToVariant(rows); emit countriesChanged(); setBusy(false);
    });
    connect(&m_api, &ManagedApiClient::locationsReady, this, [this](const QJsonArray &rows) {
        m_locations = jsonArrayToVariant(rows); emit locationsChanged();
    });
    connect(&m_api, &ManagedApiClient::subscriptionReady, this, [this](const QJsonObject &o) {
        const QString subscriptionEmail = o.value("email").toString().trimmed().toLower();
        if (!subscriptionEmail.isEmpty() && subscriptionEmail != m_accountEmail) {
            m_accountEmail = subscriptionEmail;
            persistIdentity();
            emit accountEmailChanged();
        }
        m_subscriptionPlan = o.value("plan").toString("free");
        m_accessMode = o.value("access_mode").toString("none");
        m_accessAllowed = o.value("access_allowed").toBool(false);
        const auto traffic = o.value("traffic").toObject();
        m_trafficUsedBytes = static_cast<qint64>(traffic.value("used_bytes").toDouble(0));
        m_trafficLimitBytes = static_cast<qint64>(traffic.value("limit_bytes").toDouble(0));
        m_trafficRemainingBytes = static_cast<qint64>(traffic.value("remaining_bytes").toDouble(0));
        m_geoTier = o.value("geo_tier").toString("lite");
        m_trialAvailable = o.value("trial").toObject().value("available").toBool(true);
        emit subscriptionChanged();
        if (m_syncServersOnStart) {
            m_syncServersOnStart = false;
            if (m_accessAllowed) syncAllServers(false);
        }
    });
    connect(&m_api, &ManagedApiClient::allocationReady, this, [this](const QJsonObject &o) {
        const bool connectNow = m_connectAfterAllocation;
        m_connectAfterAllocation = true;
        setBusy(false); emit managedConnectionSetReady(o, connectNow);
    });
    connect(&m_api, &ManagedApiClient::requestFailed, this, [this](const QString &operation, int code, const QString &message) {
        setBusy(false);
        if (operation == QStringLiteral("login") && code == 401) {
            setError(QStringLiteral("Incorrect email or password"));
            return;
        }
        if (code == 401 && !registered()) {
            setError(QStringLiteral("Sign in with your email and password to use Pow VPN"));
            return;
        }
        setError(QString("HTTP %1: %2").arg(code).arg(message));
    });
}

void ManagedServiceController::setApiBaseUrl(const QUrl &url) { m_api.setBaseUrl(url); if (!m_token.isEmpty()) m_api.setBearerToken(m_token); }
void ManagedServiceController::setDevicePublicKey(const QString &publicKey) { m_publicKey = publicKey; }
void ManagedServiceController::setPlatformInfo(const QString &platform, const QString &appVersion) { m_platform = platform; m_appVersion = appVersion; }

void ManagedServiceController::bootstrap(const QString &email, const QString &accountToken)
{
    if (!accountToken.trimmed().isEmpty()) {
        m_pendingEmail = email.trimmed().toLower();
        m_accountAuthenticationPending = !m_pendingEmail.isEmpty();
        setBusy(true);
        m_api.registerDevice(m_deviceUuid, m_platform, m_appVersion, m_publicKey, m_pendingEmail, accountToken);
        return;
    }
    if (!m_token.isEmpty()) {
        m_api.setBearerToken(m_token);
        // Refresh the device registration even for a persisted session.  The
        // WireGuard public key is part of the server-side peer identity; simply
        // fetching locations after a local key-store migration would otherwise
        // leave the control plane issuing profiles for an obsolete key.
        m_syncServersOnStart = true;
        setBusy(true);
        m_api.registerDevice(m_deviceUuid, m_platform, m_appVersion, m_publicKey, {}, {});
        return;
    }
    setBusy(true); m_api.registerDevice(m_deviceUuid, m_platform, m_appVersion, m_publicKey, email, accountToken);
}

bool ManagedServiceController::importAccountKey(const QString &key)
{
    const QUrl url(key.trimmed());
    if (!url.isValid() || url.scheme().compare(QStringLiteral("powvpn"), Qt::CaseInsensitive) != 0
            || url.host().compare(QStringLiteral("account"), Qt::CaseInsensitive) != 0) {
        return false;
    }

    const QUrlQuery query(url);
    const QString accountToken = query.queryItemValue(QStringLiteral("token"), QUrl::FullyDecoded).trimmed();
    const QString email = query.queryItemValue(QStringLiteral("email"), QUrl::FullyDecoded).trimmed().toLower();
    if (accountToken.isEmpty()) {
        setError(QStringLiteral("Invalid Pow VPN account key"));
        return true;
    }

    setError(QString());
    bootstrap(email, accountToken);
    return true;
}

void ManagedServiceController::signIn(const QString &email, const QString &password)
{
    const QString normalizedEmail = email.trimmed().toLower();
    if (!normalizedEmail.contains('@') || password.isEmpty()) {
        setError(QStringLiteral("Enter your email and password"));
        return;
    }
    setError(QString());
    setBusy(true);
    m_api.login(normalizedEmail, password);
}

void ManagedServiceController::refresh()
{
    if (m_token.isEmpty()) return; setBusy(true); m_api.fetchCountries(); m_api.fetchLocations(); m_api.fetchSubscription();
}

void ManagedServiceController::syncAllServers(bool connectNow)
{
    if (m_token.isEmpty()) return;
    m_connectAfterAllocation = connectNow;
    setBusy(true);
    m_api.allocateAll({"auto"});
}

void ManagedServiceController::selectAuto()
{
    syncAllServers(true);
}

void ManagedServiceController::selectCountry(const QString &countryCode)
{
    m_connectAfterAllocation = true;
    setBusy(true); m_api.allocateCountry(countryCode, {"auto"}, 2);
}

void ManagedServiceController::selectCity(const QString &countryCode, const QString &city)
{
    m_connectAfterAllocation = true;
    setBusy(true); m_api.allocateCountry(countryCode, {"auto"}, 2, city);
}

void ManagedServiceController::selectLocation(int locationId)
{
    m_connectAfterAllocation = true;
    setBusy(true); m_api.allocateLocation(locationId, {"auto"}, 2);
}


QString ManagedServiceController::bestAvailableCountry() const
{
    QString best;
    int bestQuality = -1;
    for (const auto &value : m_countries) {
        const QVariantMap country = value.toMap();
        for (const auto &cityValue : country.value(QStringLiteral("cities")).toList()) {
            const QVariantMap city = cityValue.toMap();
            if (!city.value(QStringLiteral("available")).toBool()) continue;
            const int quality = city.value(QStringLiteral("quality")).toInt();
            if (quality > bestQuality) { bestQuality = quality; best = country.value(QStringLiteral("country_code")).toString(); }
        }
    }
    return best;
}
void ManagedServiceController::reportConnectionEvent(const QString &event, int serverId, const QString &protocol,
                                                      double rttMs, double jitterMs, double packetLossPct,
                                                      qint64 rxBytes, qint64 txBytes)
{
    if (m_token.isEmpty()) return;
    QJsonObject o{{"event", event}, {"server_id", serverId}, {"protocol", protocol}};
    if (rttMs >= 0) o.insert("rtt_ms", rttMs); if (jitterMs >= 0) o.insert("jitter_ms", jitterMs);
    if (packetLossPct >= 0) o.insert("packet_loss_pct", packetLossPct);
    if (rxBytes >= 0) o.insert("rx_bytes", static_cast<double>(rxBytes)); if (txBytes >= 0) o.insert("tx_bytes", static_cast<double>(txBytes));
    m_api.sendTelemetry(o);
}

void ManagedServiceController::loadPersistedIdentity()
{
    QSettings s; m_deviceUuid = s.value("managed/deviceUuid").toString();
    if (m_deviceUuid.isEmpty()) { m_deviceUuid = QUuid::createUuid().toString(QUuid::WithoutBraces); s.setValue("managed/deviceUuid", m_deviceUuid); }
    m_token = s.value("managed/deviceToken").toString();
    m_privateKey = s.value("managed/devicePrivateKey").toString();
    m_accountEmail = s.value("managed/accountEmail").toString();
    m_publicKey = s.value("managed/devicePublicKey").toString();
}
void ManagedServiceController::persistIdentity(){ QSettings s; s.setValue("managed/deviceUuid", m_deviceUuid); s.setValue("managed/deviceToken", m_token); s.setValue("managed/accountEmail", m_accountEmail); }

void ManagedServiceController::ensureDeviceKeyPair()
{
    // One WireGuard/AmneziaWG keypair per device, generated once and reused for
    // every allocation. Only m_publicKey is ever sent to the control plane
    // (see registerDevice() in managedApiClient.cpp); m_privateKey never leaves
    // this device and is read by ManagedConnectionAdapter to build AWG configs.
    if (!m_privateKey.isEmpty() && !m_publicKey.isEmpty()) {
        return;
    }
    const auto keys = WireguardConfigurator::genClientKeys();
    if (keys.clientPrivKey.isEmpty() || keys.clientPubKey.isEmpty()) {
        setError(QStringLiteral("Failed to generate a local WireGuard keypair"));
        return;
    }
    m_privateKey = keys.clientPrivKey;
    m_publicKey = keys.clientPubKey;
    QSettings s;
    s.setValue("managed/devicePrivateKey", m_privateKey);
    s.setValue("managed/devicePublicKey", m_publicKey);
}
void ManagedServiceController::setBusy(bool value){ if(m_busy==value)return; m_busy=value; emit busyChanged(); }
void ManagedServiceController::setError(const QString &message){ m_lastError=message; emit lastErrorChanged(); }
QVariantList ManagedServiceController::jsonArrayToVariant(const QJsonArray &array) const { QVariantList r; r.reserve(array.size()); for(const auto &v:array) r.push_back(v.toObject().toVariantMap()); return r; }
