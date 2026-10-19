# Co-op fixes

| Issue | Status |
|---|---|
| Opening compact shops disables both ships' controls | Fixed: visible trays leave flight active; hold Inspect to operate keyboard/controller trays. Mouse purchases consume only the shop click. |
| Mouse hover/clicks miss compact offers during combat | Fixed: hit testing follows the game's visible software cursor while the native Windows pointer stays locked. |
| Launching with a saved P1 profile crashes during module initialization | Fixed: the runner base is established before resolving the profile's native input fields. |
| Death can crash in native Powers Step2 while a teammate survives | Fixed: run the native death check before the remaining combat event, preserving native death-prevention decisions. |
| A dead ship leaves a thrust effect referencing its missing player | Fixed: retire the pilot’s tagged native thrust effects during death teardown. |
| P2 pause inventory overlaps the native stats | Fixed: reserve outer inventory lanes and fit acquired/banished icons within them. |
| Upgrade opening keys stay above the HUD | Fixed: keep a quiet banked-choice count; fade key hints after 2.5 seconds. |
| Hidden upgrade advice leaves a purple pulse | Fixed in the reproduced recording: hide the obsolete tutorial across native draw passes and retain its linked pulse IDs after its text disappears. |
| P2 can inherit starter choices made before joining | Fixed: requests wait for both builds before either player can choose. |
| Starter gear opens the full-screen shop | Fixed: weapon, shield, body and augment choices all use personal compact trays. |
| Escape groups the two builds together | Fixed: separate native gear, mod and banished-mod sections; click P1/P2 or press Tab to switch native stats. |
| A blue shield flash remains at the old central health bar | Fixed: the four native shield-effect creation sites retain their owner and draw beside that owner's bar. Other world effects stay native. |

Runtime coverage and remaining limits are in [VALIDATION.md](docs/VALIDATION.md).
The entire late-game trait space and physical controller hardware are not covered.
