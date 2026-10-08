sc stop AmneziaWGTunnel$AmneziaVPN
sc delete AmneziaWGTunnel$AmneziaVPN
sc stop TotalVPN-service
sc delete TotalVPN-service
taskkill /IM "TotalVPN-service.exe" /F
taskkill /IM "TotalVPN.exe" /F
taskkill /IM "PowVPN-service.exe" /F
taskkill /IM "PowVPN.exe" /F
taskkill /IM "PowVPN-worker.exe" /F
exit /b 0
