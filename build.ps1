param([switch]$TestsOnly, [string]$ZigPath)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Set-Location $PSScriptRoot
$env:ZIG_GLOBAL_CACHE_DIR = Join-Path $PSScriptRoot '.cache/zig'
if ($ZigPath) { $zig = (Resolve-Path -LiteralPath $ZigPath).Path }
elseif (Get-Command zig -ErrorAction SilentlyContinue) { $zig = (Get-Command zig).Source }
else {
    $zig = Join-Path $root '.tools/zig-x86_64-windows-0.15.2/zig.exe'
    if (!(Test-Path -LiteralPath $zig)) { throw 'Install Zig 0.15.2 and add it to PATH, or pass -ZigPath.' }
}
New-Item -ItemType Directory -Path build -Force | Out-Null
& $zig c++ -std=c++17 -Wall -Wextra -Werror tests/encounter_test.cpp -o build/encounter_test.exe
if ($LASTEXITCODE) { throw 'Encounter test compilation failed' }
& .\build\encounter_test.exe
if ($LASTEXITCODE) { throw 'Encounter tests failed' }
& $zig c++ -std=c++17 -Wall -Wextra -Werror tests/controls_test.cpp -o build/controls_test.exe
if ($LASTEXITCODE) { throw 'Controls test compilation failed' }
& .\build\controls_test.exe
if ($LASTEXITCODE) { throw 'Controls tests failed' }
& $zig c++ -std=c++17 -Wall -Wextra -Werror tests/battle_policy_test.cpp -o build/battle_policy_test.exe
if ($LASTEXITCODE) { throw 'Battle policy test compilation failed' }
& .\build\battle_policy_test.exe
if ($LASTEXITCODE) { throw 'Battle policy tests failed' }
& $zig c++ -std=c++17 -Wall -Wextra -Werror tests/policy_test.cpp -o build/policy_test.exe
if ($LASTEXITCODE) { throw 'Speed test compilation failed' }
& .\build\policy_test.exe
if ($LASTEXITCODE) { throw 'Speed policy tests failed' }
& $zig c++ -std=c++17 -Wall -Wextra -Werror tests/adapter_test.cpp -luser32 -lgdi32 -lbcrypt -o build/adapter_test.exe
if ($LASTEXITCODE) { throw 'Speed adapter test compilation failed' }
& .\build\adapter_test.exe
if ($LASTEXITCODE) { throw 'Speed adapter tests failed' }
& $zig c++ -std=c++17 -Wall -Wextra -Werror tests/game_hud_test.cpp -luser32 -lgdi32 -lbcrypt -o build/game_hud_test.exe
if ($LASTEXITCODE) { throw 'Game HUD test compilation failed' }
& .\build\game_hud_test.exe
if ($LASTEXITCODE) { throw 'Game HUD tests failed' }
& $zig c++ -std=c++17 -Wall -Wextra -Werror tests/auto_attack_test.cpp -luser32 -lgdi32 -lbcrypt -o build/auto_attack_test.exe
if ($LASTEXITCODE) { throw 'Auto Attack test compilation failed' }
& .\build\auto_attack_test.exe
if ($LASTEXITCODE) { throw 'Auto Attack tests failed' }
& $zig c++ -std=c++17 -Wall -Wextra -Werror tests/limit_burst_test.cpp -luser32 -lgdi32 -lbcrypt -o build/limit_burst_test.exe
if ($LASTEXITCODE) { throw 'Limit Burst test compilation failed' }
& .\build\limit_burst_test.exe
if ($LASTEXITCODE) { throw 'Limit Burst tests failed' }
& $zig c++ -std=c++17 -Wall -Wextra -Werror tests/battle_resources_test.cpp -luser32 -lgdi32 -lbcrypt -o build/battle_resources_test.exe
if ($LASTEXITCODE) { throw 'Battle resource test compilation failed' }
& .\build\battle_resources_test.exe
if ($LASTEXITCODE) { throw 'Battle resource tests failed' }
& $zig c++ -std=c++17 -Wall -Wextra -Werror tests/cutscene_test.cpp -luser32 -lgdi32 -lbcrypt -o build/cutscene_test.exe
if ($LASTEXITCODE) { throw 'Cutscene test compilation failed' }
& .\build\cutscene_test.exe
if ($LASTEXITCODE) { throw 'Cutscene reflection tests failed' }
& $zig c++ -std=c++17 -Wall -Wextra -Werror tests/indicator_test.cpp -luser32 -lgdi32 -lbcrypt -o build/indicator_test.exe
if ($LASTEXITCODE) { throw 'Indicator test compilation failed' }
& .\build\indicator_test.exe
if ($LASTEXITCODE) { throw 'Indicator tests failed' }
& $zig c++ -std=c++17 -Wall -Wextra -Werror tests/native_indicator_test.cpp -luser32 -lgdi32 -lbcrypt -o build/native_indicator_test.exe
if ($LASTEXITCODE) { throw 'Native indicator test compilation failed' }
& .\build\native_indicator_test.exe
if ($LASTEXITCODE) { throw 'Native indicator tests failed' }
& $zig c++ -std=c++17 -Wall -Wextra -Werror tests/icon_png_test.cpp -o build/icon_png_test.exe
if ($LASTEXITCODE) { throw 'Icon PNG test compilation failed' }
& .\build\icon_png_test.exe
if ($LASTEXITCODE) { throw 'Icon PNG tests failed' }
& $zig c++ -std=c++17 -Wall -Wextra -Werror tests/screen_visibility_test.cpp -luser32 -lgdi32 -lbcrypt -o build/screen_visibility_test.exe
if ($LASTEXITCODE) { throw 'Screen visibility test compilation failed' }
& .\build\screen_visibility_test.exe
if ($LASTEXITCODE) { throw 'Screen visibility tests failed' }
foreach ($profile in @('screen_visibility','auto_attack','game_hud')) {
    & $zig c++ -std=c++17 -O2 -UNDEBUG -DPROFILE_SCREEN -Wall -Wextra -Werror "tests/${profile}_test.cpp" -luser32 -lgdi32 -lbcrypt -o "build/${profile}_profile.exe"
    if ($LASTEXITCODE) { throw "Performance check compilation failed: $profile" }
    & ".\build\${profile}_profile.exe"
    if ($LASTEXITCODE) { throw "Performance checks failed: $profile" }
}
if (!$TestsOnly) {
    & $zig c++ -target x86_64-windows-gnu -std=c++17 -O2 -Wall -Wextra -Werror -shared src/plugin.cpp -luser32 -lgdi32 -lbcrypt -o build/resonance_boosters.asi
    if ($LASTEXITCODE) { throw 'Speed plugin compilation failed' }
}
