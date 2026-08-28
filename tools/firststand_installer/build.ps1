$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$src = "src\firststand_builder.c"
$outDir = Join-Path $root "bin"
$out = "bin\FirstStand-Installer.exe"

New-Item -ItemType Directory -Force -Path $outDir | Out-Null

$bash = if ($env:MSYS_BASH) { $env:MSYS_BASH } else { "C:\msys64\usr\bin\bash.exe" }
$msysRoot = $root -replace '\\','/'
if ($msysRoot -match '^([A-Za-z]):/(.*)$') {
    $msysRoot = '/' + $Matches[1].ToLowerInvariant() + '/' + $Matches[2]
}
$sizeFlags = "-Os -ffunction-sections -fdata-sections -Wl,--gc-sections -s"
$command = "export PATH=/ucrt64/bin:/usr/bin:`$PATH; cd '$msysRoot' && gcc -std=c11 $sizeFlags -Wall -Wextra -Wno-format-truncation -o bin/FirstStand-Installer.exe src/firststand_builder.c"
if (Test-Path $bash) {
    Push-Location $root
    try {
        & $bash -lc $command
        if ($LASTEXITCODE -ne 0) {
            exit $LASTEXITCODE
        }
    } finally {
        Pop-Location
    }
} else {
    Push-Location $root
    try {
        cmd /c "gcc -std=c11 $sizeFlags -Wall -Wextra -Wno-format-truncation -o bin\FirstStand-Installer.exe src\firststand_builder.c"
        if ($LASTEXITCODE -ne 0) {
            exit $LASTEXITCODE
        }
    } finally {
        Pop-Location
    }
}

Write-Host "Built $(Join-Path $root $out)"
