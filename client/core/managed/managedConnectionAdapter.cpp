#include "managedConnectionAdapter.h"
#include "managedServiceController.h"

#include <algorithm>

#include <QDateTime>
#include <QDebug>
#include <QJsonArray>
#include <QPair>
#include <QSettings>
#include <QSet>
#include <QStringList>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>

#include "core/controllers/connectionController.h"
#include "core/controllers/selfhosted/importController.h"
#include "core/repositories/secureServersRepository.h"
#include "core/utils/errorCodes.h"
#include "core/utils/constants/configKeys.h"
#include "core/utils/serverConfigUtils.h"

using namespace amnezia;

namespace
{
// Keys AmneziaWG/WireGuard-style import recognizes; see
// client/core/utils/constants/protocolConstants.h (namespace awg) and
// client/core/utils/constants/configKeys.h (awgProtocolKeys()) upstream, and
// The node agent's parse_awg_params() implementation on the control-plane side --
// all three must agree on these exact names.
const QStringList kAwgObfuscationKeys = {
    "Jc",  "Jmin", "Jmax", "S1",  "S2",  "S3",  "S4",
    "H1",  "H2",   "H3",   "H4",  "I1",  "I2",  "I3", "I4", "I5",
    "HeaderProtectionKey", "ContentPaddingAddition", "RekeyAfterTime",
    "RekeyTimeout", "RejectAfterTime", "KeepaliveTimeout",
    "MaxHandshakeAttempts", "RandomTrailers", "DisableCookies",
};

constexpr int kHardFailureWindowMs = 8000;
constexpr int kHardFailureThreshold = 3;
constexpr int kVoluntarySwitchCooldownMs = 180000;
constexpr int kMaxVoluntarySwitchesPer5Min = 2;
constexpr int kQualityProbeIntervalMs = 15000;
// RTT > 3x baseline for 90s -> probe backup (FAILOVER.md). With a 15s sampling
// interval that is ~6 consecutive degraded samples; an unreachable endpoint
// (probe failure/timeout) counts as maximally degraded immediately.
constexpr int kDegradedSamplesForRttThreshold = 6;
constexpr int kDegradedSamplesForUnreachable = 3;
constexpr int kProbeTimeoutMs = 3000;
constexpr qint64 kRttDegradedMultiplier = 3;
constexpr auto kManagedServerIdsSettingsKey = "managed/importedServerIds";

QStringList storedManagedServerIds()
{
    return QSettings().value(QString::fromLatin1(kManagedServerIdsSettingsKey)).toStringList();
}

void storeManagedServerIds(const QStringList &ids)
{
    QSettings settings;
    if (ids.isEmpty()) {
        settings.remove(QString::fromLatin1(kManagedServerIdsSettingsKey));
    } else {
        settings.setValue(QString::fromLatin1(kManagedServerIdsSettingsKey), ids);
    }
}

QPair<QString, quint16> parseHostPort(const QString &hostPort, quint16 fallbackPort)
{
    const int sep = hostPort.lastIndexOf(':');
    if (sep <= 0) {
        return { hostPort, fallbackPort };
    }
    bool ok = false;
    const quint16 port = hostPort.mid(sep + 1).toUShort(&ok);
    return { hostPort.left(sep), ok ? port : fallbackPort };
}
} // namespace

