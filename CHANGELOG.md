# Changelog

## 0.3.9 — Australian road trains, test pre-release, 28 November 2026

- Accept up to four cargo trailers in ATS and ETS2, excluding recognised dollies.
- Add Australian-style double/triple/quad combinations and an ETS2 red advisory
  above B-double. Keep the normal dealer and existing mod filenames.
- Bundle both mods at 1.2.1 with correctly sized chain diagrams and repaired
  ETS2 tanker lead accessory/body defaults. Food-tanker Quad selection was
  confirmed by the driver after the repair; other live testing continues.
- Include in-app update download/verification/installation, cancellation controls
  after dispatch, cancelled-versus-completed status and Convoy menu detection
  from local builds 0.3.5–0.3.8. Full live Convoy cancellation remains unverified.

## 0.3.4 — Full loads and ETS2 HCT updates, test pre-release, 26 November 2026

- Add per-profile Full trailer loads for new jobs, bounded by trailer payload
  and volume rather than country road-weight limits; retain policy at handover.
- Find a nearby pickup city compatible with the attached trailer, including
  when the nearest city only has incompatible ferry depots.
- Bundle ATS mod 1.1.5 and ETS2 mod 1.1.6. Add Schmitz, Tirsan, Schwarzmuller,
  Kogel and Krone HCT freezer choices requiring the respective DLC, and correct
  cable/hitch compatibility for the latter four brands. Keep mod names stable.
- Publish the changes from local 0.3.2/0.3.3 builds with a higher version for
  update-check testing. In-game full loading, hitch alignment and multi-drop
  driving remain to be confirmed.

## 0.3.1 - Mod Manager presentation, test pre-release, 26 November 2026

- Add the approved SplitHaul tri-axle tanker artwork to both Mod Manager entries.
- Add the plain-text GitHub website near the top of both mod descriptions.
- Bundle ATS mod 1.1.5 and ETS2 mod 1.1.3, retaining existing mod filenames,
  display names, trailer resources and owned-combination compatibility.
- No dispatch behavior changes from 0.3.0.

## 0.3.0 — SplitHaul by grumpyman, test pre-release, 26 November 2026

- Rename the app, executable, installer, shortcuts and repository to SplitHaul.
- Retain the installer identity and existing data; back up the previous app.
- Update Shift+F9 registration and the GitHub update address. Existing users
  need to download this first renamed installer manually.
- Include multi-drop cargo splitting, grouped game modes, preferred distance,
  round trips, route/cargo delivery limits and ATS/ETS2 loaded-DLC filtering.
- Include ETS2 DLC city definitions and Iceland coordinates.
- Keep GT trailer mod identifiers compatible with purchased trailers.

Complete multi-drop/round-trip driving remains unverified. Historical release
assets below retain their original filenames for traceability.

## 0.2.11 — test pre-release, 25 November 2026

- Bundle the app, ATS/ETS2 telemetry plugins and both trailer mods in one
  installer, with separate optional tasks and data-folder selection.
- Preserve mod filenames and Mod Manager names across updates. Back up replaced
  mods outside the active mod directory; retain game mods on app uninstall.
- Include ATS mod 1.1.3 and ETS2 mod 1.1.1. Add ATS long triples and three-axle
  curtainsider, fuel and reefer road trains with two-axle dollies, and ETS2
  fuel, gas and food tanker doubles. Keep dealer cards limited to new choices.
- Accept attached owned combinations up to three cargo trailers in ATS or two
  in ETS2, excluding recognised converter dollies.
- Include history browsing/export improvements, profile-data management,
  selection/UI updates and manual update checking from local 0.2.9/0.2.10 work.
- Pass 517 app checks, both plugin suites (21 combination checks each), 1,061
  catalog/storage checks, 8 installer plugin checks, 13 mod installer checks
  and 47 isolated bundle integration checks. Verify both mod archives.

Newest ATS road-train menu/visual/driving tests, real elevated installation and
second-PC acceptance remain pending. This is an unsigned test build for exact
supported game executables; trailer mods target 1.62 and require matching DLC
where applicable.

## 0.2.8 — test pre-release, 23 November 2026

- Select the sole running ATS or ETS2 game automatically when the app opens,
  when a game starts, or when Shift + F9 brings the app to the front.
- Keep the selected single or multiple job type for the current game. Switching
  from the other game starts in single mode; a loaded profile can restore its
  saved preference.
- Add a prominent in-game Shift + F9 guide to the installer and project README.
- Keep ATS and ETS2 telemetry installation checked by default in Setup.

This build is still being tested. It is unsigned and requires an exact
supported game executable. No game plugin is installed or updated by reading
this repository; the installer performs those changes after validation.
