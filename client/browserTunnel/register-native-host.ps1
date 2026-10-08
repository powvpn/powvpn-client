[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[a-p]{32}$')]
    [string]$ChromeExtensionId,

    [ValidatePattern('^[^\s]+@[^\s]+$')]
    [string]$FirefoxExtensionId,

    [ValidateSet('Chrome', 'Edge', 'Firefox', 'All')]
    [string]$Browser = 'All',

    [switch]$Unregister
)

$ErrorActionPreference = 'Stop'
$hostName = 'com.powvpn.client'
$hostExe = Join-Path $PSScriptRoot 'pow-native-host.exe'
if (-not $Unregister -and -not (Test-Path -LiteralPath $hostExe)) {
    throw "Native host was not found: $hostExe"
}

$manifestRoot = Join-Path $env:LOCALAPPDATA 'PowVPN\native-messaging'
New-Item -ItemType Directory -Force -Path $manifestRoot | Out-Null

function Set-NativeHost([string]$browser, [string]$manifestPath) {
    $registryPath = switch ($browser) {
        'Chrome' { "HKCU:\Software\Google\Chrome\NativeMessagingHosts\$hostName" }
        'Edge' { "HKCU:\Software\Microsoft\Edge\NativeMessagingHosts\$hostName" }
        'Firefox' { "HKCU:\Software\Mozilla\NativeMessagingHosts\$hostName" }
    }
    if ($Unregister) {
        Remove-Item -LiteralPath $registryPath -Force -ErrorAction SilentlyContinue
        Remove-Item -LiteralPath $manifestPath -Force -ErrorAction SilentlyContinue
        return
    }
    New-Item -Path $registryPath -Force | Out-Null
    New-ItemProperty -Path $registryPath -Name '(default)' -Value $manifestPath -PropertyType String -Force | Out-Null
}

$selected = if ($Browser -eq 'All') { @('Chrome', 'Edge', 'Firefox') } else { @($Browser) }
foreach ($item in $selected) {
    if ($item -ne 'Firefox' -and [string]::IsNullOrWhiteSpace($ChromeExtensionId)) {
        throw 'Pass -ChromeExtensionId for Chrome or Edge.'
    }
    if ($item -eq 'Firefox' -and [string]::IsNullOrWhiteSpace($FirefoxExtensionId)) {
        Write-Warning 'Firefox was skipped: pass -FirefoxExtensionId for the signed Firefox add-on ID.'
        continue
    }
    $manifestPath = Join-Path $manifestRoot ("$hostName.$($item.ToLowerInvariant()).json")
    if (-not $Unregister) {
        $manifest = [ordered]@{
            name = $hostName
            description = 'Pow VPN browser tunnel bridge'
            path = $hostExe
            type = 'stdio'
        }
        if ($item -eq 'Firefox') {
            $manifest.allowed_extensions = @($FirefoxExtensionId)
        } else {
            $manifest.allowed_origins = @("chrome-extension://$ChromeExtensionId/")
        }
        $manifest | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $manifestPath -Encoding utf8NoBOM
    }
    Set-NativeHost $item $manifestPath
    Write-Host "${item}: $hostName $(if ($Unregister) {'unregistered'} else {'registered'})"
}