ManagedConnectionAdapter::ManagedConnectionAdapter(ImportController *importController,
                                                   SecureServersRepository *serversRepository,
                                                   ConnectionController *connectionController,
                                                   ManagedServiceController *managedServiceController,
                                                   QObject *parent)
    : QObject(parent),
      m_importController(importController),
      m_serversRepository(serversRepository),
      m_connectionController(connectionController),
      m_managedServiceController(managedServiceController)
{
    if (m_managedServiceController) {
        connect(m_managedServiceController, &ManagedServiceController::managedConnectionSetReady,
                this, &ManagedConnectionAdapter::onAllocationReady);
        connect(m_managedServiceController, &ManagedServiceController::registeredChanged, this, [this] {
            if (m_browserConnectRequested && m_managedServiceController->registered()) requestSelectedCountry();
        });
        connect(m_managedServiceController, &ManagedServiceController::countriesChanged, this, [this] {
            if (m_browserConnectRequested) requestSelectedCountry();
        });
    }
    if (m_connectionController) {
        connect(m_connectionController, &ConnectionController::connectionStateChanged,
                this, &ManagedConnectionAdapter::onConnectionStateChanged);
        // Connections started from the regular server picker use
        // ConnectionController directly. Associate that selected repository
        // entry with its managed candidate too, otherwise failover/telemetry
        // remain detached from the tunnel the user actually selected.
        connect(m_connectionController, &ConnectionController::openConnectionRequested, this,
                [this](const QString &serverId, DockerContainer, const QJsonObject &) {
                    for (int i = 0; i < m_candidates.size(); ++i) {
                        if (m_candidates.at(i).localServerId != serverId) continue;
                        if (m_currentIndex == i) return;

                        m_currentIndex = i;
                        const Candidate &candidate = m_candidates.at(i);
                        m_activeProtocol = candidate.protocol;
                        m_activeCity = candidate.city;
                        m_activeCountryCode = candidate.countryCode;
                        m_lastFailoverReason = QStringLiteral("connect_attempt");
                        m_degradedSamples = 0;
                        m_probeSupported = candidate.protocol == QStringLiteral("vless");
                        sendTelemetry(QStringLiteral("connect_attempt"));
                        emit stateChanged();
                        return;
                    }
                });
    }
    m_qualityTimer.setInterval(kQualityProbeIntervalMs);
    connect(&m_qualityTimer, &QTimer::timeout, this, &ManagedConnectionAdapter::onQualityProbe);
}

bool ManagedConnectionAdapter::isConnected() const
{
    return m_state == Vpn::Connected;
}

QString ManagedConnectionAdapter::stateText() const
{
    switch (m_state) {
    case Vpn::Disconnected: return QStringLiteral("disconnected");
    case Vpn::Preparing: return QStringLiteral("preparing");
    case Vpn::Connecting: return QStringLiteral("connecting");
    case Vpn::Connected: return QStringLiteral("connected");
    case Vpn::Disconnecting: return QStringLiteral("disconnecting");
    case Vpn::Reconnecting: return QStringLiteral("reconnecting");
    case Vpn::Error: return QStringLiteral("error");
    default: return QStringLiteral("unknown");
    }
}

QString ManagedConnectionAdapter::buildAwgConfigText(const QJsonObject &config, const QString &clientPrivateKey) const
{
    QStringList lines;
    lines << QStringLiteral("[Interface]");
    lines << QStringLiteral("PrivateKey = %1").arg(clientPrivateKey);
    lines << QStringLiteral("Address = %1").arg(config.value("address").toString());

    QStringList dnsList;
    for (const auto &d : config.value("dns").toArray()) {
        const QString s = d.toString();
        if (!s.isEmpty()) dnsList << s;
    }
    if (!dnsList.isEmpty()) {
        lines << QStringLiteral("DNS = %1").arg(dnsList.join(QStringLiteral(", ")));
    }

    const QJsonObject obfuscation = config.value("obfuscation").toObject();
    for (const QString &key : kAwgObfuscationKeys) {
        if (obfuscation.contains(key)) {
            lines << QStringLiteral("%1 = %2").arg(key, obfuscation.value(key).toVariant().toString());
        }
    }

    lines << QString();
    lines << QStringLiteral("[Peer]");
    lines << QStringLiteral("PublicKey = %1").arg(config.value("server_public_key").toString());
    const QString psk = config.value("preshared_key").toString();
    if (!psk.isEmpty()) {
        lines << QStringLiteral("PresharedKey = %1").arg(psk);
    }
    // Pow VPN nodes currently provide IPv4 egress only. Do not advertise an
    // IPv6 default route until the node fleet has global IPv6 forwarding.
    lines << QStringLiteral("AllowedIPs = 0.0.0.0/0");
    lines << QStringLiteral("Endpoint = %1").arg(config.value("endpoint").toString());
    lines << QStringLiteral("PersistentKeepalive = %1").arg(config.value("persistent_keepalive").toInt(25));

    return lines.join(QLatin1Char('\n'));
}

