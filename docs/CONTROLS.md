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

Middle mouse opens/closes personal upgrades. Scroll to browse,
hover a hex to inspect, and left click to choose. Reroll and Researcher banish
controls appear in the strip.

## Player 2 · gold

| Action | Default |
|---|---|
| Thrust forward | I |
| Rotate left / right | J / L |
| Down in upgrade menus | K (while holding O) |
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

Opening a strip keeps flight controls active. Hold **O** (the configured
Formation/Inspect control) to browse with movement keys, choose with **N**,
reroll with **U**, or banish with **8**. Release Inspect to fly again. The same
modifier applies to keyboard/controller P1 using that player's bindings.
P2 defaults to three-button flight: thrust forward and rotate left/right.
K only navigates menus; it does not brake or reverse. Right Shift remains the
separate Stabilize action. Directional steering is an optional setting that
turns toward the pressed direction and retains native momentum.

F6 pauses play and opens the editor. `ConfigureControls.cmd` opens it separately; `ConfigureP2.cmd` remains an alias.
Choose directional/tank steering, rebind keys or configure controller mappings.
Profiles live in `runtime\p1-controls.ini` and `runtime\p2-controls.ini`. P2 keyboard bindings take priority
over conflicting P1 keyboard alternatives; P1's mouse remains available.
Physical controller hardware has not been tested. Starting gear and later
augments both use the compact strips and configured controls.

## Playing together

Each player keeps their own upgrade choices and rerolls. Closing a strip
preserves its offers and spends nothing. Visible strips leave both ships
controllable. P1's clicks on shop items consume only the left click until
release; firing remains available. Holding Inspect temporarily directs that
player's keyboard/controller to the strip while their ship drifts under normal
physics and remains vulnerable. The teammate keeps full control. Escape remains
a shared pause. Both native build
inventories appear separately; click P1 or P2, or press Tab, to switch the
central stats. Hover an icon for the native item information.

Both HUDs use native health, shield and status rendering with red/gold accents.
Upgrade names use the native heading font in white; counts use the native small
font in gray. Player color stays on the selection outlines.
Control hints fade. Initial weapon, shield and body selections use the same
compact strips as later augments. The native upgrade key also opens a strip. Obsolete single-player upgrade prompts
are hidden; navigation hints fade and the selected name stays visible.

Both ships join before starting gear is selected. F8 retries a failed join;
it does not give a free redeploy.
For Steam Remote Play, the host uses mouse and the guest uses keyboard. Grant
guest keyboard access, leave guest mouse off, and keep the host game focused.

The `+number` above a pilot’s HUD is their saved upgrade count. The opening-button hint appears briefly when more upgrades become available.
