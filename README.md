# Nova Drift Co-op

Experimental two-ship co-op for **Nova Drift 1.2.38.4**, Steam build **25890040**,
Windows x64 (GameMaker YYC). Both ships share the arena and XP. Each keeps its
own weapon, body, shield, augments, rerolls and upgrade choices. Each level gives
each player a choice they can bank and use during combat. Both personal upgrade
trays can be open at once; the teammate keeps playing.

This repository contains the mod's source and tools. Bring your own matching
Steam installation; game files, saves, dependencies and compiled binaries are
excluded from Git.

## Launch an installed copy

Open `%USERPROFILE%\Games\NovaDriftCoop\PlayCoop.cmd` for local play, or
`PlayRemoteCoop.cmd` for Steam Remote Play Together. Start a **fresh run**;
two-player save/resume is unsupported. The original Steam installation and
original save profile stay separate.

Normal launches request the game's native fullscreen mode. For a local window,
run `PlayCoop.cmd windowed`. `PlayCoop.cmd diagnostics` enables diagnostic logging.
The project's root shortcuts use the installed copy, with an optional
`NOVACOOP_INSTALL` override; installation-specific paths stay in ignored `local/`.

The Steam entry is **Spacewar (480)**. Nova Drift's own Steam entry does not
advertise Remote Play Together. Your friends will see Spacewar while this
experimental wrapper hosts Nova Drift. See [Steam setup](README.RemotePlay.md)
for installation, invitations and restoring the original Launch Options.

## Co-op balance

The default preset adds **44% more arena area**, lets hostile AI choose either
living ship, and increases supported regular enemy groups by roughly **60%**.
Ordinary pickup XP is **70%** of native value while both ships live; XP-earned
levels still give each player a separate choice. Trait bonus levels stay with
their buyer. Native boss order and wave timers are preserved.

Edit `runtime\coop-balance.ini` and restart to tune the preset. See
[balance details and coverage](docs/BALANCE.md).

## Reviving and zoom

The arena eases between the single-ship and two-ship view over 0.85 seconds.
A downed ship leaves a beacon in its color. After five gameplay seconds, stay
within 145 world units for three seconds to revive it. Progress fades when the
teammate moves away and freezes during pause. The revived ship keeps its build,
returns with half its effective hull and an empty shield, and receives the
native 180-tick protection timer. Both ships dying ends the run normally.
F8 can join P2 before automatic joining; it no longer gives a free redeploy.

## Controls

**P1, red:** mouse aims; left button thrusts; right button fires. These are the
copied game's mouse bindings. **P2, gold:** keyboard controls below. Directional
steering turns toward the pressed direction and retains native momentum.

| P2 action | Default |
|---|---|
| Up / left / down / right | I / J / K / L |
| Fire / confirm upgrade | N |
| Stabilize / back in upgrade menu | Right Shift |
| Blink | Right Ctrl |
| Deploy selected construct / reroll upgrade | U |
| Formation | O |
| Construct slots 1–5 | 7 / 8 / 9 / 0 / minus |
| Banish upgrade | 8 |
| Strafe left / right | Semicolon / apostrophe |
| Strafe forward / back | Left / right bracket |
| Strafe modifier | Right Alt |
| Open / close personal upgrades | F7 (controller: Back) |
| Configure P2 | F6 |

F6 pauses play and opens the control editor. `ConfigureP2.cmd` opens it separately.
Choose directional/tank steering, rebind keys or configure controller mappings.
Settings live in `runtime\p2-controls.ini`. P2 keyboard bindings take priority
over conflicting P1 keyboard alternatives; P1's mouse remains available.
Physical controller hardware has not been tested.

Both HUDs use native health, shield and status rendering with red/gold accents.
Control hints fade; no numeric health or defense values are added. Upgrades use
native offers and artwork in small red/gold hex strips beside the HUD. Initial weapon,
shield and body selection still uses the native screen. During combat, P1 uses
middle mouse to open/close a strip, the wheel to browse and left click to choose.
Hover a hex to inspect its full native description.
P2 uses F7, movement keys to browse and N to choose; hold O (the configurable
Formation binding) to inspect, and use Right Shift to close. Native description
backgrounds, selection frames, icons, fonts and formatted stats are reused.
Full descriptions stay hidden until hover or Inspect.
Each tray has reroll and Researcher banish controls. Closing preserves offers
and does not spend a choice or reroll. Choosing consumes only that ship's input;
the other ship keeps moving and fighting. Neither player gains immunity while
choosing. Escape remains a shared pause.

## Build and install from source

The current build workflow runs from **WSL on Windows**, with Windows interop:

- Python 3.8+ and Git in WSL.
- MSYS2 UCRT64 `g++.exe` at `C:\msys64\ucrt64\bin\g++.exe`.
- Visual Studio 2022 Build Tools, x64 C++ tools, Windows SDK and C++ runtime,
  installed at the default BuildTools path used by `tools/build_ship.cmd`.
- Your supported, pristine Steam copy of Nova Drift. Other game builds are rejected.

Clone this private repository, then from its WSL directory run:

```sh
python3 tools/setup_project.py \
  --game-dir '/mnt/g/SteamLibrary/steamapps/common/Nova Drift'
```

Adjust the path to your Steam library. This copies the game into ignored `game/`,
records a private original-save baseline, fetches ten hash-pinned upstream files,
builds the mod/launchers and stages an owned Windows-local installation. It does
not install toolchains or change Steam settings. `--base /mnt/c/path/to/new-folder`
selects another destination; existing folders require the ownership marker.
Close the co-op game and helper before running setup or staging an update.

The isolated profile initially copies your native preferences. Co-op run writes
are suppressed because the original continuation format stores one player.
The launcher redirects profile paths and blocks game Steam API initialization;
Steam's outer launch chain supplies overlay and Remote Play.

For individual rebuild/staging commands and verification see
[development](docs/DEVELOPMENT.md). Dependency pins and upstream licensing are
listed in [dependencies.json](dependencies.json) and
[third-party notes](docs/THIRD_PARTY.md).

## Limits and troubleshooting

This is shared-screen co-op streamed through Steam, with one authoritative game
process. Late-joining P2 starts from P1's build and then develops independently.
Every late-game augment combination has not been tested. Remote Play merges
keyboard input, so grant the guest keyboard access and leave guest mouse off.
Keep the game focused while playing.

If `SteamCoop.exe` is missing, the Steam shortcut reports it. Defender has
previously quarantined this experimental helper; another deletion can block
launching. The project does not change Windows Security settings. The relevant
source is [src/steam_coop.cpp](src/steam_coop.cpp); build and review it locally.
Local `PlayCoop.cmd` uses `novacoop_launch.exe` directly.

For tested behavior and remaining gaps see [validation](docs/VALIDATION.md).
The earlier ally experiment is retained in source for research; the supported
launch path described here is the two-native-ship version.