QString ManagedConnectionAdapter::buildVlessUri(const QJsonObject &config, const QString &label) const
{
    const QString clientId = config.value("client_id").toString();
    const QString address = config.value("address").toString();
    const int port = config.value("port").toInt(443);
    const QString flow = config.value("flow").toString();
    const QString network = config.value("network").toString(QStringLiteral("tcp"));
    const QString sni = config.value("server_name").toString();
    const QString pbk = config.value("public_key").toString();
    const QString sid = config.value("short_id").toString();
    const QString fp = config.value("fingerprint").toString(QStringLiteral("chrome"));

    QString query = QStringLiteral("security=reality&sni=%1&fp=%2&pbk=%3&sid=%4&type=%5")
                        .arg(sni, fp, pbk, sid, network);
    if (!flow.isEmpty()) {
        query += QStringLiteral("&flow=%1").arg(flow);
    }

    const QString fragment = QString::fromUtf8(QUrl::toPercentEncoding(label));
    return QStringLiteral("vless://%1@%2:%3?%4#%5").arg(clientId, address, QString::number(port), query, fragment);
}

QString ManagedConnectionAdapter::importCandidate(const QJsonObject &entry)
{
    if (!m_importController || !m_serversRepository) {
        return QString();
    }

    const QString protocol = entry.value("protocol").toString();
    const QJsonObject config = entry.value("config").toObject();
    const QJsonObject location = entry.value("location").toObject();
    const QString city = location.value("city").toString();
    const QString country = location.value("country_name").toString();
    const QString displayName = !country.isEmpty() && !city.isEmpty()
                                    ? QStringLiteral("%1 - %2").arg(country, city)
                                    : (!country.isEmpty() ? country : city);
    const QString label = QStringLiteral("Pow VPN - %1").arg(displayName);

    QString rawText;
    if (protocol == QStringLiteral("amneziawg")) {
        const QString privateKey = m_managedServiceController ? m_managedServiceController->devicePrivateKey() : QString();
        if (privateKey.isEmpty()) {
            qWarning() << "ManagedConnectionAdapter: no local WireGuard private key yet, skipping AWG candidate";
            return QString();
        }
        rawText = buildAwgConfigText(config, privateKey);
    } else if (protocol == QStringLiteral("vless")) {
        rawText = buildVlessUri(config, label);
    } else {
        qWarning() << "ManagedConnectionAdapter: unsupported protocol from control plane:" << protocol;
        return QString();
    }

    const ImportController::ImportResult result = m_importController->extractConfigFromData(rawText);
    if (result.errorCode != ErrorCode::NoError || result.config.isEmpty()) {
        qWarning() << "ManagedConnectionAdapter: failed to parse generated" << protocol << "config, errorCode="
                   << static_cast<int>(result.errorCode);
        return QString();
    }

    // The control plane already told us which country this candidate belongs to; use
    // it as the server description so the client lists e.g. "Canada" instead of the
    // generic "Server N" that ImportController assigns when a config has no description.
    QJsonObject namedConfig = result.config;
    if (!displayName.isEmpty()) {
        namedConfig[configKey::description] = displayName;
    }

    return m_serversRepository->addServer(QString(), namedConfig, serverConfigUtils::configTypeFromJson(namedConfig));
}

