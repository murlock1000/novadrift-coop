# Validation record

## P1 shop without a pointer (2026-10-19)

DLL `79b8f151ed30c3e066f0d9e80a74b788118dac989d28fa21580d27ed60e63944`
passed 35 native mouse-shop checks both with and without saved P1 bindings.
Real Windows wheel and button events select the highlighted item without any
cursor-placement fixture. Tests cover native item descriptions, deliberate
purchase/reroll chords, held thrust/fire, middle-button taps, P2 Inspect
isolation and an unrelated rebound P1 fire key. Reviewed game-client captures
show the existing native hexes, fonts and item descriptions. Original saves
were unchanged. The prior preview-6 DLL failed the held-Inspect step after
successfully browsing with the wheel.
The same DLL also passed the updated 33-check shop-control regression with
saved P1 bindings, including mouse chords, P2 keyboard selection, continued
flight/fire and shared pause.
The ordinary early-launch check also passed with `qa_focus=0`: both native HUDs,
compact shops, short middle-button taps, P2's F7 and rotation/thrust worked.
It now uses real combat input to earn the first level rather than waiting for
a stationary ship to receive incidental XP.

The new interaction replaces the earlier hover/hit-test approach. Its tests
could position the game's aim object, which proved that hit tests used those
coordinates but did not prove a usable visible pointer existed during flight.

## Normal-launch UI and P2 steering (2026-10-19)

DLL `1bc5c9fcad1e7047790cda34b10ccbb213b0d5d0c99bb24c43c37a4ed4825900`
passed an ordinary early-launch regression with `qa_focus=0`, diagnostics on,
and no injected QA commands. P1 was created before module initialization
finished. P2 joined after hook installation, both native health bars rendered,
and both compact shops opened on the first shared level. Real middle-mouse and
F7 input closed and reopened each player's shop. J and L rotated P2's native ship in opposite directions,
and I accelerated it with the shops visible. The fullscreen game-client capture
was reviewed. Original save fingerprints were unchanged.

The previous DLL `e40f336baf1863fa3d10fd49f28d0adb6c150df6a6f177d31fc0b501cb008916`
failed the same early-launch HUD assertion: its Controller Create was missed,
two ships joined during initialization, and native HUD capture remained inactive.
Earlier suites waited for module ready before starting a run and missed this case.

The new P2 defaults and translation passed 60 portable checks covering thrust,
rotation at seven headings, menu-only K, I+K, separate special actions,
unchanged P1 braking and optional directional steering. The compiled Windows
control editor also passed its separate-profile self-test. The active user's
existing P2 profile was switched to tank steering without replacing bindings.

The same DLL passed the 33-check native shop-control regression, including saved
P1 controls, independent firing, deliberate mouse and keyboard purchases, input
consumption and flight after purchasing. The test now waits for the current
command and measured motion after a purchase; its click-consumption assertion
checks the actual thrust input rather than mistaking weapon recoil for thrust.
The old visible-tray input-blocking mutation still failed the first acceleration
assertion. The native upgrade-flow suite passed all 25 checks on this DLL.

## Death crashes, HUD and gameplay recording (2026-10-19)

DLL `21986c00abe4b491bbe54ed074f82820ad5e3e1708203a49fd8d7405f3e2dfca`
passed 29 native death/effect checks, 25 upgrade-flow checks and 33 shop-control
checks, including launch with saved P1 controls. Both pilots were
tested with native Hullbreaker, Salvo and Reflect. Real thrust input created the
native effects before shield break and death. The tests verified effect cleanup,
survivor firing, retained Powers/builds, beacon progress, native revival, resumed
Salvo firing, and the native end screen after both deaths. Proximity placement
is an explicit QA fixture. Original save fingerprints remained unchanged.

With the same DLL, disabling the preflight death check reproduced
`oPowers.Step2:79`; omitting thrust-effect cleanup reproduced
`oBurstThrust.Step2:5`. Both were the original missing-player errors,
confirming that the regression detects each fix independently.

The pause screenshot was recaptured and visually checked: P2’s inventory now
sits outside the native stats columns. Native icons and hover rendering remain.
Upgrade key hints fade after 2.5 seconds; the gray banked-choice count remains.
Obsolete tutorial effects are associated with their parent even before joining
and after the parent disappears, addressing the detached purple pulse path.
The recording then reproduced a second background path: the tutorial’s base
trapezoid survived DrawGUI text suppression. The final visibility change hides
that exact obsolete tutorial across native draw passes while leaving its
simulation and cleanup active. The same visibility rule includes the native
Banish, Decline and Reroll guides: the latter two explicitly carry the native
`sTutorialBarWide` background in object metadata. In-game captures after this change show the bar
absent with trays both open and closed. This final visibility change was made
after the suites above; its DLL hash is
`5fd3f9e894d5aaa410730017ec1fe5feffcef9666654b1616922e08e31c18e97`.

## Flight controls with compact shops (2026-10-19)

