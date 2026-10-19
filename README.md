# Nova Drift Co-op

**Two pilots. One arena. Two completely different builds.**

Turn Nova Drift into a shared-screen co-op run. Fight together, grow stronger
with shared XP, and shape your own ship with separate weapons, shields, bodies
and augments. Play side by side, or bring a friend through Steam Remote Play Together.

![Two ships sharing the arena, with separate red and gold HUDs](docs/screenshots/co-op-gameplay.png)

## Built for two

- **Your own ship, your own build.** Each player gets independent upgrades and rerolls.
- **Keep the fight going.** Bank upgrade choices and spend them when you're ready.
- **More room, more enemies.** A larger arena and increased regular enemy groups, with enemies targeting both pilots.
- **Bring your teammate back.** Stay near their beacon to revive them without losing their build.
- **A familiar look.** Native HUDs, upgrade artwork and descriptions, with red and gold accents to tell the ships apart.

## Choose upgrades together

Small upgrade strips sit beside each player's HUD, leaving the arena visible.
Both players can browse at once, or one can keep fighting while the other chooses.
Hover a hex or hold Inspect to reveal its [full description](docs/screenshots/upgrade-inspect.png). Combat stays live.

![Both players browsing compact native upgrade strips](docs/screenshots/compact-upgrades.png)

## Cover each other

A fallen pilot leaves a beacon. After a short delay, stay close for three seconds
to bring them back with their build intact. The view smoothly adjusts as ships
join, fall and return. If both pilots go down, the run ends.

![A gold revive beacon marking a fallen teammate](docs/screenshots/beacon-revival.png)

## Install and play

Requires **Nova Drift 1.2.38.4 on Steam, Windows x64**. This is an experimental,
source-only mod; the game and compiled mod files are not included.

1. Follow the [installation guide](docs/INSTALLATION.md) to build and set up your co-op copy.
2. Open `PlayCoop.cmd` in `%USERPROFILE%\Games\NovaDriftCoop`.
3. Start a **fresh run**. The second ship joins automatically.

For online play, follow the [Steam Remote Play setup](README.RemotePlay.md), then
open `PlayRemoteCoop.cmd` and invite your friend. Steam shows the session as **Spacewar**.

Your original Steam installation and saves stay separate from the co-op copy.

## Pick your controls

| | Player 1 · red | Player 2 · gold |
|---|---|---|
| Move | Mouse aim + left button thrust | I / J / K / L |
| Fire | Right mouse button | N |
| Open upgrades | Middle mouse button | F7 |
| Inspect an upgrade | Hover its hex | Hold O |
| Choose an upgrade | Left click | N |

Press **F6** to configure P2. **Escape** pauses the game for both players.
See [all controls](docs/CONTROLS.md) for rerolls, constructs and controller mappings.

Initial weapon, shield and body choices use the original selection screen.
Two-player save/resume is not supported. See [tested features and known limits](docs/VALIDATION.md).

---

[Installation](docs/INSTALLATION.md) · [Steam Remote Play](README.RemotePlay.md) ·
[Controls](docs/CONTROLS.md) · [Balance](docs/BALANCE.md) ·
[Development](docs/DEVELOPMENT.md) · [Third-party notices](docs/THIRD_PARTY.md)