void ManagedConnectionAdapter::onAllocationReady(const QJsonObject &allocation, bool connectNow)
{
    if (m_connectionController && m_currentIndex >= 0) {
        m_connectionController->closeConnection();
    }
    m_qualityTimer.stop();
    // Managed entries are projections of the current server list. Their local
    // repository IDs must survive process restarts so a new allocation can
    // replace, rather than duplicate, the previous projection. Older builds did
    // not persist those IDs, so migrate their AWG entries by matching the local
    // device private key. A manually imported profile uses another private key
    // and is therefore left untouched.
    if (m_serversRepository) {
        const QStringList persistedIds = storedManagedServerIds();
        QSet<QString> managedIds(persistedIds.cbegin(), persistedIds.cend());
        for (const Candidate &candidate : m_candidates) {
            if (!candidate.localServerId.isEmpty()) managedIds.insert(candidate.localServerId);
        }

        const QString devicePrivateKey = m_managedServiceController
                                             ? m_managedServiceController->devicePrivateKey()
                                             : QString();
        if (!devicePrivateKey.isEmpty()) {
            for (const QString &serverId : m_serversRepository->orderedServerIds()) {
                const auto server = m_serversRepository->selfHostedUserConfig(serverId);
                if (!server.has_value()) continue;

                for (auto it = server->containers.cbegin(); it != server->containers.cend(); ++it) {
                    const AwgProtocolConfig *awg = it.value().getAwgProtocolConfig();
                    if (awg && awg->clientConfig.has_value()
                        && awg->clientConfig->clientPrivateKey == devicePrivateKey) {
                        managedIds.insert(serverId);
                        break;
                    }
                }
            }
        }

        for (const QString &serverId : managedIds) {
            if (m_serversRepository->indexOfServerId(serverId) >= 0) {
                m_serversRepository->removeServer(serverId);
            }
        }
        storeManagedServerIds({});
    }
    m_candidates.clear();
    m_currentIndex = -1;
    resetFailoverBookkeeping();

    // Profiles are shown alphabetically by country/city. Connection priority
    // remains separate and follows the backend health order.
    QVector<QJsonObject> displayEntries;
    for (const auto &value : allocation.value("connections").toArray()) displayEntries.push_back(value.toObject());
    std::sort(displayEntries.begin(), displayEntries.end(), [](const QJsonObject &left, const QJsonObject &right) {
        const QJsonObject leftLocation = left.value("location").toObject();
        const QJsonObject rightLocation = right.value("location").toObject();
        const int country = QString::compare(leftLocation.value("country_name").toString(), rightLocation.value("country_name").toString(), Qt::CaseInsensitive);
        if (country != 0) return country < 0;
        return QString::compare(leftLocation.value("city").toString(), rightLocation.value("city").toString(), Qt::CaseInsensitive) < 0;
    });
    for (const QJsonObject &entry : displayEntries) {
        const QString localServerId = importCandidate(entry);
        if (localServerId.isEmpty()) {
            continue; // best-effort: skip candidates we couldn't build/import, keep the rest
        }
        Candidate c;
        c.localServerId = localServerId;
        c.backendServerId = entry.value("server_id").toInt();
        c.priority = entry.value("priority").toInt(m_candidates.size());
        c.protocol = entry.value("protocol").toString();
        const QJsonObject location = entry.value("location").toObject();
        c.countryCode = location.value("country_code").toString();
        c.city = location.value("city").toString();

        const QJsonObject config = entry.value("config").toObject();
        if (c.protocol == QStringLiteral("amneziawg")) {
            const auto hostPort = parseHostPort(config.value("endpoint").toString(), 0);
            c.endpointHost = hostPort.first;
            c.endpointPort = hostPort.second;
        } else {
            c.endpointHost = config.value("address").toString();
            c.endpointPort = static_cast<quint16>(config.value("port").toInt(443));
        }
        m_candidates.push_back(c);
    }

    std::sort(m_candidates.begin(), m_candidates.end(), [](const Candidate &left, const Candidate &right) {
        return left.priority < right.priority;
    });

    QStringList importedManagedIds;
    importedManagedIds.reserve(m_candidates.size());
    for (const Candidate &candidate : std::as_const(m_candidates)) {
        if (!candidate.localServerId.isEmpty()) importedManagedIds.append(candidate.localServerId);
    }
    storeManagedServerIds(importedManagedIds);

    if (m_candidates.isEmpty()) {
        emit connectionFailed(QStringLiteral("Could not build a usable server configuration"));
        return;
    }

    emit managedProfilesReady();

    // The control plane orders candidates by connection priority. Startup sync
    // refreshes local profiles only; explicit user choice begins a connection.
    if (connectNow) connectToCandidate(0, QStringLiteral("connect_attempt"));
    else emit stateChanged();
}

void ManagedConnectionAdapter::connectToCandidate(int index, const QString &reasonEvent)
{
    if (index < 0 || index >= m_candidates.size() || !m_connectionController) {
        return;
    }
    if (m_currentIndex >= 0 && m_currentIndex < m_candidates.size() && m_currentIndex != index) {
        m_connectionController->closeConnection();
    }
    m_currentIndex = index;
    const Candidate &c = m_candidates[index];
    m_activeProtocol = c.protocol;
    m_activeCity = c.city;
    m_activeCountryCode = c.countryCode;
    m_lastFailoverReason = reasonEvent;
    m_degradedSamples = 0;
    // Only VLESS/REALITY is TCP; AWG/WireGuard are UDP and cannot be
    // latency-probed with a TCP connect. Probing a UDP port over TCP always
    // fails and would mark a perfectly healthy tunnel as unreachable.
    m_probeSupported = (c.protocol == QStringLiteral("vless"));

    sendTelemetry(reasonEvent);
    m_connectionController->openConnection(c.localServerId);
    emit stateChanged();
}

