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
and public Steam relay traffic. A current-DLL relay run kept both ships alive:
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
Warsaw SDR relay (about 18–19 ms RTT). A 240-sample current-DLL trial kept both
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
