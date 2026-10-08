#pragma once

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QString>
#include <QVector>
#include <QTimer>
#include <QElapsedTimer>
#include <QTcpSocket>

#include "core/protocols/vpnProtocol.h"

class ImportController;
class ConnectionController;
class SecureServersRepository;
class ManagedServiceController;

// Bridges Pow VPN's /api/v1/client/allocate response to upstream Amnezia's
// native connection pipeline (ImportController -> SecureServersRepository ->
// ConnectionController), and drives client-side failover per
// client-overlay/FAILOVER.md.
//
// This does NOT reimplement WireGuard/VLESS. It builds the same plain-text
// WireGuard/AmneziaWG ".conf" block and "vless://" share-link formats that
// ImportController::extractConfigFromData() already parses for manually
// imported servers, then reuses the existing import + connect pipeline.
class ManagedConnectionAdapter : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool connected READ isConnected NOTIFY stateChanged)
    Q_PROPERTY(QString stateText READ stateText NOTIFY stateChanged)
    Q_PROPERTY(QString activeProtocol READ activeProtocol NOTIFY stateChanged)
    Q_PROPERTY(QString activeCity READ activeCity NOTIFY stateChanged)
    Q_PROPERTY(QString activeCountryCode READ activeCountryCode NOTIFY stateChanged)
    Q_PROPERTY(QString lastFailoverReason READ lastFailoverReason NOTIFY stateChanged)

public:
    explicit ManagedConnectionAdapter(ImportController *importController,
                                       SecureServersRepository *serversRepository,
                                       ConnectionController *connectionController,
                                       ManagedServiceController *managedServiceController,
                                       QObject *parent = nullptr);

    bool isConnected() const;
    QString stateText() const;
    QString activeProtocol() const { return m_activeProtocol; }
    QString activeCity() const { return m_activeCity; }
    QString activeCountryCode() const { return m_activeCountryCode; }
    QString lastFailoverReason() const { return m_lastFailoverReason; }

    Q_INVOKABLE void disconnectManaged();
    Q_INVOKABLE void connectCountry(const QString &countryCode, const QString &accountToken = {});

signals:
    void stateChanged();
    void managedProfilesReady();
    void connectionFailed(const QString &message);

private slots:
    void onAllocationReady(const QJsonObject &allocation, bool connectNow);
    void onConnectionStateChanged(Vpn::ConnectionState state);
    void onQualityProbe();
    void onProbeFinished(bool ok, qint64 elapsedMs);

private:
    struct Candidate
    {
        QString localServerId;   // id in Amnezia's own SecureServersRepository
        int backendServerId = 0; // Pow VPN control-plane VpnServer.id (for telemetry)
        int priority = 0;       // backend connection priority, independent of UI order
        QString protocol;        // "amneziawg" | "vless"
        QString countryCode;
        QString city;
        QString endpointHost;    // for the quality probe only; never used for the tunnel itself
        quint16 endpointPort = 0;
        qint64 baselineProbeMs = -1;
    };

    QString buildAwgConfigText(const QJsonObject &config, const QString &clientPrivateKey) const;
    QString buildVlessUri(const QJsonObject &config, const QString &label) const;
    QString importCandidate(const QJsonObject &connectionEntry);
    void connectToCandidate(int index, const QString &reasonEvent);
    void scheduleFallback(const QString &reasonEvent, bool isHardFailure);
    void resetFailoverBookkeeping();
    void requestSelectedCountry();
    void sendTelemetry(const QString &event, double rttMs = -1, double jitterMs = -1, double packetLossPct = -1);

    ImportController *m_importController;
    SecureServersRepository *m_serversRepository;
    ConnectionController *m_connectionController;
    ManagedServiceController *m_managedServiceController;

    QVector<Candidate> m_candidates;
    int m_currentIndex = -1;
    QString m_activeProtocol;
    QString m_activeCity;
    QString m_activeCountryCode;
    QString m_lastFailoverReason;
    QString m_requestedCountry;
    QString m_requestedAccountToken;
    bool m_browserConnectRequested = false;
    Vpn::ConnectionState m_state = Vpn::Disconnected;

    // Hard-failure bookkeeping: 3 failed attempts within 8s -> next candidate (FAILOVER.md).
    int m_hardFailureCount = 0;
    QElapsedTimer m_hardFailureWindow;

    // Voluntary (quality-driven) switch bookkeeping.
    QElapsedTimer m_cooldownTimer;
    QVector<qint64> m_recentSwitchTimestampsMs; // for "never switch more than twice in 5 minutes"

    // Best-effort quality monitoring: periodic TCP-connect latency probe to the
    // active endpoint's own host:port (never a third-party host, so this stays
    // consistent with "no browsing/destination telemetry"). This is a
    // simplified stand-in for real per-tunnel RTT/loss stats (see the note in
    // client-overlay/INTEGRATION.md) and does not implement the "probe backup,
    // switch only if >=20% better" comparison -- it only detects sustained
    // degradation on the active candidate and falls over to the next one,
    // respecting the 180s cooldown and the 2-switches/5min cap.
    QTimer m_qualityTimer;
    // TCP quality probing only makes sense for TCP-based protocols (VLESS).
    // AmneziaWG/WireGuard connect over UDP, so a TCP connect to their
    // endpoint always fails and must never be read as "unreachable".
    bool m_probeSupported = false;
    QTcpSocket *m_probeSocket = nullptr;
    QElapsedTimer m_probeClock;
    int m_degradedSamples = 0;
};
