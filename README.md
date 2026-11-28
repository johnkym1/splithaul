# SplitHaul by grumpyman

A Windows dispatch app for single-player **American Truck Simulator** and
**Euro Truck Simulator 2**. Plan single, dual or multi-drop jobs with an owned
trailer combination, split the original cargo across deliveries, and keep an
exportable job history.

**[Download SplitHaul 0.3.9 for Windows](https://github.com/johnkym1/splithaul/releases/download/v0.3.9/SplitHaul-Setup-0.3.9-win64.exe)**

[Release notes and SHA-256 checksum](https://github.com/johnkym1/splithaul/releases/tag/v0.3.9)

This is an unsigned test pre-release. Setup bundles the app, both game plugins,
ATS and ETS2 trailer mods 1.2.1. The green **Code** button
downloads source code; use the installer link to install the app.

## Upgrading from Dispatch Companion

Dispatch Companion is now **SplitHaul by grumpyman**. Download the new installer
once: the old app's update checker cannot follow the renamed repository. Close
the app and both games, then run Setup. Existing installations upgrade in place
using the same installer identity, with the old executable backed up and the
shortcuts renamed. Profiles, settings and history are reused. Open SplitHaul
once to register its executable for Shift+F9 in both games.

New installations use a SplitHaul data directory. Existing installations keep
using their established data directory, including one named DispatchCompanion,
so there are no competing copies of history. The existing GT trailer mod
filenames and Mod Manager names stay compatible with purchased trailers.

## Start driving

1. Install the app and select the plugins/mods for the games you use. Select
   Steam game folders for plugins and Documents game folders for mods.
2. Open **SplitHaul** once from the Start menu.
3. Start ATS or ETS2 in single player, enable the chosen GT trailer mod if
   needed, and attach an owned combination: **1–4 cargo trailers in ATS or ETS2**. Recognised dollies are excluded from that count.
4. Enter Drive and stop. **Hold Shift and tap F9** to open or focus SplitHaul.
5. Choose **Single**, **Dual** or **Multi-drop** in Options. ATS modes are grouped
   together, followed by ETS2 modes.
6. Choose pickup, compatible cargo and final delivery. For multi-drop, choose
   an available delivery count and generate stops or select them manually.
   The final destination counts as one delivery.
7. Dispatch, return to the game and follow the pickup flag. Keep SplitHaul open
   through the deliveries. Deliver normally and return to Drive stopped for
   the next loaded leg. No SDK reload or save reload is needed.

Multi-drop divides the original cargo into whole units across 3–8 deliveries,
subject to route and cargo availability. All trailers stay attached. There are
no additional pickups. Capacity comes from the complete combination reported by
the game; it is not an assumed tonnes-per-trailer allowance.

## Full trailer loads

Enable **Options > Full trailer loads** for new SplitHaul jobs to use the
attached combination's own weight and volume capacity instead of country
road-weight limits. The option is off by default and saved per profile.
Existing jobs and ordinary Cargo Market offers are unchanged. Update both
the app and the selected game plugin before using it.

For the tested fuel-double definition, diesel capacity is 60 whole units
(51 tonnes). Actual capacity depends on the attached trailer definition and
cargo; the app does not multiply a single-trailer load by trailer count.
The capacity calculation is tested; loading and driving still need in-game
verification.

## Distance, round trips and DLC

**Preferred km** is saved separately for ATS and ETS2. Set it to **0** for a direct
route, or enter up to 10000 km. **Round trip** returns to the pickup city at a
different company and needs a nonzero distance. The planner aims near the chosen
distance, allowing 20% extra, and limits deliveries to compatible itineraries
and whole cargo units. Manual routes obey the same limit.

Distances use scaled city positions, not roads or ferry links. Actual game GPS
distance can differ. If fewer than three deliveries fit, increase the distance,
change cargo/destination, or use Single/Dual.

City and depot lists automatically follow each running game's **loaded DLC**.
Start the game and load a profile to refresh them. Iceland is included for ETS2
and appears when its DLC is loaded. An unowned DLC archive in Steam does not
unlock its cities. Mod-map cities are not imported automatically.

## Trailer mods and saved data

The optional archives are `GT_Tanker_Combinations_ATS.scs` and
`GT_Tanker_Combinations_ETS2.scs`. Keep the existing **GT Multi-Trailer
Combinations - ATS/ETS2** Mod Manager entries enabled rather than adding duplicates.
Both mods include Australian-style doubles, triples and quads through the normal
**Customize > Chain type** menu, where supported by the trailer family.
Freezers, curtainsiders, tippers, livestock, logging, flatbeds and compatible
tankers are included. Matching DLC remains required for branded trailers.
Dollies retain their existing axle arrangements; these are not exact Australian
manufacturer models. ETS2 displays the red advisory **over B-Double not recommended**
for combinations above a B-double. Selection diagrams are correctly sized in 1.2.1,
and tanker accessory defaults and compatibility are repaired. The driver has
confirmed the ETS2 food-tanker Quad now selects successfully; other combinations,
purchasing, driving and Australian replacement maps still need further testing.

## In-app updates and cancellation

SplitHaul 0.3.5 and later can download, verify and launch an update installer from
**Options > Check for updates**. Version 0.3.4 and earlier use the browser for
this first update. Close both games before installing updated plugins or mods.
Cancellation controls appear after dispatch, distinguish cancelled jobs from
completed jobs, and use stopped/menu checks before changing a live game job.
Convoy menu detection was updated; full live Convoy cancellation remains unverified.

Setup backs up replaced plugins and mods. Mods remain on app uninstall because
purchased trailers may depend on them. See the [driver guide](installer/DriverGuide.txt)
and [trailer mod guide](installer/TrailerMods.txt).

## Test-release limits

- Single-player only. TruckersMP is not supported.
- Plugins accept only their exact supported game executable builds. A game
  update may require a new SplitHaul build. Setup checks the game before replacing
  a plugin. Trailer mods target 1.62 and require matching DLC where applicable.
- Complete multi-drop/round-trip drives, newest ATS road-train driving and
  second-PC acceptance remain unverified. Automated and isolated installer tests
  do not establish in-game success.
- A game restart does not automatically resume an unfinished multi-stop sequence.
- SplitHaul is independent of SCS Software and is not endorsed by SCS Software.

## Source and support

- `dispatch_cpp/`: native Windows app, catalogs, UI, history and tests.
- `dispatch_shared/`: shared protocol, launcher and game selection.
- `native_job_bridge/`: ATS and ETS2 plugin targets and tests.
- `installer/`: Inno Setup source and packaging tools.

The internal dispatch source-directory names describe their function. User data,
game executables, extracted game assets and generated catalogs are excluded from
this source repository. See [third-party notices](THIRD_PARTY_NOTICES.md).

Author: **grumpyman** · GitHub: [johnkym1](https://github.com/johnkym1)
