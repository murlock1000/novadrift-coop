# Nova Drift local co-op prototype

Player one uses the normal ship. Player two pilots a real Ally with independent
movement, aim, and firing in the same arena. This native DLL prototype targets
**Nova Drift 1.2.38.4**, Steam build **25890040**, Windows x64 GameMaker YYC.

## Play

Open [PlayAllyCoop.cmd](PlayAllyCoop.cmd) from Windows.
Start a run normally, then press **F8** to deploy player two's Ally.

| Player two | Keyboard | Second XInput controller (slot 1) |
|---|---|---|
| Deploy / redeploy after death | F8 | Start |
| Move | I / J / K / L | Left stick |
| Aim | Movement direction; U / O rotate | Right stick |
| Fire | N | Right trigger or A |

The first controller (slot 0) and normal ship controls remain available to
player one. The launcher uses a separate profile in `local/test-profile`,
initialized from a copy of the existing save. This prototype runs offline.
Close the game normally to finish; the launcher cleans up its temporary drive.

This is an Ally-based local prototype. Both players share player one's run,
build, and progression. It uses the Ally's regular weapon; integrated weapons,
two full ship builds, separate upgrade menus, and online multiplayer are not
implemented. Physical controller input has not been verified with hardware.

## Build and validation

The current machine already has the pinned loader dependencies under
`local/deps-research`. Their versions, official sources, and digests are recorded
in [binary-manifest.json](local/deps-research/binary-manifest.json).
Aurie 2.0.2 and YYToolkit 5.0.0c are used with the matching experimental SDK
commit `d5cc0078bfbfd5907884ad3fe0b3c6cbaa1c3f54`; the release tag's older headers
have an incompatible interface.

Build the plugin with `tools/build_coop.cmd` from Windows (VS2022 x64 Build
Tools), then run these commands from WSL:

```sh
python3 tools/build_native.py
python3 tools/prepare_runtime.py
python3 tools/qa_local.py --seconds 240 --debug --lifecycle
python3 tools/qa_local.py --seconds 90 --debug --mutation-movement
```

The final command is a negative control: it disables manual velocity writes
while keeping the mod enabled, so movement assertions must fail. In live combat, main-ship knockback may make a movement comparison inconclusive.
The harness retains those trials and allows up to three attempts per direction;
all Ally direction and drift checks remain strict. The normal
suite measures independent movement, actual projectile creation and release,
real enemy damage, pause/resume, destruction/redeployment, and restart. Tests
use scripted inputs and an explicit background-focus override; the play
launcher resets both to ordinary keyboard/controller input.

Destruction QA sets the Ally's hull to zero, observes normal removal, then
redeploys it. Restart QA first enters the game's pause menu, then uses its native
restart object and cleanup script. Player-one survival checks use its live
instance; player-one hull is not instrumented.

The source Steam installation and its saves are preserved. `game/` is a pristine,
SHA-256-verified copy: 50 files, 852,255,215 bytes. Only `local/runtime` is patched.
`prepare_runtime.py` creates that runtime, stages the compiled components, and
patches a pristine runtime executable with Aurie. The diagnostic probe is
disabled during normal play and the co-op validation.

Relevant code: [coop_ally.cpp](src/coop_ally.cpp),
[native_abi.hpp](src/native_abi.hpp), [launch.cpp](src/launch.cpp),
[sandbox.cpp](src/sandbox.cpp), and [qa_local.py](tools/qa_local.py).
Native hooks validate pinned code signatures before installation. This is tied
to the identified game build; updates require a new native compatibility audit.

Private research and initial feasibility findings remain under `local/`.
Read-only inventory can be reproduced with:

```sh
python3 tools/inspect_game.py --game-dir game --out local/inventory
```

## Recorded result

The full local suite passed **82 checks** with a clean exit and unchanged original
saves. It measured three movement directions, nine projectiles during held fire,
90 enemy hull removed during the combat phase, pause/resume, destruction and
redeployment, and movement/firing after a native restart from the pause menu.
The final run needed no motion retries.

[Full report](local/qa/20261017T174200.059415Z/report.json) ·
[In-game screenshot](local/qa/20261017T174200.059415Z/game-screenshot.png)

The movement negative control failed at the expected drift assertion with the
same DLL. The ordinary launch shortcut then passed with manual controls restored
and a clean exit. All 50 original game files, all 50 pristine copied files, and
all three original save files still matched their baselines.

[Validation summary](local/validation-summary.json) ·
[Negative control](local/qa/20261017T174345.506440Z/report.json) ·
[Launcher smoke test](local/launcher-smoke/report.json)
