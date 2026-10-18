# Steam Remote Play Together

Use **Spacewar, app 480**, as the Steam entry for the isolated Nova Drift co-op
copy. Steam and friends identify the session as Spacewar. Neither original
executable is replaced. Nova Drift itself does not advertise Remote Play Together;
Valve requires an eligible app for the invitation UI.
[Valve API documentation](https://partner.steamgames.com/doc/api/ISteamRemotePlay),
[Spacewar SDK example](https://partner.steamgames.com/doc/sdk/api/example).

## Configure

1. Sign into Steam. Install Spacewar using `steam://install/480` from Windows Run.
2. Build/install the co-op copy using [README.md](README.md).
3. Open `ConfigureRemotePlay.cmd` in the installed co-op folder. Its **Copy launch
   option** button copies the complete option when clicked.
4. In **Spacewar → Properties → General → Launch Options**, preserve the existing
   value, then paste the helper's option. At the default install location it has
   this form (replace the user path with your actual path):

   ```text
   "C:\Users\YOUR_NAME\Games\NovaDriftCoop\runtime\SteamCoop.exe" --host %command%
   ```

5. Enable Steam Overlay and Remote Play in Steam settings.
6. Open `PlayRemoteCoop.cmd`, or launch Spacewar from Steam. Nova Drift starts in
   fullscreen. Start a fresh run.
7. Press **Shift+Tab**, choose your friend, then **Remote Play Together**. You can
   also use **Invite Anyone to Play → Invite a Guest** for a private invitation
   link. Grant guest **keyboard** access and leave guest **mouse** access off.
8. Host uses mouse; guest uses I/J/K/L and N. Keep the host game focused. Exit the
   guest/session and game when finished; Steam should stop showing Spacewar running.

[Valve Remote Play guide](https://store.steampowered.com/remoteplay).
No invitations are sent automatically. Treat invitation links as private.

To undo setup, restore Spacewar's previous Launch Options. Leave them empty if
previously empty. This restores its ordinary executable launch.

## Workspace configuration tool

Preview first, then apply with no Steam game running:

```sh
python3 tools/configure_steam_remoteplay.py
python3 tools/configure_steam_remoteplay.py --apply
```

Apply closes Steam gracefully, backs up `localconfig.vdf`, changes Spacewar's
Launch Options, checks preservation of unrelated settings, then restarts Steam.
It records the prior option under ignored
`local/remoteplay-re/steam-configuration/480/`. If migrating this workspace's old
Fling wrapper, it restores Fling's recorded original option only when the current
value still exactly matches the old wrapper. Later user edits are preserved.
Steam may ask you to sign in after restart; no credentials are handled by the tool.

```sh
python3 tools/configure_steam_remoteplay.py --restore
```

Restore refuses to replace options changed since setup. Keep the private restore
record until you undo the configuration.

## Implementation and testing

`SteamCoop.exe --host` requires inherited Steam app identity 480. It ignores donor
launch arguments, starts the isolated launcher with a fixed interactive duration
of zero and explicit `fullscreen`, and waits for it to exit so Steam can track
play time. It rejects duplicate owned sessions and resets QA controls to normal
play. It uses no additional Steam API DLL. The inner sandbox keeps the game's
profile and Steam API isolated; the Steam parent chain supplies overlay/capture.
[Steam Overlay documentation](https://partner.steamgames.com/doc/features/overlay).

See [validation](docs/VALIDATION.md) for exactly which live paths have been tested.
The owned guest harness in `tools/qa_steam_guest.py` requires a fresh private
Invite Anyone URL. It isolates and restores Steam Link preferences and checks
client identity. Its process report alone does not establish video/input success.
A relay test additionally needs a hash-pinned public SDR configuration from
Valve's `ISteamApps/GetSDRConfig/v1?appid=480` endpoint. Authentication checks stay
intact. See the script's `--help`; keep URLs, logs and captures under ignored
`local/`, never in Git.
