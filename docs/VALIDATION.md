# Validation record

## Native co-op

The current ship DLL passed a 37-check bounded native regression: two real Player
instances, independent builds/upgrades, shared XP, input isolation, survivor
Escape pause/resume, and native game-over confirmation/restart. Original saves
were unchanged. Earlier mutation checks exercised failure detection for ownership,
native UI and input sharing. Full late-game augment coverage is not established.

## Remote Play

Before the switch to Spacewar, actual Steam host/owned guest sessions using the
Fling launch entry verified decoded game video, LAN P2 upgrade/input/release,
and public Steam relay traffic. A pre-polish DLL relay run kept both ships alive:
remote fire produced 26 P2 projectiles; a separate host-mouse phase produced 14
P1 projectiles. Input affected the intended ship and release stopped fire. An
earlier relay run disconnected while firing and reconnected through another relay.
Guest preference directories were restored after cleanup.

Those are same-PC host/guest tests over actual Steam relays. A friend on another
physical device/network, controlled lag/loss and independently audible output
have not been tested. The audio channel initialized, but that is not an audible
output assertion. Historical Fling results do not alone prove the Spacewar entry.

## Display/configuration change

The launcher passed 31 command/preference checks using synthetic native profiles.
Steam VDF tests cover option insertion/restoration, unrelated values, CRLF/quoted
paths, duplicates, exact legacy migration and preserving later user edits.
Live Spacewar launch on 2026-10-18 passed: Steam app 480 tracked the wrapper →
launcher → game chain, injected its overlay and exposed Remote Play Together.
An owned Invite Anyone guest decoded Nova Drift video over the actual public
Warsaw SDR relay (about 18–19 ms RTT). A 240-sample pre-polish DLL trial kept both
ships alive: guest fire produced 24 P2 projectiles, separate host mouse fire
produced 12 P1 projectiles, steering affected P2 and release stopped guest fire.
Guest preferences were restored. This remains a same-PC relay test.

The normal launcher omitted `-inawindow`, requested native fullscreen and set
only the isolated profile's windowed/borderless preferences to zero. Its physical
client covered the 1920×1080 monitor, including the taskbar area. Geometry alone
does not establish exclusive presentation. Original Nova Drift/Spacewar EXEs and
original Nova Drift saves were unchanged. Nine Steam configuration tests passed.

A clean source-only Git export fetched all ten pinned dependencies and built the
sandbox DLL, native launcher, P2 editor and ship DLL with the documented toolchains.

Private evidence, exact per-run hashes, screenshots and process identities stay
under `local/`; this repository records results without private game artifacts.

## Co-op balance preset (2026-10-18)

The tuned DLL passed **15 native balance checks** in a separate marked QA
installation, plus the existing **37-check native regression**. Original saves
matched their baseline after both suites. A deliberate P1-only targeting mutation
failed the P2-near target/aim assertion, confirming that the check detects the
original targeting defect.

The balance suite exercised a real native Interceptor choosing P2, P1, P2 across
a wrap edge, and visible P1 while P2 was cloaked. It checked the native target
coordinates as well as the resulting instance ID and retained world ownership.
A separate phase killed P2 through native health/death processing and verified
that the enemy targeted surviving P1. Controlled AI fixtures explicitly set and
restore player visibility/positions; they do not bypass cloak logic in normal play.

Native camera publication grew width and height by 1.20. A ship positioned in
the added margin stayed there after the real wrapPlayer routine, demonstrating
expanded playable bounds. Regular-wave probes exercised the native Curated and
Dynamic scripts: seven accepted groups had a total budget of 12 before tuning
and 19 afterward, with fractional carry. This proves the supported group-budget
change; it is not an exact count assertion for every deferred enemy in every wave.

Real small/large/mega pickup instances passed through native updateScore with
0.70 XP scaling; an intro pickup instance's scripted award stayed unscaled. These
are native award-routine checks, not a full physical pickup-collision matrix.
Actual native Grandeur Create/Alarm granted two choices and bonus effective
levels only to its owner, separately for P1 and P2. Subsequent XP progression
advanced the shared team level and granted a choice to both.

The bounded diagnostic run had frame-interval medians around 16.6–16.8 ms, with
occasional larger spikes during scripted multi-wave probes. No late-game load
or exhaustive boss/decoy/trait-combination performance claim is made. Bravado's
indirect shared-world rarity path retains native context; this preset does not
aggregate both builds' rarity modifiers. See [balance coverage](BALANCE.md).


## Combat trays and revival (October 2026)