The initial input-fix DLL passed 33 input/upgrade checks, 36 native tray checks and 27
per-player controls checks. Both ships accelerate from rest and fire with both
trays visible; keyboard combat does not browse or purchase offers. Holding
Inspect deliberately routes that player's controls to the tray. Releasing it
restores movement even with confirmation held. Mouse purchases consume only
that click, preserving firing and teammate movement, including rebound P1
controls. Escape still pauses both players.

Hit testing projects the native software cursor through the current camera
spans. The OS pointer remains locked by the game during combat. Mouse-button
checks use actual Windows events; the cursor is positioned as an explicit QA
fixture because injected motion did not reproduce the raw-device cursor stream
in this setup. This is not a physical mouse/controller hardware playtest.

Two negative controls restored the old all-input blocker and OS-pointer hit
testing. They failed the acceleration and native-cursor checks respectively.
All three positive suites and both negative runs used the same DLL. Original
save fingerprints remained unchanged.

## Compact starting gear and separate pause builds (2026-10-19)

Starting weapons, shields and bodies now use the same compact trays as later
augments. Native upgrade and deferred-upgrade entry points are redirected even
before joining finishes, so P1 cannot buy starter gear before P2 gets its own
build. The native intro timing is preserved.

The final DLL passed 25 upgrade-flow checks: automatic joining without a forced
join command, separate native offers and purchases for all three starter stages,
different weapons/shields/bodies, independent augments, native shield flashes at
each owner's relocated health bar, separate native pause icons, switching native
stats to P2, and shared Escape pause/resume. It also passed 34 stock-HUD checks,
including native pixel comparisons, deployment, revival, and a surviving P2.
During this change, the broader 36-check ship/lifecycle run and 35-check tray run
also passed, before a final cleanup of expired shield-effect IDs.

Pause inventories reuse native gear, mod and banished-mod rendering and native
hover information. Tab or clicking the P1/P2 headings changes the central native
stats. Score and elapsed time remain team values. The shared native scroll and
color animation advance once despite rendering two owner surfaces.

The final build also passed 27 checks for per-player rebinding and keyboard/mouse
input. Three deliberate mutations each failed the intended assertion: restoring
the full-screen starter menu, combined pause rendering, or central shield-flash
coordinates. These tests used the same compiled DLL with QA-only mutation flags.

README gameplay, compact-tray and Inspect images are unmodified game-client
captures with simulation running. The stock fixture uses the ordinary native
body so Spectre's background-darkening effect does not obscure documentation.
Earlier entries below describe previous builds; their full-screen starter
exception no longer applies. All original save fingerprints remained unchanged.

## Native co-op

An earlier ship build passed a 37-check bounded native regression: two real Player
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

The following beacon opacity patch passed the 24-check native overlay suite.
Beacons now draw once after the frontmost background in the world pass, below
ships, with normal alpha blending and restored GPU state. The progress fill is
6% opacity, the outline pulses between 24–32%, and the smaller center icon is
40%. Reviewed 1080p captures include each survivor directly over the beacon.
Both outer-edge beacons remain visible and revive the correct build.


## Ammo and construct HUD (2026-10-19)

Each player's native weapon reserves and mine, turret, drone and ally icons now
sit above their own health bars. Native counts, readiness masks, assembly meters
and construction flashes use the corresponding build. Configured direct-slot
key labels replace the original P1 bindings in P2's strip. The single-player
construct tutorial is suppressed during co-op.

The updated DLL passed **25 native checks** at 1920×1080, comparing opaque
interiors from each owner's rendered source with the actual game client.
The run exercised P2 mine deployment, both upgrade strips, P2 revival and
surviving P2 after P1 died. A deliberate shared-strip mutation failed P2's
native-pixel assertion while P1's display matched. Original saves were unchanged. Native reserve updates and flash animation are held only during QA
screenshot capture so their state cannot advance between the two images. This is HUD coverage for those fixtures, not every late-game build.

The README revival image is a fresh game-client capture with the full native
beacon visible inside the arena; its gold outline is checked in the image.

## Gameplay video (2026-10-19)

The README links a committed H.264/AAC MP4 recorded from the native game client
at 1920×1080, 30 fps. Only the game window and its process audio were captured;
there is no desktop, microphone or voice track. The clip uses scripted inputs,
preselected builds and a staged revival position. Combat, offers, purchases,
death, beacon progress and respawn are native simulation. Setup is trimmed out.

The successful continuous recording used DLL
`5fd3f9e894d5aaa410730017ec1fe5feffcef9666654b1616922e08e31c18e97`.
The final tutorial visibility grouping was extended afterward to include the
other obsolete upgrade guides. Native playtest captures are kept private; the
export and poster are in `docs/videos` and `docs/screenshots`.

The final DLL `e40f336baf1863fa3d10fd49f28d0adb6c150df6a6f177d31fc0b501cb008916` passed the 25-check native upgrade-flow suite after grouping those obsolete guides. Original save fingerprints remained unchanged. The final video export was checked for black segments, decoded end frames, codec/resolution and audio peaks.