void ManagedConnectionAdapter::disconnectManaged()
{
    if (m_connectionController) {
        m_connectionController->closeConnection();
    }
    m_qualityTimer.stop();
    if (m_currentIndex >= 0) {
        sendTelemetry(QStringLiteral("disconnect"));
    }
    m_currentIndex = -1;
    emit stateChanged();
}

void ManagedConnectionAdapter::connectCountry(const QString &countryCode, const QString &accountToken)
{
    if (!m_managedServiceController) {
        emit connectionFailed(QStringLiteral("Pow VPN managed service is unavailable"));
        return;
    }
    m_requestedCountry = countryCode.trimmed().toUpper();
    m_requestedAccountToken = accountToken;
    m_browserConnectRequested = true;
    // A browser-extension account token is authoritative: it must replace an
    // old guest/device session before requesting the selected location.
    if (!accountToken.trimmed().isEmpty()) {
        m_managedServiceController->bootstrap({}, accountToken);
        return;
    }
    if (!m_managedServiceController->registered()) {
        m_managedServiceController->bootstrap();
        return;
    }
    requestSelectedCountry();
}

void ManagedConnectionAdapter::requestSelectedCountry()
{
    if (!m_managedServiceController || !m_managedServiceController->registered()) return;
    const QString country = m_requestedCountry;
    m_browserConnectRequested = false;
    if (country.isEmpty() || country == QStringLiteral("AUTO")) {
        m_managedServiceController->selectAuto();
        return;
    }
    m_managedServiceController->selectCountry(country);
}
void ManagedConnectionAdapter::resetFailoverBookkeeping()
{
    m_hardFailureCount = 0;
    m_hardFailureWindow.invalidate();
    m_cooldownTimer.invalidate();
    m_recentSwitchTimestampsMs.clear();
}