The first polish build passed **101 native integration checks** across the
37-check regression, 18-check revival/zoom suite, 15-check balance suite,
26-check compact-tray suite and five-check banked-starter handoff suite. All
five runs used the same DLL and left original saves unchanged.

The polish integration uses isolated owned runtimes. `qa_ships.py --polish`
checks intermediate native camera sizes, both death/revive orders, the native
pause gate, half effective hull, preserved Powers/build IDs and run generation,
and normal final-player game over. A controlled native enemy-projectile
collision is rejected during the native revive timer and damages the same ship
after that timer expires. This demonstrates that collision path; it does not
claim protection against every direct or damage-over-time source.

`qa_ships.py --trays` exercises each original starter stage, simultaneous
personal trays, continuing native ship/enemy steps, P2 firing while P1 chooses,
held-fire rejection, native purchases and acquired mods, owner-local rerolls,
Researcher banish, generation-failure rollback, native gear replacement, native
Orbital shield helpers, manual pause and cleanup on death. Normal combat uses
no persistent native UI828 instance: offer/confirm brokers are created and
retired synchronously, with native list create/destroy tracking in diagnostics.

The compact view uses the original upgrade hex sprites, `sSelectaWhite`,
`sTutorialBox`, native fonts and `drawTextCustom`. The full description is shown
only during hover or Inspect. No extracted game artwork or text is distributed
in this repository. Standalone augment definitions without the usual parent,
including Turret and Grandeur, are included in ownership routing; Orbital's
controller/orbs also retain their ship's context.

These are bounded native integration checks, not an exhaustive validation of
all late-game trait combinations, controller hardware, or remote friend setups.
`--starter-bank` additionally verifies that P1 banking an ordinary choice
never blocks P2 from completing the three native starter choices.
QA keeps automatic revival disabled in legacy death fixtures; `--polish`
explicitly enables it. Normal play always uses the teammate-beacon mechanic.

## Menu and overlay follow-up (2026-10-19)

The final rendering build passed the 20-check overlay suite and 37-check native
regression suite. The preceding build, before the final draw-order adjustment,
also passed 26 compact-tray, 18 revival and five starter-handoff checks. A deliberate
hidden-beacon mutation failed the client-pixel assertion as expected.

The overlay suite checks native upgrade-key and direct deferred requests during
combat, including either downed owner. These remain outside the full-screen
native shop; the three starter equipment stages and shared pause stay available.
It captures the verified foreground game client and checks owner-colored pixels
around both revive-range rings, then revives each pilot with unchanged build IDs
and run generation. Both Viper body helpers are created natively; each death
removes only its owner's helper, and native refresh recreates it on revival.

Reviewed 1920×1080 client captures cover simultaneous strips, native Inspect
text, settled hints, shared pause and both downed states. Card position counters,
duplicate open-key labels, persistent navigation instructions and obsolete
single-player upgrade/tutorial banners are removed or faded. Exact localized
message matching preserves other native messages. This is bounded visual review,
not proof for every resolution, language or long-description combination.

The proximity fixture pins and replenishes the survivor to avoid unrelated
ambient combat ending the test. Native collision protection probes clear that
fixture before measuring actual damage. Normal play does not replenish hull
while reviving.

## Native styling, beacon bounds and two-player controls (2026-10-19)

The compact labels now explicitly select the native `fontHeading` and
`fontSmall` resources, using white titles and gray counters. Player colors
remain on selection outlines. The beacon uses native `sHexTacticalBeacon`,
`sSelectaWhite` and `sSelectaPulse` artwork, with a filling hex for revive
progress. No cross, circle or progress segments are drawn with line primitives.

The expanded arena stays in place while either teammate is down. The final
build passed the 24-check native overlay suite. The suite kills each player in an outer margin beyond the single-player
bounds, checks the unchanged co-op scale, checks presented native-art pixels
and revives them there with the same build.

The 14-check dual-control suite passes on the final build, including a native
restart from game over with a saved P1 profile. It checks independent rebound
fire keys, P1 hot reload,
P1 directional movement and compact keyboard selection, preserved P2 settings
and shared pause. The editor self-test switches players and persists separate
profiles. Physical controller hardware and all native starter-menu control
combinations remain untested. P1 starter/menu controls retain the original
game bindings; compact combat trays use the co-op profile.

The prebuilt ZIP was extracted and installed by Windows PowerShell into a
separate owned test copy. Checks exercise payload checksum rejection and
refusal to install inside the original game. Loader/toolkit downloads are
hash-pinned. The original Steam executable and original saves remain unchanged.
