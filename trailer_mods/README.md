# GT Multi-Trailer Combinations — ATS / ETS2 1.2.1

## Selection repair — 1.2.1, 28 November 2026

All seven Australian chain diagrams now fit the native 256 x 64 icon area and
show wheels, drawbars and B-link platforms. ETS2 tanker lead accessories accept
the custom chassis, and food-tanker bodies provide their required cap defaults.
1,450 repair/preservation checks passed. The driver confirmed that selecting a
food-tanker Quad works after this update. Other combinations, purchase and driving
remain subject to further live testing.

The repair layer is `repair_road_train_selection.py`; it uses the shared
`road_train_icons.py` renderer after the 1.2.0 combination build.

## Australian combinations — 28 November 2026

Use the usual trailer dealer, select the trailer family, and open Customize →
Chain type. A-double, B-triple, AB-triple, A-triple, A-quad, BAB-Quad and ABB-Quad
are offered where the available chassis provide the required coupling layout.
The normal B-double choices remain. There are no additional dealer cards.

The packages include refrigerated/freezer, curtainside, tipper, livestock,
logging and flatbed combinations, plus compatible tankers, hoppers and
containers. New tri-axle livestock/logging leads in ATS and livestock/tipper
leads in ETS2 have compiled visible towing hitches. Converter dollies retain
their existing axle arrangements; this does not add a tri-axle dolly or exact
Australian manufacturer models. Matching DLC remains required for branded
trailers. Availability differs by family; not every family offers every layout.

Use SplitHaul 0.3.9 and its matching plugins for four-cargo-trailer dispatch.
ETS2 displays the red advisory `over B-Double not recommended` for combinations
above a B-double. Added combinations have no country-validity restriction;
existing route weight limits and the Full trailer loads setting still apply.
Australian replacement-map compatibility has not been tested.

`build_australian_combinations.py` builds from the retained 1.1.5 ATS / 1.1.7 ETS2
baseline in `research/australian-road-trains/before-1.2.0`, using locally installed
SCS assets. Pillow is needed for the original topology icons. The script stages
archives without changing game homes. `verify_australian_combinations.py`
independently checks packaged configuration/body-set discovery, physical
topology, cargo distribution, capacity, dolly recognition and compiled hitch
models. The latest reports are in `research/australian-road-trains/`.

103 ATS and 142 ETS2 body/configuration variants passed 5,308 independent
checks, in addition to the builder checks. Existing purchased-trailer resources
and dealer cards are byte-preserved. Preview, purchase, coupling appearance and
driving still require in-game testing; static verification is not a live test.

