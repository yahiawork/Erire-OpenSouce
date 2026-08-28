$ErrorActionPreference = "Stop"

$toolRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $toolRoot "..\..")
$envPath = Join-Path $repoRoot "Site\.env"
$localConfig = Join-Path $toolRoot "build\erire.local.installer.ini"
$payloadName = "erire_payload_" + [DateTime]::UtcNow.ToString("yyyyMMddHHmmss")
$payload = Join-Path $repoRoot ("release\" + $payloadName)

function Read-DotEnv($path) {
    $values = @{}
    if (!(Test-Path -LiteralPath $path)) {
        throw ".env not found at $path"
    }
    foreach ($line in Get-Content -LiteralPath $path) {
        $trimmed = $line.Trim()
        if (!$trimmed -or $trimmed.StartsWith("#") -or !$trimmed.Contains("=")) {
            continue
        }
        $parts = $trimmed.Split("=", 2)
        $key = $parts[0].Trim()
        $value = $parts[1].Trim()
        if (($value.StartsWith('"') -and $value.EndsWith('"')) -or ($value.StartsWith("'") -and $value.EndsWith("'"))) {
            $value = $value.Substring(1, $value.Length - 2)
        }
        $values[$key] = $value
    }
    return $values
}

function Copy-DirectoryFresh($source, $destination) {
    if (!(Test-Path -LiteralPath $source)) {
        Write-Warning "Skipping missing directory: $source"
        return
    }
    if (Test-Path -LiteralPath $destination) {
        Get-ChildItem -LiteralPath $destination -Force -Recurse -ErrorAction SilentlyContinue | ForEach-Object {
            try { $_.Attributes = 'Normal' } catch { }
        }
        try {
            Remove-Item -LiteralPath $destination -Recurse -Force -ErrorAction Stop
        } catch {
            [System.IO.Directory]::Delete($destination, $true)
        }
    }
    Copy-Item -Path $source -Destination $destination -Recurse -Force
}

$envValues = Read-DotEnv $envPath
$appName = if ($envValues["PRODUCT_NAME"]) { $envValues["PRODUCT_NAME"] } else { "Erire Core" }
$apiKey = if ($envValues["ACTIVATION_API_TOKEN"]) { $envValues["ACTIVATION_API_TOKEN"] } else { "" }
$activationUrl = if ($envValues["INSTALLER_ACTIVATION_URL"]) {
    $envValues["INSTALLER_ACTIVATION_URL"]
} elseif ($envValues["PUBLIC_BASE_URL"]) {
    $envValues["PUBLIC_BASE_URL"].TrimEnd("/") + "/api/activate"
} else {
    "https://erire.pythonanywhere.com/api/activate"
}

New-Item -ItemType Directory -Force -Path (Join-Path $toolRoot "build") | Out-Null
New-Item -ItemType Directory -Force -Path $payload | Out-Null

Copy-Item -Path (Join-Path $repoRoot "ErireStudio.exe") -Destination $payload -Force
Copy-Item -Path (Join-Path $repoRoot "ErireRunner.exe") -Destination $payload -Force
Copy-Item -Path (Join-Path $repoRoot "erire.exe") -Destination $payload -Force
if (Test-Path -LiteralPath (Join-Path $repoRoot "README.md")) {
    Copy-Item -Path (Join-Path $repoRoot "README.md") -Destination $payload -Force
} elseif (Test-Path -LiteralPath (Join-Path $repoRoot "app\README.md")) {
    Copy-Item -Path (Join-Path $repoRoot "app\README.md") -Destination (Join-Path $payload "README.md") -Force
}
Copy-DirectoryFresh (Join-Path $repoRoot "assets") (Join-Path $payload "assets")
Copy-DirectoryFresh (Join-Path $repoRoot "docs") (Join-Path $payload "docs")
Copy-DirectoryFresh (Join-Path $repoRoot "studio\frontend") (Join-Path $payload "studio\frontend")
Copy-DirectoryFresh (Join-Path $repoRoot "webview2") (Join-Path $payload "webview2")

@"
app_name=$appName
app_version=1.0.4
publisher=FirstStandStudio
source_dir=..\..\release\$payloadName
output=..\..\release\ErireCore-FirstStand-Setup.exe
install_dir={localappdata}\Programs\{app_name}
main_exe=ErireStudio.exe
icon=..\..\assets\brand\erire-logo.ico
gcc=gcc
windres=windres
bash=C:\msys64\usr\bin\bash.exe
use_msys_bash=true
stub_source=src\firststand_stub.c
require_key=true
activation_url=$activationUrl
api_key=$apiKey
google_required=true
desktop_shortcut=true
start_menu_shortcut=true
run_after_install=false
"@ | Set-Content -LiteralPath $localConfig -Encoding ASCII

& (Join-Path $toolRoot "build.ps1")
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Push-Location $toolRoot
try {
    & ".\bin\FirstStand-Installer.exe" "build\erire.local.installer.ini"
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
} finally {
    Pop-Location
}

$installer = Join-Path $repoRoot "release\ErireCore-FirstStand-Setup.exe"
$webInstaller = Join-Path $repoRoot "app\static\downloads\ErireInstaller.exe"
$compressScript = Join-Path $toolRoot "compress_download_installer.ps1"
if (Test-Path -LiteralPath $compressScript) {
    & $compressScript -InputInstaller $installer -OutputInstaller $webInstaller
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
} else {
    Copy-Item -Path $installer -Destination $webInstaller -Force
}

Write-Host "Built installer from app\.env"
Write-Host "Activation URL: $activationUrl"
Write-Host "API key included: $([bool]$apiKey)"
Write-Host "Installer: $installer"
Write-Host "Web download copy: $webInstaller"
