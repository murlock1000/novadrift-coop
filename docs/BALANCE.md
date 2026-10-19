# Co-op balance preset

The first preset keeps Nova Drift's native combat and progression rules while
making room for two ships. Values are starting points for playtesting, not a
claim that every build or late-game encounter is equally difficult.

| Setting | Default | Effect |
|---|---:|---|
| `enabled` | 1 | Enable arena/attention/density/XP tuning |
| `arena_scale` | 1.20 | 20% more width and height: 44% more playable area |
| `enemy_budget` | 1.60 | Increase supported regular spawn-group counts, with fractional carry |
| `xp_per_enemy` | 0.70 | Shared XP from normal small/large/mega pickups |

Edit `runtime\coop-balance.ini` in the installed copy and restart the game.
The file is created with defaults if missing. `enabled=0` restores native arena,
attention, group counts and pickup amounts; the independent-trait correction
remains part of co-op ownership. Invalid settings disable the tuning pass and
write a reason to the private mod log. Supported ranges: arena 1–1.5, enemy
budget 1–2.5, XP 0.25–1.5. Keep a copy of your chosen profile when updating.

## Arena and attention

The arena grows around its existing center. Native camera/view publication also
updates wrapping, edge placement and despawn bounds. Boss/gear zoom changes still
work; scaling is applied to fresh camera output without multiplying the game's
zoom state repeatedly. The expanded arena stays in place while a teammate is down, so a beacon in
the outer margin remains visible and reachable.

Hostile AI chooses between living, visible ships using distance across wrap
edges and a switching margin to avoid rapid oscillation. Native targeting then
handles aim prediction, cloak and decoys. Enemy attention has its own temporary
scope; choosing P2 does not turn enemy shots into P2 weapons or change ownership
of collision damage. Friendly homing weapons retain their normal target logic.
Cross-owner decoy combinations and every special boss attack are not exhaustively
covered by the initial test suite.

## Enemy pressure

Native wave order, wave timers and boss scheduling are preserved. This version
raises native Curated/Dynamic group counts for these confirmed ordinary families:
large/small boxes, Celestial Drones, Hive Drones/hot variants, Interceptors and
Swarmlings. Fractional carry distributes added enemies across groups without
always rounding each group up. Other families, boss groups and scripted special
spawns keep their native counts. Thus `1.60` is a supported-group budget multiplier,
not a guarantee of exactly 60% more total enemies in every wave.

Additional group pressure and reduced XP per ordinary pickup are intended to
keep two independently growing builds from clearing single-player waves too
quickly. The rough full-coverage rate would be 1.60 × 0.70 = 1.12 times native XP;
actual pacing varies with wave composition and player kills. The first scripted
XP pickup and special-mode awards retain their native values. Score is unchanged.
With one surviving ship, new regular groups and pickup amounts use native values;
already-spawned enemies keep their state. Boss health/damage are unchanged.

## XP and traits

XP-earned levels advance the shared team level and give each living player their
own banked upgrade choice. Builds, rerolls and purchases stay separate.
From the first weapon choice onward, either player can open a personal tray
during combat. Both may choose at once; closing preserves the exact offer set.
No choosing immunity is added, so the teammate can cover the selecting ship.

A trait's bonus levels belong to its buyer. For example, Grandeur's native
two-level grant becomes two bonus effective levels and two choices for that ship;
it does not grant choices to the partner or advance the shared XP threshold.
Native limits and upgrade eligibility still apply. XP/difficulty multipliers stay
native shared settings; traits with indirect enemy-rarity/drop effects retain
those native effects rather than receiving an invented averaged multiplier.

## Verification

See [validation](VALIDATION.md). For developer checks:

```sh
g++ -std=c++17 -Wall -Wextra -Werror tests/balance_policy_contract.cpp \
  -o local/balance-policy-contract
local/balance-policy-contract
NOVACOOP_TEST_RUNTIME=/mnt/c/path/to/owned-qa/runtime \
  python3 tools/qa_ships.py --balance
```

Prepare a separate QA install with `tools/prepare_ships.py --base ... --no-activate`.
The balance harness uses actual native enemies, wrap, pickup award and trait
routines in that owned copy. It also executes bounded native spawn-group probes.
`--balance --mutation-balance-target` deliberately restores P1-only attention;
the P2-near targeting assertion must fail. All probe commands require QA mode.
No gameplay probes run during normal play.
