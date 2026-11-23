# Dispatch Companion

## Download the Windows installer

**[Download Dispatch Companion 0.2.8 Setup (.exe)](https://github.com/johnkym1/dispatch-companion/releases/download/v0.2.8/DispatchCompanion-Setup-0.2.8-win64.exe)**

This is the test installer. The green **Code** button downloads the source
files. You can also [view the release and its checksum](https://github.com/johnkym1/dispatch-companion/releases/tag/v0.2.8).

Dispatch Companion is a Windows tool by Grumpyman for single-player **American
Truck Simulator** and **Euro Truck Simulator 2**. With one owned trailer, it
can dispatch a compatible single job or a two-stop plan. A two-stop plan starts
with the full compatible load and automatically assigns a smaller remaining
load after the first delivery. The app also keeps a readable job history that
you can export to CSV.

**Version 0.2.8 is a test pre-release.** It supports only the exact game builds
recognised by its telemetry DLLs. Setup checks the game executable before
installing them; a game update may require a new Dispatch Companion build.
This project is independent and is not endorsed by SCS Software.

## How to use it in game — SHIFT + F9

1. Download the [Windows installer](https://github.com/johnkym1/dispatch-companion/releases/download/v0.2.8/DispatchCompanion-Setup-0.2.8-win64.exe).
   Close ATS, ETS2 and Dispatch Companion before running Setup.
   ATS and ETS2 telemetry are checked by default; uncheck a game you do not use.
2. At the end of Setup, open Dispatch Companion once. This registers the
   installed app for the in-game shortcut.
3. Start ATS or ETS2 in single player. Attach **one owned trailer**, enter
   Drive, and stop the truck.
4. **Hold Shift and tap F9 while in the game.** Dispatch Companion opens or
   comes to the front. F9 alone is not the shortcut. If nothing opens, start
   the app once from the Windows Start menu and try Shift + F9 again.
5. The app detects the sole running game. In **Options**, choose **Single** or
   **Multiple**. Multiple currently means **two delivery stops**.
6. Select the pickup, cargo and delivery companies in the app. Dispatch while
   stopped, return to the game, and follow the route to the green pickup flag.
7. For a two-stop plan, deliver the first load normally and leave Dispatch
   Companion open. The second leg is assigned with its remaining cargo. After
   the final delivery, review the result and acknowledge completion in the app.

No `sdk reload` or saved-game reload is needed between deliveries. You can view
and export completed jobs from **History**. To cancel a job from the app, stop
the truck and open the game's pause menu first.

## Limits and safety

- Single-player only; TruckersMP and arbitrary stop counts are not supported.
- One owned trailer must be attached. The game decides which cargo fits it.
- Both games cannot be selected automatically at the same time. If both are
  running, choose the game in Options.
- The app and game DLL must be updated together when a supported game build
  changes. Setup backs up an existing `GT_Telemetry.dll` and leaves other
  plugins, game profiles and saves alone.
- This test build is unsigned. Keep your antivirus enabled. Check the asset's
  SHA-256 file before running it and report a detection for investigation.

## Project files

- `dispatch_cpp/`: native Windows app, catalog loader, history, UI and tests.
- `dispatch_shared/`: protocol and game-selection code shared with the DLLs.
- `native_job_bridge/`: separate ATS and ETS2 telemetry DLL targets and tests.
- `installer/`: Inno Setup source, packaging script and driver guide.

The public source tree excludes extracted game files, saves, profiles, job
history, local research, backups and build outputs. It includes the MIT-licensed
nlohmann JSON header and SCS telemetry SDK headers with their license notices.

Source builds currently use Visual Studio 18 Community's C++ tools and Inno
Setup 6.7.4 on Windows. The build scripts expect locally extracted game
definitions to generate the compact cargo catalogs; those game files are not
part of this repository. End users should use the pre-release installer.
