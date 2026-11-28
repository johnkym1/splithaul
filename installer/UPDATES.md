# Publishing SplitHaul updates

The Check for updates button reads the published releases at
[johnkym1/splithaul](https://github.com/johnkym1/splithaul/releases).
GitHub already provides the update feed. No additional server is needed.

## Prepare a build

1. Choose a higher three-part version, for example `0.2.10` after `0.2.9`.
2. Update the numeric, text and wide version macros and release date in
   `dispatch_cpp/app_version.h`. Update release notes and publish the matching
   source snapshot through the project's source-only release process.
3. Keep the mod filenames and Mod Manager display names stable across updates.
   Update `installer/trailer_mods.json` only after validating a new mod archive;
   it pins the exact versions and hashes included in Setup. Never run the older
   trailer builder layers as part of installer packaging.
4. Run `powershell -File installer/build.ps1` from the development workspace.
   It builds and checks the app/DLLs and produces the installer, SHA-256 sidecar
   and payload manifest in `dist`. Inspect the generated payload manifest.
5. Run `powershell -File installer/test_bundle.ps1` for isolated installer tests.
   It uses fixture game homes and a separate test uninstall identity; it does
   not install into real game folders. Logs and verification are saved under
   `installer/build/bundle-test-<id>`.

## Publish on GitHub

1. Open the repository's Releases page and choose **Draft a new release**.
2. Create the matching tag, such as `v0.2.10`, against the matching source
   commit. Enter a release title and notes describing the changes.
3. Attach all three generated files:
   - `SplitHaul-Setup-0.2.10-win64.exe`
   - `SplitHaul-Setup-0.2.10-win64.exe.sha256`
   - `SplitHaul-0.2.10-manifest.json`
4. Keep **Set as a pre-release** selected while distributing test builds.
5. Publish only once the assets are fully uploaded. Drafts are not offered as
   updates, and incomplete Windows releases are ignored by the checker.

The app recognises tags in the form `vMAJOR.MINOR.PATCH`; use the pre-release
checkbox for test builds rather than adding a `-beta` suffix to the tag.
It currently includes both stable and test releases. When the project moves
to stable-only updates, change `updates::includePrereleases` and the Updates
status caption together.

## What a driver sees

The driver opens Options and presses Check for updates. A newer eligible release
changes the button to Download update. Downloads run in the background with
progress, cancellation and retry. The app verifies the exact release asset URL,
size, GitHub SHA-256 digest and matching checksum sidecar before offering Install
update. HTTPS redirects are restricted to GitHub's release asset service.

The driver closes both games and presses Install update. Confirmation starts
normal Setup (including Windows elevation) and closes SplitHaul only after the
launch succeeds. Cancelled elevation leaves the app and download available.
The cached installer is rechecked immediately before launch. An installed copy
passes its existing install folder to Setup; a development copy uses normal
Setup defaults. Setup independently checks running processes before replacement.

No browser or GitHub sign-in is needed. Version 0.3.4 and earlier still use the
browser flow and need one update to gain this feature. No silent or forced
installation is performed. Settings and histories in installed app data persist.

## Current package

The current all-in-one build is 0.3.9, with ATS and ETS2 mods 1.2.1.
It adds four-cargo-trailer support, Australian-style double/triple/quad layouts,
correctly sized chain diagrams, and the ETS2 tanker selection repair. The driver
confirmed the food-tanker Quad selects successfully after the repair. Other
new combinations and driving remain under test. ETS2 shows the red advisory
`over B-Double not recommended` above a B-double.
It also includes in-app downloading/installation and cancellation improvements
from local versions 0.3.5–0.3.8. Full live Convoy cancellation remains unverified.
It includes full trailer loads, compatible automatic pickup-city selection,
branded ETS2 HCT freezer combinations with corrected hitch accessories,
and country-unlocked standard Double, B-Double and HCT definitions.
The app, catalogs, both game plugins and both mod archives are bundled. Optional
mod tasks install to the chosen game data folders, with verified backups.
Mods must be enabled manually in each profile and remain after app uninstall.
The Mod Manager names and GT_Tanker_Combinations_*.scs filenames are stable.

Local builds 0.3.2 and 0.3.3 were not published. Version 0.3.4 lets those builds
exercise Check for updates. Full trailer loads defaults off for new profiles;
enable it in Options for the profile being tested. This is a test pre-release.

Older Dispatch Companion installations need a manual first SplitHaul download
because their updater cannot follow the renamed repository. SplitHaul 0.3.0
and newer can find this release through Check for updates.
