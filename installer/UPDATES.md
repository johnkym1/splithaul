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

The driver opens Options and presses Check for updates. The app compares its
installed version with the highest eligible published version. If a newer
version exists, it offers to open that exact GitHub release page. The driver
downloads Setup, closes Dispatch and both games, and runs the installer.
Existing app profiles, settings and history are preserved by the installer.
Game plugin installation remains optional and uses Setup's usual game-version
checks and backups.

There is no automatic installation and no GitHub sign-in is required to check
public releases. A network failure or GitHub rate limit produces an error and
can be retried; it does not claim the installed version is current.

## Current package

The all-in-one release is 0.3.4, including ATS mod 1.1.5 and ETS2 mod 1.1.6.
It includes full trailer loads, compatible automatic pickup-city selection,
and branded ETS2 HCT freezer combinations with corrected hitch accessories.
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
