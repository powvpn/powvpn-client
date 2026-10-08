set PowVPNPath=%~dp0
echo %PowVPNPath%

rem Define directories for logs
set "ORG_DIR=%AppData%\PowVPN.ORG"
set "USER_APP_DIR=%ORG_DIR%\PowVPN"
set "USER_LOG_DIR=%USER_APP_DIR%\log"
set "SYS_APP_DIR=%ProgramData%\PowVPN"
set "SYS_LOG_DIR=%SYS_APP_DIR%\log"
set "SYS_LOG_FILE=%SYS_LOG_DIR%\PowVPN-service.log"

timeout /t 1
sc stop PowVPN-service
sc delete PowVPN-service
sc stop AmneziaWGTunnel$AmneziaVPN
sc delete AmneziaWGTunnel$AmneziaVPN
taskkill /IM "PowVPN-service.exe" /F
taskkill /IM "PowVPN.exe" /F
taskkill /IM "PowVPN-worker.exe" /F

rem Delete the service log file under ProgramData
if exist "%SYS_LOG_FILE%" del /F /Q "%SYS_LOG_FILE%"
if exist "%SYS_LOG_DIR%" rmdir /S /Q "%SYS_LOG_DIR%"
rem Try to remove application dir if empty
rd "%SYS_APP_DIR%" 2>nul

rem Delete client logs under current user's AppData\Roaming (Organization\Application)
if exist "%USER_LOG_DIR%" rmdir /S /Q "%USER_LOG_DIR%"
rem Try to remove app and org directories if empty
rd "%USER_APP_DIR%" 2>nul
rd "%ORG_DIR%" 2>nul

exit /b 0