void ManagedConnectionAdapter::scheduleFallback(const QString &reasonEvent, bool isHardFailure)
{
    if (m_candidates.isEmpty()) {
        return;
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    while (!m_recentSwitchTimestampsMs.isEmpty() && now - m_recentSwitchTimestampsMs.first() > 5 * 60 * 1000) {
        m_recentSwitchTimestampsMs.removeFirst();
    }

    // "Never switch more than twice in 5 minutes unless the active tunnel is dead."
    // A hard failure means the tunnel IS dead, so it always bypasses this cap;
    // only voluntary (quality-driven) switches respect it.
    if (!isHardFailure) {
        if (m_recentSwitchTimestampsMs.size() >= kMaxVoluntarySwitchesPer5Min) {
            return;
        }
        if (m_cooldownTimer.isValid() && m_cooldownTimer.elapsed() < kVoluntarySwitchCooldownMs) {
            return;
        }
    }

    const int nextIndex = m_currentIndex + 1 >= m_candidates.size() ? 0 : m_currentIndex + 1;
    if (nextIndex == m_currentIndex) {
        emit connectionFailed(QStringLiteral("All candidate servers failed"));
        return;
    }
    // Only wrap around to a candidate we've already tried on a hard failure loop
    // (all others exhausted); otherwise stop once we run out of untried candidates.
    if (!isHardFailure && nextIndex <= m_currentIndex) {
        return;
    }

    const bool protocolChanged = m_candidates[nextIndex].protocol != m_candidates[m_currentIndex].protocol;
    m_recentSwitchTimestampsMs.push_back(now);
    m_cooldownTimer.restart();
    connectToCandidate(nextIndex, protocolChanged ? QStringLiteral("protocol_fallback") : reasonEvent);
}

void ManagedConnectionAdapter::onConnectionStateChanged(Vpn::ConnectionState state)
{
    m_state = state;
    emit stateChanged();

    if (state == Vpn::Connected) {
        m_hardFailureCount = 0;
        m_hardFailureWindow.invalidate();
        m_degradedSamples = 0;
        sendTelemetry(QStringLiteral("connect_success"));
        m_qualityTimer.start();
        return;
    }

    if (state == Vpn::Error) {
        sendTelemetry(QStringLiteral("connect_failed"));
        m_qualityTimer.stop();

        if (!m_hardFailureWindow.isValid() || m_hardFailureWindow.elapsed() > kHardFailureWindowMs) {
            m_hardFailureWindow.restart();
            m_hardFailureCount = 1;
        } else {
            ++m_hardFailureCount;
        }

        if (m_hardFailureCount >= kHardFailureThreshold) {
            m_hardFailureCount = 0;
            scheduleFallback(QStringLiteral("server_switch"), /*isHardFailure=*/true);
        } else if (m_currentIndex >= 0 && m_currentIndex < m_candidates.size() && m_connectionController) {
            // Quick retry on the same candidate before giving up on it.
            m_connectionController->openConnection(m_candidates[m_currentIndex].localServerId);
        }
        return;
    }

    if (state == Vpn::Disconnected) {
        m_qualityTimer.stop();
    }
}

void ManagedConnectionAdapter::onQualityProbe()
{
    if (m_state != Vpn::Connected || m_currentIndex < 0 || m_currentIndex >= m_candidates.size()) {
        return;
    }
    if (m_probeSocket) {
        return; // a probe is already in flight; skip this tick rather than pile up
    }
    if (!m_probeSupported) {
        // AWG/WireGuard is UDP; a TCP connect probe is meaningless here.
        return;
    }

    Candidate &c = m_candidates[m_currentIndex];
    if (c.endpointHost.isEmpty() || c.endpointPort == 0) {
        return; // nothing sane to probe (shouldn't happen once onAllocationReady ran)
    }

    if (!m_connectionController || !m_connectionController->isConnected()) {
        // The controller itself already thinks we're not connected; let
        // onConnectionStateChanged() drive failover instead of double-counting here.
        return;
    }

    m_probeSocket = new QTcpSocket(this);
    m_probeClock.start();

    auto *timeoutGuard = new QTimer(this);
    timeoutGuard->setSingleShot(true);

    connect(m_probeSocket, &QTcpSocket::connected, this, [this, timeoutGuard]() {
        const qint64 elapsed = m_probeClock.elapsed();
        timeoutGuard->stop();
        timeoutGuard->deleteLater();
        m_probeSocket->deleteLater();
        m_probeSocket = nullptr;
        onProbeFinished(true, elapsed);
    });
    connect(m_probeSocket, &QTcpSocket::errorOccurred, this, [this, timeoutGuard](QAbstractSocket::SocketError) {
        timeoutGuard->stop();
        timeoutGuard->deleteLater();
        m_probeSocket->deleteLater();
        m_probeSocket = nullptr;
        onProbeFinished(false, -1);
    });
    connect(timeoutGuard, &QTimer::timeout, this, [this, timeoutGuard]() {
        timeoutGuard->deleteLater();
        if (m_probeSocket) {
            m_probeSocket->abort();
            m_probeSocket->deleteLater();
            m_probeSocket = nullptr;
        }
        onProbeFinished(false, -1);
    });

    timeoutGuard->start(kProbeTimeoutMs);
    m_probeSocket->connectToHost(c.endpointHost, c.endpointPort);
}

void ManagedConnectionAdapter::onProbeFinished(bool ok, qint64 elapsedMs)
{
    if (m_currentIndex < 0 || m_currentIndex >= m_candidates.size()) {
        return;
    }
    Candidate &c = m_candidates[m_currentIndex];

    if (!ok) {
        m_degradedSamples = qMax(m_degradedSamples + 1, kDegradedSamplesForUnreachable);
    } else if (c.baselineProbeMs < 0) {
        // First successful sample after connecting becomes this session's baseline,
        // approximating "location baseline" from FAILOVER.md without requiring
        // extra plumbing from the control plane's own provisioning-time benchmark.
        c.baselineProbeMs = elapsedMs;
        m_degradedSamples = 0;
    } else if (elapsedMs > c.baselineProbeMs * kRttDegradedMultiplier) {
        ++m_degradedSamples;
        sendTelemetry(QStringLiteral("quality_degraded"), static_cast<double>(elapsedMs));
    } else {
        m_degradedSamples = 0;
    }

    const int threshold = ok ? kDegradedSamplesForRttThreshold : kDegradedSamplesForUnreachable;
    if (m_degradedSamples >= threshold) {
        m_degradedSamples = 0;
        scheduleFallback(QStringLiteral("quality_degraded"), /*isHardFailure=*/false);
    }
}

void ManagedConnectionAdapter::sendTelemetry(const QString &event, double rttMs, double jitterMs, double packetLossPct)
{
    if (!m_managedServiceController || m_currentIndex < 0 || m_currentIndex >= m_candidates.size()) {
        return;
    }
    const Candidate &c = m_candidates[m_currentIndex];
    m_managedServiceController->reportConnectionEvent(event, c.backendServerId, c.protocol, rttMs, jitterMs, packetLossPct);
}
