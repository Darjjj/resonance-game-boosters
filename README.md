# Resonance Game Boosters

Inspired by Final Fantasy X/X-2 HD Remaster's Game Boosters.

A single-player ASI mod for the Windows Steam version of Final Fantasy Resonance.

| Key | Booster | Action |
| --- | --- | --- |
| F1 | Turbo Mode | Cycle 1x, 2x and 4x, including battles |
| F2 | Supercharge | Refill party HP, MP and Limit Burst |
| F3 | Encounter Rate | Default, no random encounters, double encounters |
| F4 | Auto-Battle | Automatically use basic Attack for ready party members |
| F5 | HUD Hide/Show | Toggle the entire game HUD and booster icons |

Turbo starts at 1x. Other preferences are saved. Detected cutscenes run at 1x.
Only active icons appear, in fixed slots. Icons hide on title/pause menus,
battle results and detected cutscenes. Standard, ultrawide and exclusive
fullscreen displays are supported.

## Download and install

1. Download the ZIP from [Releases](https://github.com/Darjjj/resonance-game-boosters/releases).
2. Install [Ultimate ASI Loader x64](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases) as `winmm.dll` beside `FFRS/Binaries/Win64/FFRS-Win64-Shipping.exe`, unless you already have a working loader. Loader tested: 10.0.0.
3. Close the game. Extract the ZIP into its installation folder. The ASI and INI belong in `FFRS/Binaries/Win64/scripts/`.
4. Launch through Steam and use F1–F5.

Upgrading from a preview: remove `resonance_speed.asi` and rename your existing
`resonance_speed.ini` to `resonance_boosters.ini` before extraction. Keep your
INI when upgrading; do not load both plugin names.

Configure hotkeys and preferences in `resonance_boosters.ini`. Diagnostic log:
`resonance_boosters.log`. The legacy `[Speed]` INI section is retained so older
settings remain compatible.

To uninstall, close the game and remove this mod's ASI, INI and log. Keep the
loader if other mods need it.

## Compatibility

Built with compatibility in mind, without replacing game assets or installing
executable patches. Other mods using the same hotkeys or changing timing,
resources or HUD opacity may conflict. Hotkeys are configurable.

Supported Steam build: **24250053**. Executable SHA256:
`B8C588E46EBADBE37FECA7BDF2DA0AE63A8538099D14A53EC0C2241B086E5FFA`.
Unsupported executables are rejected before integration.

Core boosters were tested through the intro multiple times and dozens of
battles. Pause-menu hiding and exclusive fullscreen icons were confirmed
in-game. Tests also cover policies, reflection validation, stale objects,
resource refills, legal targets, HUD restoration, rendering and steady-state
performance. Not every battle mechanic or combination of mods is verified.

## Build

On Windows, install [Zig 0.15.2](https://ziglang.org/download/) and add it to PATH:

```powershell
./build.ps1
```

Or pass `-ZigPath C:/path/to/zig.exe`. Output: `build/resonance_boosters.asi`.
`./build.ps1 -TestsOnly` runs the tests without building the plugin.

The embedded badge data and reference pixels are included. Regenerating them
is optional and requires Python and Pillow:

```powershell
python -m pip install Pillow
python assets/boosters/convert.py
```

`./install.ps1` installs into the default Steam path, or accepts `-GameRoot`.
It requires the game closed and an existing loader, verifies the executable,
backs up local saves/config/mods, migrates old filenames and preserves settings.
Backups stay local and are excluded from Git.

## License and credits

Original source and badge artwork: [MIT](LICENSE). Third-party runtime notices
are retained in release ZIPs. No game binaries, extracted game assets,
generated game SDK, ASI loader, saves or personal logs are included.

Concept, direction and playtesting: FxDarj. Developed with assistance from
OpenAI Codex, including generated badge artwork. Loader: ThirteenAG.
Layout reference: [PolarWizard's Resonance fix](https://github.com/PolarWizard/final-fantasy-resonance-fix),
commit `0f5bc2c4bab2b1680c59d8d1c2135253ae14c0df`.

[Nexus listing](https://www.nexusmods.com/finalfantasyresonance/mods/65)
