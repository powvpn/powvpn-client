#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonDocument>
#include <QProcess>

#include <QThread>

#include <chrono>

#include "ipc.h"
#include "logger.h"
#include "ikev2VpnProtocolWindows.h"
#include "core/utils/utilities.h"
#include "core/protocols/protocolUtils.h"


static Ikev2Protocol* self = nullptr;
static std::mutex rasDialFuncMutex;

namespace
{
QString psQuoted(QString value)
{
    return value.replace("'", "''");
}
}

extern "C" {
static void WINAPI RasDialFuncCallback(UINT unMsg,
                                       RASCONNSTATE rasconnstate,
                                       DWORD dwError );
}

Ikev2Protocol::Ikev2Protocol(const QJsonObject &configuration, QObject* parent) :
    VpnProtocol(configuration, parent)
{
    self = this;
    readIkev2Configuration(configuration);
}

Ikev2Protocol::~Ikev2Protocol()
{
    qDebug() << "IpsecProtocol::~IpsecProtocol()";
    Ikev2Protocol::stop();
}

void Ikev2Protocol::stop()
{
    setConnectionState(Vpn::ConnectionState::Disconnecting);
    {
        if (!disconnect_vpn()){
            qDebug()<<"We don't disconnect";
            setConnectionState(Vpn::ConnectionState::Error);
        }
        else {
            setConnectionState(Vpn::ConnectionState::Error);
        }
    }
}

void Ikev2Protocol::newConnectionStateEventReceived(UINT unMsg, tagRASCONNSTATE rasconnstate, DWORD dwError)
{
    Q_UNUSED(unMsg);
    qDebug()<<"Receive the new event "<<static_cast<int>(rasconnstate);
    switch (rasconnstate)
    {
    case RASCS_OpenPort:
        //qDebug()<<__FUNCTION__ << __LINE__;
        setConnectionState(Vpn::ConnectionState::Preparing);
        break;
    case RASCS_PortOpened:
        //qDebug()<<__FUNCTION__ << __LINE__;
        setConnectionState(Vpn::ConnectionState::Preparing);
        break;
    case RASCS_ConnectDevice:
        //qDebug()<<__FUNCTION__ << __LINE__;
        setConnectionState(Vpn::ConnectionState::Preparing);
        break;
    case RASCS_DeviceConnected:
        //qDebug()<<__FUNCTION__ << __LINE__;
        setConnectionState(Vpn::ConnectionState::Preparing);
        break;
    case RASCS_AllDevicesConnected:
        //qDebug()<<__FUNCTION__ << __LINE__;
        setConnectionState(Vpn::ConnectionState::Preparing);
        break;
    case RASCS_Authenticate:
        //qDebug()<<__FUNCTION__ << __LINE__;
        setConnectionState(Vpn::ConnectionState::Preparing);
        break;
    case RASCS_AuthNotify:
        //qDebug()<<__FUNCTION__ << __LINE__;
        if (dwError != 0) {
            qDebug() << "have error" << dwError;
            setConnectionState(Vpn::ConnectionState::Disconnected);
        } else {
            qDebug() << "RASCS_AuthNotify but no error" << dwError;
        }
        break;
    case RASCS_AuthRetry:
        //qDebug()<<__FUNCTION__ << __LINE__;
        setConnectionState(Vpn::ConnectionState::Preparing);
        break;
    case RASCS_AuthCallback:
        qDebug()<<__FUNCTION__ << __LINE__;
        break;
    case RASCS_AuthChangePassword:
        qDebug()<<__FUNCTION__ << __LINE__;
        break;
    case RASCS_AuthProject:
        qDebug()<<__FUNCTION__ << __LINE__;
        break;
    case RASCS_AuthLinkSpeed:
        qDebug()<<__FUNCTION__ << __LINE__;
        break;
    case RASCS_AuthAck:
        qDebug()<<__FUNCTION__ << __LINE__;
        break;
    case RASCS_ReAuthenticate:
        //qDebug()<<__FUNCTION__ << __LINE__;
        break;
    case RASCS_Authenticated:
        //qDebug()<<__FUNCTION__ << __LINE__;
        break;
    case RASCS_PrepareForCallback:
        qDebug()<<__FUNCTION__ << __LINE__;
        break;
    case RASCS_WaitForModemReset:
        qDebug()<<__FUNCTION__ << __LINE__;
        break;
    case RASCS_WaitForCallback:
        qDebug()<<__FUNCTION__ << __LINE__;
        break;
    case RASCS_Projected:
        qDebug()<<__FUNCTION__ << __LINE__;
        break;
#if (WINVER >= 0x400)
    case RASCS_StartAuthentication:    // Windows 95 only
        qDebug()<<__FUNCTION__ << __LINE__;
        break;
    case RASCS_CallbackComplete:       // Windows 95 only
        qDebug()<<__FUNCTION__ << __LINE__;
        break;
    case RASCS_LogonNetwork:           // Windows 95 only
        qDebug()<<__FUNCTION__ << __LINE__;
        break;
#endif
    case RASCS_SubEntryConnected:
        qDebug()<<__FUNCTION__ << __LINE__;
        break;
    case RASCS_SubEntryDisconnected:
        qDebug()<<__FUNCTION__ << __LINE__;
        break;
        //PAUSED STATES:
    case RASCS_Interactive:
        qDebug()<<__FUNCTION__ << __LINE__;
        break;
    case RASCS_RetryAuthentication:
        qDebug()<<__FUNCTION__ << __LINE__;
        break;
    case RASCS_CallbackSetByCaller:
        qDebug()<<__FUNCTION__ << __LINE__;
        break;
    case RASCS_PasswordExpired:
        setConnectionState(Vpn::ConnectionState::Error);
        qDebug()<<__FUNCTION__ << __LINE__;
        break;

    case RASCS_Connected: // = RASCS_DONE:
        setConnectionState(Vpn::ConnectionState::Connected);
        //qDebug()<<__FUNCTION__ << __LINE__;
        break;
    case RASCS_Disconnected:
        setConnectionState(Vpn::ConnectionState::Disconnected);
        //qDebug()<<__FUNCTION__ << __LINE__;
        break;
    default:
        //qDebug()<<__FUNCTION__ << __LINE__;
        break;
    }
}

