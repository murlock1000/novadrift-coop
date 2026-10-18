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
