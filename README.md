# Nova Drift Co-op

A two-player co-op mod for Nova Drift. Each player controls their own ship and
chooses their own upgrades. XP is shared. Supports local play and Steam Remote
Play Together.

![Two ships sharing the arena, with separate red and gold HUDs](docs/screenshots/co-op-gameplay.png)

## Features

- Separate weapons, shields, bodies, augments and rerolls for each player.
- Upgrade choices can be saved for later and used during combat.
- A larger arena, more enemies in regular waves, and enemies that target both players.
- Revive a teammate at their beacon without resetting their build.
- Separate native health, ammo and construct displays, with red and gold player colors.

## Upgrades

Weapons, shields, bodies and augments all use small upgrade strips beside each
player's HUD, leaving the arena visible.
Both players can browse at once, or one can keep fighting while the other chooses.
Hover a hex or hold Inspect to reveal its [full description](docs/screenshots/upgrade-inspect.png). Combat stays live.

![Both players browsing compact native upgrade strips](docs/screenshots/compact-upgrades.png)

Press Escape to see both builds separately. Click P1 or P2, or press Tab, to
switch the native stats display.

![Separate builds and native stats in the pause screen](docs/screenshots/separate-builds.png)

## Revival

When a player dies, their ship leaves a beacon. After a short delay, the other
player can stay near it for three seconds to revive them. Their build is preserved.
The arena stays expanded while a teammate is down. If both players die, the run ends.

![A gold revive beacon marking a fallen teammate](docs/screenshots/beacon-revival.png)

## Installation

Requires **Nova Drift 1.2.38.4 on Steam, Windows x64**.

1. Download the Windows ZIP from [Releases](https://github.com/murlock1000/novadrift-coop/releases) and extract it.
2. Run `InstallCoop.cmd`. It finds your Steam copy and creates a separate co-op installation.
3. Open `PlayCoop.cmd` in `%USERPROFILE%\Games\NovaDriftCoop` and start a **fresh run**.

No compiler or WSL is needed. The installer downloads the mod loader; the game
is not included. See [installation](docs/INSTALLATION.md) for custom paths, updates
or building from source.

For online play, follow the [Steam Remote Play setup](README.RemotePlay.md), then
open `PlayRemoteCoop.cmd` and invite your friend. Steam shows the session as **Spacewar**.

Your original Steam installation and saves stay separate from the co-op copy.

## Controls

| | Player 1 · red | Player 2 · gold |
|---|---|---|
| Move | Mouse aim + left button thrust | I / J / K / L |
| Fire | Right mouse button | N |
| Open upgrades | Middle mouse button | F7 |
| Inspect an upgrade | Hover its hex | Hold O |
| Choose an upgrade | Left click | N |

Press **F6** to configure either player, or open `ConfigureControls.cmd`. **Escape** pauses the game for both players.
See [all controls](docs/CONTROLS.md) for rerolls, constructs and controller mappings.

Two-player save/resume is not supported. See [tested features and known limits](docs/VALIDATION.md).

---

[Installation](docs/INSTALLATION.md) · [Steam Remote Play](README.RemotePlay.md) ·
[Controls](docs/CONTROLS.md) · [Balance](docs/BALANCE.md) ·
[Development](docs/DEVELOPMENT.md) · [Third-party notices](docs/THIRD_PARTY.md)