void Ikev2Protocol::readIkev2Configuration(const QJsonObject &configuration)
{
    m_config = configuration.value(ProtocolUtils::key_proto_config_data(Proto::Ikev2)).toObject();
    const QString nativeConfig = m_config.value(configKey::config).toString();
    if (!nativeConfig.isEmpty()) {
        const QJsonDocument parsed = QJsonDocument::fromJson(nativeConfig.toUtf8());
        if (parsed.isObject()) {
            m_config = parsed.object();
        }
    }
}

ErrorCode Ikev2Protocol::start()
{
    const bool eapCredentials = m_config.value("eap").toBool()
        || (m_config[configKey::cert].toString().isEmpty() && !m_config[configKey::userName].toString().isEmpty());
    if (eapCredentials) {
        setConnectionState(Vpn::ConnectionState::Connecting);
        disconnect_vpn();
        delete_vpn_connection(tunnelName());
        if (!create_eap_vpn(tunnelName(), m_config[configKey::hostName].toString())) {
            setLastError(ErrorCode::InternalError);
            return ErrorCode::InternalError;
        }
        if (!connect_to_vpn(tunnelName())) {
            setLastError(ErrorCode::InternalError);
            return ErrorCode::InternalError;
        }
        return ErrorCode::NoError;
    }

    QByteArray cert = QByteArray::fromBase64(m_config[configKey::cert].toString().toUtf8());
    setConnectionState(Vpn::ConnectionState::Connecting);

    QTemporaryFile * certFile = new QTemporaryFile;
    certFile->setAutoRemove(false);
    certFile->open();
    QString m_filename = certFile->fileName();
    certFile->write(cert);
    certFile->close();
    delete certFile;

    {
        auto certInstallProcess = IpcClient::CreatePrivilegedProcess();

        if (!certInstallProcess) {
            setLastError(ErrorCode::AmneziaServiceConnectionFailed);
            return ErrorCode::AmneziaServiceConnectionFailed;
        }

        certInstallProcess->waitForSource();
        if (!certInstallProcess->isInitialized()) {
            qWarning() << "IpcProcess replica is not connected!";
            setLastError(ErrorCode::AmneziaServiceConnectionFailed);
            return ErrorCode::AmneziaServiceConnectionFailed;
        }
        certInstallProcess->setProgram(PermittedProcess::CertUtil);

        QStringList arguments({"-f", "-importpfx", "-p", m_config[configKey::password].toString(),
            QDir::toNativeSeparators(m_filename), "NoExport"
        });

        certInstallProcess->setArguments(arguments);
        certInstallProcess->start();
    }
    // /*
    {
        if ( disconnect_vpn()){
            qDebug()<<"VPN was disconnected";
        }
        if ( delete_vpn_connection (tunnelName())){
            qDebug()<<"VPN was deleted";
        }
    }

    {
     {
      if ( !create_new_vpn(tunnelName(), m_config[configKey::hostName].toString())){
                                                                                    qDebug() <<"Can't create the VPN connect";
}
}
}

{
    QProcess adapterConfigProcess;
    adapterConfigProcess.setProgram("powershell");
    QString arguments = QString("-command \"Set-VpnConnectionIPsecConfiguration\" "
                                "-ConnectionName '%1' "
                                "-AuthenticationTransformConstants GCMAES128 "
                                "-CipherTransformConstants GCMAES128 "
                                "-EncryptionMethod AES256 "
                                "-IntegrityCheckMethod SHA256 "
                                "-PfsGroup PFS2048 "
                                "-DHGroup Group14 "
                                "-PassThru -Force\"")
                            .arg(tunnelName());

    adapterConfigProcess.setNativeArguments(arguments);

    adapterConfigProcess.start();
    adapterConfigProcess.waitForFinished(5000);
}
//*/
{
    if (!connect_to_vpn(tunnelName())) {
        qDebug()<<"We can't connect to VPN";
    }
}
//setConnectionState(Connecting);
return ErrorCode::NoError;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool Ikev2Protocol::create_new_vpn(const QString & vpn_name,
                                   const QString & serv_addr){

    if ( RasValidateEntryName(nullptr, vpn_name.toStdWString().c_str()) != ERROR_SUCCESS)
        return false;
    DWORD size = 0;
    ::RasGetEntryProperties(nullptr, L"", nullptr, &size, nullptr, nullptr);
    LPRASENTRY pras = static_cast<LPRASENTRY>(malloc(size));
    memset(pras, 0, size);
    pras->dwSize = size;
    pras->dwType = RASET_Vpn;
    pras->dwRedialCount = 1;
    pras->dwRedialPause = 60;
    pras->dwfNetProtocols =  RASNP_Ip|RASNP_Ipv6;
    pras->dwEncryptionType = ET_RequireMax;
    wcscpy_s(pras->szLocalPhoneNumber, serv_addr.toStdWString().c_str());
    wcscpy_s(pras->szDeviceType, RASDT_Vpn);
    pras->dwfOptions = RASEO_RemoteDefaultGateway;
    pras->dwfOptions |= RASEO_RequireDataEncryption;
    pras->dwfOptions2 |= RASEO2_RequireMachineCertificates;
    pras->dwVpnStrategy = VS_Ikev2Only;
    const auto nRet = ::RasSetEntryProperties(nullptr, vpn_name.toStdWString().c_str(), pras, pras->dwSize, NULL, 0);
    free(pras);
    if (nRet == ERROR_SUCCESS)
        return true;
    return false;
}

bool Ikev2Protocol::create_eap_vpn(const QString &vpn_name, const QString &serverAddress)
{
    if (vpn_name.isEmpty() || serverAddress.isEmpty()) {
        return false;
    }

    // Our managed IKEv2 servers authenticate with EAP-MSCHAPv2, not an
    // individual P12 certificate. The server certificate must already be
    // trusted by Windows; the client never weakens certificate validation.
    const QString script = QStringLiteral(R"(
$ErrorActionPreference = 'Stop'
$name = '%1'
$server = '%2'
$eapConfig = @'
<EapHostConfig xmlns="http://www.microsoft.com/provisioning/EapHostConfig"><EapMethod><Type xmlns="http://www.microsoft.com/provisioning/EapCommon">26</Type><VendorId xmlns="http://www.microsoft.com/provisioning/EapCommon">0</VendorId><VendorType xmlns="http://www.microsoft.com/provisioning/EapCommon">0</VendorType><AuthorId xmlns="http://www.microsoft.com/provisioning/EapCommon">0</AuthorId></EapMethod><Config xmlns="http://www.microsoft.com/provisioning/EapHostConfig"><Eap xmlns="http://www.microsoft.com/provisioning/BaseEapConnectionPropertiesV1"><Type>26</Type><EapType xmlns="http://www.microsoft.com/provisioning/MsChapV2ConnectionPropertiesV1"><UseWinLogonCredentials>false</UseWinLogonCredentials></EapType></Eap></Config></EapHostConfig>
'@
Remove-VpnConnection -Name $name -Force -ErrorAction SilentlyContinue
Add-VpnConnection -Name $name -ServerAddress $server -TunnelType IKEv2 -EncryptionLevel Maximum -AuthenticationMethod Eap -EapConfigXmlStream $eapConfig -RememberCredential -Force | Out-Null
Set-VpnConnectionIPsecConfiguration -ConnectionName $name -AuthenticationTransformConstants SHA256128 -CipherTransformConstants AES256 -EncryptionMethod AES256 -IntegrityCheckMethod SHA256 -DHGroup Group14 -PfsGroup PFS2048 -Force | Out-Null
)").arg(psQuoted(vpn_name), psQuoted(serverAddress));

    QProcess powershell;
    powershell.start("powershell.exe", {"-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-Command", script});
    return powershell.waitForFinished(15000) && powershell.exitStatus() == QProcess::NormalExit && powershell.exitCode() == 0;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool Ikev2Protocol::delete_vpn_connection(const QString &vpn_name){

    if ( RasDeleteEntry(nullptr, vpn_name.toStdWString().c_str()) == ERROR_SUCCESS){
        return true;
    }
    return false;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool Ikev2Protocol::connect_to_vpn(const QString & vpn_name){
    RASDIALPARAMS RasDialParams;
    memset(&RasDialParams, 0x0, sizeof(RASDIALPARAMS));
    RasDialParams.dwSize = sizeof(RASDIALPARAMS);
    wcscpy_s(RasDialParams.szEntryName, vpn_name.toStdWString().c_str());
    const QString username = m_config[configKey::userName].toString();
    const QString password = m_config[configKey::password].toString();
    if (!username.isEmpty()) {
        wcscpy_s(RasDialParams.szUserName, username.toStdWString().c_str());
    }
    if (!password.isEmpty()) {
        wcscpy_s(RasDialParams.szPassword, password.toStdWString().c_str());
    }
    auto ret = RasDial(NULL, NULL, &RasDialParams, 0,
                       &RasDialFuncCallback,
                       &hRasConn);

    if (ret == ERROR_SUCCESS){
        return true;
    }
    return false;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool Ikev2Protocol::disconnect_vpn(){
    if ( hRasConn != nullptr ){
        auto ret = RasHangUp(hRasConn);
        qDebug() << "RasHangUp " << ret;
        if (ret != ERROR_SUCCESS)
            return false;
    }
    QThread::msleep(3000);

    return true;
}

void WINAPI RasDialFuncCallback(UINT unMsg,
                                RASCONNSTATE rasconnstate,
                                DWORD dwError ){
    std::lock_guard<std::mutex> guard(rasDialFuncMutex);
    if (self) {
        self->newConnectionStateEventReceived(unMsg, rasconnstate, dwError);
    }
}
