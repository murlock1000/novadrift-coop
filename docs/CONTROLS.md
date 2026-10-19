# Controls

Press **F6** in-game or open **ConfigureControls.cmd**, then select **Player 1**
or **Player 2**. Save applies the selected player's profile. Both players can
rebind keyboard/mouse buttons, choose directional or tank steering, and select
an XInput controller slot, buttons and deadzone. Use separate controller slots.

P1 also has **Aim with mouse**. Leave it enabled for the original mouse setup;
disable it for keyboard or controller steering. Until you save a P1 profile,
P1 uses the copied game's controls. P2's existing profile remains compatible.

## Player 1 · red

Mouse aims, left mouse button thrusts, and right mouse button fires. These are
the copied game's mouse bindings.

During combat, middle mouse opens/closes personal upgrades. Scroll to browse,
hover a hex to inspect, and left click to choose. Reroll and Researcher banish
controls appear in the strip.

## Player 2 · gold

| Action | Default |
|---|---|
| Up / left / down / right | I / J / K / L |
| Fire / confirm upgrade | N |
| Stabilize / close upgrades | Right Shift |
| Blink | Right Ctrl |
| Deploy selected construct / reroll upgrade | U |
| Formation / hold to inspect upgrade | O |
| Construct slots 1–5 | 7 / 8 / 9 / 0 / minus |
| Banish upgrade as Researcher | 8 |
| Strafe left / right | Semicolon / apostrophe |
| Strafe forward / back | Left / right bracket |
| Strafe modifier | Right Alt |
| Open / close personal upgrades | F7 (controller: Back) |
| Configure controls | F6 |

Movement keys browse upgrades while P2's strip is open. Directional steering in
combat turns toward the pressed direction and retains native momentum.

F6 pauses play and opens the editor. `ConfigureControls.cmd` opens it separately; `ConfigureP2.cmd` remains an alias.
Choose directional/tank steering, rebind keys or configure controller mappings.
Profiles live in `runtime\p1-controls.ini` and `runtime\p2-controls.ini`. P2 keyboard bindings take priority
over conflicting P1 keyboard alternatives; P1's mouse remains available.
Physical controller hardware has not been tested. P1's original starter/menu
bindings still apply on the native full-screen menus; the compact combat trays
use the configured controls.

## Playing together

Each player keeps their own upgrade choices and rerolls. Closing a strip
preserves its offers and spends nothing. While choosing, that ship drifts under
normal physics, remains vulnerable, and uses its controls for the strip; the
teammate can keep fighting. Escape remains a shared pause.

Both HUDs use native health, shield and status rendering with red/gold accents.
Upgrade names use the native heading font in white; counts use the native small
font in gray. Player color stays on the selection outlines.
Control hints fade. Initial weapon, shield and body selections use the native
screen; combat upgrades use the compact strips. The native upgrade key also
opens a compact strip during combat. Obsolete single-player upgrade prompts
are hidden; navigation hints fade and the selected name stays visible.

F8 can join P2 before automatic joining; it does not give a free redeploy.
For Steam Remote Play, the host uses mouse and the guest uses keyboard. Grant
guest keyboard access, leave guest mouse off, and keep the host game focused.
