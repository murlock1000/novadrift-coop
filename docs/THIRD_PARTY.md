# Third-party dependencies

Dependencies are fetched from official upstream locations into ignored `local/`.
They are not redistributed in this repository. `dependencies.json` records exact
URLs and SHA-256 hashes; setup verifies bytes before they are used.

| Dependency | Pin | Upstream |
|---|---|---|
| Aurie loader and patcher | v2.0.2 | https://github.com/AurieFramework/Aurie |
| YYToolkit runtime | v5.0.0c | https://github.com/AurieFramework/YYToolkit |
| YYToolkit Zeus SDK headers/shared types | d5cc0078bfbfd5907884ad3fe0b3c6cbaa1c3f54 | https://github.com/AurieFramework/YYToolkit/tree/d5cc0078bfbfd5907884ad3fe0b3c6cbaa1c3f54 |

The SDK commit is intentional; the older stable-tag API does not match this mod.
Consult upstream LICENSE files for their terms. Native Windows SDK/runtime and
MSYS2 toolchain components are prerequisites managed separately by the user.

Nova Drift is a separate commercial game. No ownership of the game, assets or
Steam services is claimed. A legitimate matching game installation is required.
Spacewar is Valve's Steamworks example used here as an experimental Remote Play
entry; Valve does not endorse this mod.