Definition format reference: [SCS unit names](https://modding.scssoft.com/wiki/Documentation/Engine/Units)
and [owned trailer configurations](https://modding.scssoft.com/wiki/Documentation/Engine/Units/trailer_configuration).

## Branded HCT accessory repair — ETS2 1.1.6

Live screenshots showed missing visible hitches on all four branded HCT leads.
The game log rejected their cable accessories: native SII definitions used a
tab before the colon in `suitable_for[]`, which the earlier parser missed.
Version 1.1.6 fixes those restrictions, puts the visible hitch first in each
lead's default accessories, and identifies it as a suitable factory accessory.
The common parser now accepts whitespace before the colon and checks these
native restrictions correctly. Existing meshes, coupling positions, trailer
configurations, dealer entries and mod names are unchanged.

314 repair/preservation checks passed. Re-select the GT HCT configuration to
refresh default accessories when testing. Visible hitch rendering and the
absence of cable conflicts still need an in-game retest. Evidence:
`research/hct-accessories-20261126/`; builder: `fix_ets2_hct_accessories.py`.

## Additional branded freezer HCTs — ETS2 1.1.5

Prepared four additional HCT choices in the existing trailer customization menus:
Tirsan SRI, Schwarzmuller Reefer, Kogel COOL and Krone Cool Liner. Select the
brand's refrigerated trailer, then **Customize > HCT > GT HCT**. Each requires
its own matching trailer DLC and uses two full-length three-axle cargo trailers
with a standard two-axle SCS converter dolly. No other trailer DLC is required
for these four choices. The Schmitz HCT from 1.1.4 remains unchanged.

Each new lead chassis retains its original mesh, materials, wheel positions and
collision file. The lead has a new towing joint and the existing visible SCS
coupling assembly, mounted at the native dolly's drawbar height. Kogel's existing
rear attachment locator was too high for this dolly and is lowered only on the
new GT lead. Required factory accessories get suitable aliases where needed.

571 configuration, model-roundtrip, accessory, axle, aggregate and archive
preservation checks passed. The four added HCT definitions have no country
validity restrictions. They add no dealer cards and preserve the stable mod
filename, display name, SplitHaul branding and all prior owned-trailer resources.
Purchasing, visual clearances, coupling and driving still need live game tests.

Build: `add_ets2_brand_hct.py`, on the verified 1.1.4 archive. Evidence:
`research/brand-hct-20261126/verification.json`. Version 1.1.5 was installed after
ETS2 closed, with the previous 1.1.4 archive backed up and the installed hash
verified. See `research/brand-hct-20261126/installation.json`. Existing compiled
installers and published releases have not been replaced.

## Schmitz S.KO COOL HCT — ETS2 1.1.4

In the Schmitz refrigerated trailer's normal customization menus, choose HCT,
then **GT HCT - 2 x 13.6 m Freezer (3 + 2 + 3 axles)**. The Schmitz DLC is
required. This adds two three-axle, full-length freezer trailers connected by
the native two-axle Schmitz converter dolly. No additional dealer card is added.

This added HCT definition has no country validity restriction. Native game
HCT definitions are not globally unlocked. Depot space and route suitability
still vary. The archive filename, Mod Manager name, SplitHaul icon and website
remain unchanged, and all earlier trailer resources are preserved.

The native dolly uses approximately 1.67 degrees of pitch to join the factory
couplings. Calculated wheel height changes are under 0.021 m versus its native
0.12 m suspension travel. These are geometry checks, not a physics simulation
or live driving confirmation. No native geometry or collisions were modified.
245 archive/configuration/accessory/geometry checks passed. Purchasing, actual
coupling, customization and driving still require an in-game test.
Evidence: `research/schmitz-hct-20261126/`.

Build with `add_ets2_schmitz_hct.py` on ETS2 mod 1.1.3. It adds only the new
configuration, body set, cargo definition and a recognised dolly alias with
compatible native accessory defaults. Installer package pins now select 1.1.4;
existing compiled installers and published releases have not been replaced.

## ATS three-axle road trains

ATS 1.1.3 adds **GT Road Train (3 x … ft, 3 axles each)** under the normal
Triple configuration choices:

- Curtainsider: three 45-foot three-axle trailers.
- Fuel tanker: three 48-foot three-axle trailers, one/four compartment bodies.
- Refrigerated trailer: three 53-foot three-axle trailers.

All three cargo trailers have native three-axle chassis and six physical wheel
locators each. They connect through two **two-axle** native converter dollies,
giving thirteen trailer/dolly axles in total (excluding the truck). The scanned
installed ATS base/trailer-DLC assets contain one- and two-axle dollies, not a
three-axle dolly. This release does not claim or model a three-axle dolly.
Native liftable-axle behavior is retained where the source chassis has it.

The custom box and curtainsider lead models add coupling and accessory locators
without changing the original three-axle mesh, wheel positions, materials or
collision model. Their visible coupling is the complete native ATS hook piece
from the box-trailer hook bumper, aligned to the physical joint. The fuel lead
uses its original coupling-equipped three-axle model. Restricted default
accessories receive dedicated suitability aliases for the new lead chassis;
no SCS definitions are overwritten. Existing owned-trailer resources and the
six dealer cards remain unchanged.

580 checks passed, including independently reopened compiled models, all
physical wheel locators, coupling alignment, cargo volumes, axle totals and
archive preservation. Visual appearance, menu selection and driving still need
in-game verification. Evidence: `research/ats-three-axle-20261125/`.

Build this layer last with `add_ats_three_axle.py`, followed by
`verify_ats_three_axle.py` (which packages only after verification succeeds).

## ATS long triples

ATS 1.1.2 adds **GT Long Triple** configuration choices to the existing SCS
trailer customization menus. Choose Triple, then the long configuration:

- Box: three 45-foot trailers; dry van, insulated or refrigerated bodies.
- Flatbed: three 45-foot trailers; wooden/aluminium deck or curtainside bodies.
- Fuel and food tankers: three 43-foot tanks.
- Container: three 40-foot trailers.

Each uses two native two-axle converter dollies. The cargo-trailer count is
three. No extra dealer cards are added; the six curated ATS cards remain.
ATS's existing Turnpike Double configuration provides the two-long-trailer
option. The new three-long-trailer choices use ATS's Triple category, not a
new HCT category.

The cleanup in 1.1.1 had incorrectly treated long triples as equivalent to
stock pup triples. This update restores that distinct choice through the
configuration menu rather than restoring duplicate dealer presets.
361 checks passed, including native model coupling points, body arrays,
aggregate capacity, unchanged dealer entries and preservation of existing
resources. The new menu selection still requires an in-game test.

Build this layer last with `add_ats_long_triples.py`, after the expansion and
dealer cleanup steps. Evidence and backup: `research/ats-long-triples-20261125/`.

## Clean dealer selection

Version 1.1.1 removes the excessive dealer-card expansion. Use the game's
existing trailer selections and Customize > Single/Double/B-Double/Rocky
Mountain/Turnpike/Triple menus wherever those options are already provided.

Only genuinely additional combinations have dedicated dealer entries:

- **ATS: 6** — gas tanker triple, bottom-dumper triple, end-dumper triple,
  dry-bulk triple, container triple and Lode King Prestige hopper triple.
- **ETS2: 4** — fuel tanker double, gas tanker double, food tanker double in
  chrome, and food tanker double in paint.

One representative is retained per new type, rather than a dealer card for
every length, axle arrangement or stock body variant. The retained entries are
unchanged from the previously validated packages. All previously supplied
trailer definitions, custom chassis, dolly aliases and models remain present,
so this cleanup does not remove dependencies of purchased GT combinations.
Only redundant dealer-preset files are removed. Profiles and saves are untouched.

Both installed archives were verified against their backups: 175 to 6 added
cards in ATS and 170 to 4 in ETS2; every retained preset and non-dealer resource
is byte-identical. The resulting in-game dealer appearance still needs checking.
Evidence and backups: `research/dealer-cleanup-20261125/`.

After any full expansion rebuild, run `tidy_dealer_selection.py` before
installation to apply this selection policy. Do not install the intermediate
unfiltered expansion package.

## Previous expansion inventory (not the current dealer-card count)

The expanded packages contain **175 ATS dealer presets** and **170 ETS2 dealer
presets**, including the previously installed tanker combinations. They cover
supported box, flatbed, container, dumper, hopper, dry-bulk and log combinations
where available, plus installed supported Lode King, Krone, Schmitz and
Feldbinder variants. ATS permits up to three cargo trailers; ETS2 permits two.
Dollies are excluded. See [COVERAGE.md](COVERAGE.md) for the exact families and
limitations. This does not add combinations for every single-only or Workshop
trailer.

**Mod Manager names:** GT Multi-Trailer Combinations - ATS / ETS2. The original
`GT_Tanker_Combinations_*.scs` filenames are retained so existing profile mod
activation and owned tanker references remain intact. Select complete previews
under Trailer Purchase > SCS. The prior tanker files are byte-for-byte unchanged
apart from package description/manifest; the expansion adds new definitions and
dealer presets. No stock definitions are overridden.

**Expansion verification:** 48,411 checks passed across the 335 newly added
presets; 393 referenced model files opened successfully. The six ATS and four
ETS2 existing tanker presets passed their separate checks as well. New entries
still require in-game preview, purchase and driving tests. Backups and install
evidence are under `research/expanded/`.

To rebuild the expansion after the tanker packages: run
`expand_trailers.py --extract --build`, `verify_expanded_trailers.py`, then
`expand_trailers.py --package`. Installed DLC assets are required for the
corresponding branded dealer entries; no Workshop mod is required.

## Tanker details and prior verification

Personal dealer add-ons for the installed ATS/ETS2 1.62 game assets.

| Package | Fuel | Gas | Food |
| --- | --- | --- | --- |
| ATS | Double and triple | B-double and B-triple | Double and triple |
| ETS2 | Double + dolly | Double + dolly | Double + dolly, chrome or paint |

The limits match Dispatch Companion 0.2.10: three cargo trailers in ATS, two
in ETS2. Standard SCS converter dollies are excluded from that count.

## Enable and select

1. Restart the game if it was running when the mod was installed.
2. In the chosen profile's Mod Manager, enable **GT Tanker Combinations - ATS**
   or **GT Tanker Combinations - ETS2**. Use the package for that game only.
3. **ETS2:** Open Trailer Purchase > SCS. Select a complete double-tanker
   thumbnail (fuel, gas, or food in chrome/paint) and purchase that preset
   directly. The dealer uses the usual SCS tanker names; identify the double
   by its two tanks and dolly in the preview. Changing to a stock chassis
   configuration replaces the combination.
4. **ATS:** Open Trailer Purchase > SCS and choose a complete double or triple
   tanker preview. There are six added entries: fuel double/triple, food
   double/triple and gas B-double/B-triple. Purchase the preset directly.
   Fuel/food triples contain three tanks and two dollies; dollies do not count
   towards the three-trailer limit. Gas combinations use fifth-wheel coupling
   chassis without converter dollies. Changing to a stock chassis configuration
   replaces the combination.
5. Purchase the combination and attach it through Trailer Manager. Start with
   a testing profile. Stop in Drive and press Shift+F9; check the count before
   dispatching a Single delivery. Multiple still means two delivery stops.

These packages supply the combinations; **GT Road Train Unlock is not required**.
They use new dealer/configuration, definition and lead-chassis names and do not
overwrite the stock trailers or the earlier road-train mod. Existing singles
remain available. The new GT definitions omit regional availability lists;
normal route capacity and compatible cargo checks still apply. Keep the mod
enabled while a profile owns its GT combinations; sell them before removing it.

## What was built and checked

ETS2's fuel, gas and food single chassis lack a rear `s_hook` locator. Separate
GT lead chassis add that connection at the standard dolly drawbar height,
retaining the original front hook, geometry parts, materials and collisions.
The original single-chassis models are not replaced. The rear tanker and dolly
use normal SCS definitions. ATS fuel/food combinations reuse its stock coupling
chassis; the gas B-triple repeats the stock B-double lead chassis.

Capacity uses the sum of the bodies and chassis. Cargo mass is shared between
the tank bodies, with zero cargo assigned to converter dollies. Each combination
carries a compatible cargo type; this does not create separate jobs or different
cargoes for each tank.

**ATS 1.0.1:** 1,294 dealer-chain, accessory, axle, capacity and cargo-route
checks passed across six complete presets. All 31 referenced native models
reopened, with 15 coupling links checked against native coupling geometry.
The package uses ATS stock chassis, bodies, wheels and default accessories;
it does not import the ETS2 food-tank caps or custom hitch models. The old GT
configuration selectors are removed in favour of complete dealer entries.
Previous installed ATS package is backed up in `research/ats-dealer-20261125/`.

**ETS2 1.0.2:** 331 package/model checks and 860 complete-dealer-chain checks
passed at installation. The shared dealer validator subsequently added cargo
route and orphaned-unit checks and passed 890 checks. Compiled
ETS2 models were reopened with ConverterPIX and their hook/part/material counts
checked. Both existing Dispatch catalogs include fuel, gas and food cargo with
pickup/delivery routes. No Dispatch executable or plugin change was needed.

**Version 1.0.0 crashed in ETS2's purchase/configuration flow.** The saved crash
shows a null chassis lookup inside the game configurator. Version 1.0.1 removes
the ETS2 GT chain-conversion selectors and supplies complete dealer presets,
following the structural approach of the locally installed SCS Gastank HCT
FFB Edition (Workshop 3300452705). No third-party preset or DLC assets are
bundled. Each preset explicitly includes chassis, body, data, paint, default
accessories and all wheels for the lead, dolly and rear.

User screenshots of 1.0.1 confirmed that the dealer previews now showed the
combinations, but exposed omitted food-tanker end caps and a missing visible
hitch. Version 1.0.2 explicitly adds matching chrome/paint front and rear caps
to both food tanks. It extracts the intact stock SCS box-trailer towing jaw and
mounting assembly into a separate visual accessory, retaining stock materials,
and places that accessory on the lead at the physical coupling point. No
third-party mod files or DLC assets are bundled. The locally installed SCS
Fueltank HCT Schmitz Edition (3300929078) was also inspected as a reference.

Check a **fresh dealer preview** after updating. Existing purchased trailers
keep their saved accessory lists; replacing the mod does not retrofit their
missing accessories. No save edits have been performed.

The user confirmed that the ETS2 1.0.2 appearance looks good. This confirms
the visual correction; it does not establish driving or cargo-loading results.
**ATS 1.0.1 preview/purchase and both games' driving/loading still require
in-game verification.** Evidence is in
`research/verification.json`, `research/dealer-verification.json`, and
`research/crash-20261125/`; build hashes are in `research/build.json`.
Installation and previous package backup are in `research/visual-fix-20261125/`.

## Rebuilding

`prepare_sources.py` extracts only the required definitions/models from the
locally installed game archives using the existing ConverterPIX installation.
`build_tanker_mods.py` uses the local copy of SCS Conversion Tools 2.21 to compile
the added ETS2 coupling locators, then writes the two `.scs` packages.
`verify_tanker_mods.py` checks package content and reopens the compiled models.
`verify_dealer_presets.py` validates the full ETS2 slave chain, every accessory
dependency, axle/wheel coverage, capacity and cargo-body compatibility.
Pass `--game ats` for the six ATS dealer presets. `verify_ats_models.py` opens
their native model dependencies and checks coupling heights against the stock
gas B-double reference (which has a 40 mm local-coordinate height difference).
Original game archives, saves, profiles and other mods are never edited.

SCS Software owns the original assets. These are personal local packages, not
a published release. References:
[SCS trailer/coupling definitions](https://modding.scssoft.com/wiki/Documentation/Engine/Units/trailer),
[owned-trailer configurations](https://modding.scssoft.com/wiki/Documentation/Engine/Units/trailer_configuration),
[SCS Conversion Tools](https://modding.scssoft.com/wiki/Documentation/Tools/Conversion_Tools).
