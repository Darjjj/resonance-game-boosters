param([string]$GameRoot = 'C:\Steam\steamapps\common\FINAL FANTASY RESONANCE')
$ErrorActionPreference = 'Stop'
$bin = Join-Path $GameRoot 'FFRS\Binaries\Win64'
$exe = Join-Path $bin 'FFRS-Win64-Shipping.exe'
if ((Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash -ne 'B8C588E46EBADBE37FECA7BDF2DA0AE63A8538099D14A53EC0C2241B086E5FFA') {
    throw 'Unsupported executable; nothing installed.'
}
if (Get-Process -Name FFRS-Win64-Shipping -ErrorAction SilentlyContinue) { throw 'Close the game before installing.' }
if (!(Test-Path -LiteralPath (Join-Path $bin 'winmm.dll'))) { throw 'Existing ASI loader required; no loader will be installed.' }
$source = Join-Path $PSScriptRoot 'build\resonance_boosters.asi'
if (!(Test-Path -LiteralPath $source)) { throw 'Run speedmod/build.ps1 first.' }
$scripts = Join-Path $bin 'scripts'
if (!(Test-Path -LiteralPath $scripts)) { throw 'Existing scripts folder required.' }
$backup = Join-Path $PSScriptRoot ('backups\' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Path $backup | Out-Null
Copy-Item -LiteralPath (Join-Path $bin 'winmm.dll') -Destination $backup
Copy-Item -LiteralPath $scripts -Destination (Join-Path $backup 'scripts') -Recurse
$userData = Join-Path $env:LOCALAPPDATA 'FFRS'
foreach ($relative in @('Steam', 'Saved\Config')) {
    $data = Join-Path $userData $relative
    if (Test-Path -LiteralPath $data) {
        $destination = Join-Path $backup ('UserData\' + $relative)
        New-Item -ItemType Directory -Path (Split-Path $destination) -Force | Out-Null
        Copy-Item -LiteralPath $data -Destination $destination -Recurse
    }
}
$before = @(Get-ChildItem -LiteralPath $scripts -File | Get-FileHash -Algorithm SHA256)
$before | Select-Object Path,Hash | ConvertTo-Json | Set-Content (Join-Path $backup 'mod-hashes.json')
Copy-Item -LiteralPath $source -Destination (Join-Path $scripts 'resonance_boosters.asi')
# The old name must not load alongside the renamed plugin. It is already backed up.
$legacyPlugin = Join-Path $scripts 'resonance_speed.asi'
if (Test-Path -LiteralPath $legacyPlugin) { Remove-Item -LiteralPath $legacyPlugin }
$config = Join-Path $scripts 'resonance_boosters.ini'
$legacyConfig = Join-Path $scripts 'resonance_speed.ini'
if (!(Test-Path -LiteralPath $config) -and (Test-Path -LiteralPath $legacyConfig)) {
    Copy-Item -LiteralPath $legacyConfig -Destination $config
}
# Both legacy files are backed up before migration.
if (Test-Path -LiteralPath $legacyConfig) { Remove-Item -LiteralPath $legacyConfig }
$legacyLog = Join-Path $scripts 'resonance_speed.log'
$newLog = Join-Path $scripts 'resonance_boosters.log'
if (Test-Path -LiteralPath $legacyLog) {
    if (!(Test-Path -LiteralPath $newLog)) { Copy-Item -LiteralPath $legacyLog -Destination $newLog }
    Remove-Item -LiteralPath $legacyLog
}
if (!(Test-Path -LiteralPath $config)) {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'resonance_boosters.ini') -Destination $config
} else {
    # Add the new controls without discarding existing user preferences.
    $configText = Get-Content -LiteralPath $config -Raw
    $version = [regex]::Match($configText, '(?mi)^\s*ControlsVersion\s*=\s*(\d+)')
    if (!$version.Success -or [int]$version.Groups[1].Value -lt 5) {
        foreach ($entry in ([ordered]@{ControlsVersion=5; CycleKey=112; BattleBoostToggleKey=113; EncounterToggleKey=114; AutoAttackToggleKey=115; HudToggleKey=116}).GetEnumerator()) {
            $pattern = '(?mi)^\s*' + $entry.Key + '\s*=.*$'
            $line = $entry.Key + '=' + $entry.Value
            if ([regex]::IsMatch($configText, $pattern)) { $configText = [regex]::Replace($configText, $pattern, $line) }
            else { $configText = $configText.TrimEnd() + [Environment]::NewLine + $line + [Environment]::NewLine }
        }
        Set-Content -LiteralPath $config -Value $configText
    }
    # Move former defaults below both the objective banner and battle portrait.
    $layoutVersion = [regex]::Match($configText, '(?mi)^\s*IndicatorLayoutVersion\s*=\s*(\d+)')
    if (!$layoutVersion.Success -or [int]$layoutVersion.Groups[1].Value -lt 2) {
        $configText = [regex]::Replace($configText, '(?mi)^\s*IndicatorY\s*=\s*(?:20|160)\s*$', 'IndicatorY=240')
        if ($layoutVersion.Success) {
            $configText = [regex]::Replace($configText, '(?mi)^\s*IndicatorLayoutVersion\s*=.*$', 'IndicatorLayoutVersion=2')
        } else {
            $configText = $configText.TrimEnd() + [Environment]::NewLine + 'IndicatorLayoutVersion=2' + [Environment]::NewLine
        }
        Set-Content -LiteralPath $config -Value $configText
    }
    $defaults = [ordered]@{ControlsVersion=5; CycleKey=112; BattleBoostToggleKey=113; EncounterToggleKey=114; AutoAttackToggleKey=115; HudToggleKey=116; EncounterMode=0; BattleBoostEnabled=0; AutoAttackEnabled=0; ShowIndicator=1; IndicatorX=20; IndicatorY=240; IndicatorLayoutVersion=2}
    foreach ($entry in $defaults.GetEnumerator()) {
        if ($configText -notmatch ('(?mi)^\s*' + $entry.Key + '\s*=')) {
            Add-Content -LiteralPath $config -Value ($entry.Key + '=' + $entry.Value)
        }
    }
}
foreach ($old in $before) {
    if ([IO.Path]::GetFileName($old.Path) -in @('resonance_boosters.asi','resonance_speed.asi','resonance_speed.ini','resonance_boosters.ini','resonance_speed.log','resonance_boosters.log')) { continue }
    if ((Get-FileHash -LiteralPath $old.Path -Algorithm SHA256).Hash -ne $old.Hash) {
        throw ('Another mod changed during installation: ' + $old.Path)
    }
}
Write-Output ('Boosters mod installed; other mod hashes unchanged. Backup: ' + $backup)
