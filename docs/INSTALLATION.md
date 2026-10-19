# Installation

You need **Nova Drift 1.2.38.4**, Steam build **25890040**, on Windows x64.
The mod uses a separate game copy and save profile.

## Windows download

1. Download and extract the Windows ZIP from [Releases](https://github.com/murlock1000/novadrift-coop/releases).
2. Double-click `InstallCoop.cmd`. If Steam's library cannot be found, enter the folder containing your original `NovaDrift.exe`.
3. Open `%USERPROFILE%\Games\NovaDriftCoop\PlayCoop.cmd` and start a fresh run.

No compiler, Python or WSL is required. Internet access is needed to download
hash-pinned Aurie and YYToolkit components from their upstream releases.
The ZIP contains the compiled mod, launchers, control editor and installer.
The game is not included. Access to this private repository is required to download.

Run `ConfigureControls.cmd` or press F6 to select a player and change their controls.
Steam Remote Play needs the separate [Steam setup](../README.RemotePlay.md).

To update, close the co-op game, editor and Steam helper, then run the new
installer. It preserves existing control profiles. To choose custom paths,
run `InstallCoop.ps1 -GameDirectory 'G:\SteamLibrary\steamapps\common\Nova Drift' -Destination 'C:\Games\NovaDriftCoop'` from PowerShell in the extracted folder.

## Build from source

### Source requirements

- **Nova Drift 1.2.38.4**, Steam build **25890040**, Windows x64.
- WSL with Windows interop, Python 3.8+ and Git.
- MSYS2 UCRT64 `g++.exe` at `C:\msys64\ucrt64\bin\g++.exe`.
- Visual Studio 2022 Build Tools with x64 C++ tools, Windows SDK and C++ runtime,
  at the default BuildTools path used by `tools/build_ship.cmd`.

Other game builds are rejected. Setup does not install these toolchains.

### Build and install

Clone the repository and run these commands from WSL:

```sh
git clone https://github.com/murlock1000/novadrift-coop.git
cd novadrift-coop
python3 tools/setup_project.py \
  --game-dir '/mnt/g/SteamLibrary/steamapps/common/Nova Drift'
```

Change `--game-dir` to your Steam library's Nova Drift folder. Access to the
private repository is required.

Setup copies the matching game, fetches hash-pinned dependencies, builds the mod
and launchers, and installs the co-op copy at:

```text
%USERPROFILE%\Games\NovaDriftCoop
```

It preserves the original Steam installation and saves. To choose another
location, add `--base /mnt/c/path/to/new-folder`. An existing destination must
have this project's ownership marker.

## Launch

Open the installed folder and run:

- **`PlayCoop.cmd`** — local co-op.
- **`PlayRemoteCoop.cmd`** — Steam Remote Play, after completing the [Steam setup](../README.RemotePlay.md).
- **`ConfigureControls.cmd`** (or `ConfigureP2.cmd`) — change either player's controls; F6 also opens this editor in-game.

Start a **fresh run**. P2 joins automatically. Two-player save/resume is unsupported.
The initial co-op profile copies your native preferences; co-op run saves are
suppressed because the native continuation format stores one player.

Normal launches request native fullscreen. For a window, run `PlayCoop.cmd windowed`.
Keep the game focused during Remote Play. Give the guest keyboard access and
leave guest mouse access off.

## Source updates

Close the co-op game, launcher, control editor and Steam helper, pull the latest
source, then rerun the setup command with the same source and destination.
The tools verify the matching game and recorded original-save baseline before
updating. Moving an installation requires staging it again because the loader
uses an absolute path. See [development](DEVELOPMENT.md) for individual build
and staging commands.

## Troubleshooting

If `SteamCoop.exe` is missing, the Steam shortcut reports it. Defender has
previously quarantined this experimental helper. Its source is
[`src/steam_coop.cpp`](../src/steam_coop.cpp); rerun the installer to restore it, or build it from source.
The project does not change Windows Security settings. Local `PlayCoop.cmd`
uses `novacoop_launch.exe` directly.

`PlayCoop.cmd diagnostics` enables diagnostic logs. See [validation](VALIDATION.md)
for tested behavior and remaining gaps, and [third-party notes](THIRD_PARTY.md)
for dependency licensing.
