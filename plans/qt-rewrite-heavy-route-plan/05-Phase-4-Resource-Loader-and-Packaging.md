# Phase 4 — Resource Loader and Packaging

Read [00-Start-Here.md](00-Start-Here.md) first for the overall plan, workflow, and phase sequence.

## Mandatory route for new and resumed work

- Use the Heavy route for every task, slice, follow-up, and reopened gate in this phase. `Default route: Heavy` is a binding instruction.
- At each start or resume, explicitly select Heavy and reread this phase plan and [00-Start-Here.md](00-Start-Here.md). Follow the active `AGENTS.md` Heavy-route instructions and `~/.codex/codex_workflow/heavy_route.md`, including their delegation, verification, and deployment-state requirements.
- A new session, handoff, or context reset does not change the route. Do not continue under Light or Medium. If a required Heavy-route step blocks progress, report the blocker before implementation instead of silently switching routes.

## Build and test verification

- Use `build/windows-x64-debug` as the standard local Windows x64 Debug
  build folder for slice work. Reuse this same configured folder across
  slices and phases; do not create slice-, task-, or reviewer-specific build
  folders. Build the affected targets and their dependencies, and let the
  build system recompile changed or out-of-date inputs.
- If a fresh build is needed, empty the applicable standard build folder
  before configuring and building in it. For Windows x64 Debug, keep using
  `build/windows-x64-debug`; do not create a new folder for the fresh build.
  Apply the same rule when changing the build system, toolchain, or
  dependency configuration; when stale artifacts could explain a result;
  or when a phase gate explicitly requires clean-checkout evidence.
- A gate requiring another platform or configuration must use that CMake
  preset's standard `build/<preset-name>` folder and reuse it for that
  preset, rather than creating a slice-specific folder.
- Keep any phase-specific full builds, test suites, platform matrices, and
  packaging checks required by an exit gate. Record whether verification
  used an incremental or fresh build, which targets were built, and which
  tests ran. Describe focused results as focused; do not report them as a
  full-suite pass.

## Status

- Status: In progress
- Default route: Heavy
- Depends on: Phase 1
- Blocks: Bootstrap, feature migration, packaging, and memory gates
- Owner: Codex (Heavy route)
- Last updated: 2026-10-25
- Current note: F1485 closed and pushed the Batch 2 commit set from
  `Qt-Rewrite` to `origin` as `1475a46d` (25 commits, F1469–F1485). F1483 is
  committed as `6428b2ca`; local macOS verification installed the resource and
  probe components together and resolved `../Resources` to `Contents/Resources`.
  The staged audit passed with 193 files (187 SHA checks, five QMs, two QML
  keys). Focused CTest passed 3/3, and Linux guard fixtures passed 5/5.
  The F1483 workflow is intended to trigger on pushes to `Qt-Rewrite`, but
  first and subsequent authenticated Actions API queries found no run or checks
  for `1475a46d`; native results remain pending. Its root-resolver and prefix
  guard tests remain separate from F1486. The Linux guard rejects a changed
  `--prefix` only for mixed absolute/relative bindir/datadir; both-relative,
  both-absolute, same-prefix mixed, and `DESTDIR` cases remain allowed.
  F1484 is accepted as verification-only: the `ClassMngrNext` launch shell is
  a QtCore-only version printer with no tested storage or working-directory
  side effects. This finding applies to the shell/package, not every `src/next`
  adapter; legacy `NavigationController` still uses
  `LegacyDocumentContentResourcePort` to obtain the legacy `documents` pack.
  Keep legacy RCC implementation and pack inputs as the compatibility oracle
  through Phase 12. Calendar QML remains compiled via `qt_add_resources`, not
  an external pack. Batch 3 was pushed in two commits, from `1475a46d` to
  `a2e506a7`; F1486's workflow expansion is commit `094cda87`. Local `HEAD`
  and `origin/Qt-Rewrite` now both resolve to
  `1605bac36d4192bf027440a2b83dce8a5e2d99d1` (parent
  `a2e506a74c316bf902d6a4b5cd956970286471b3`, tree
  `12f431a05225f30aecb076fd88886403a2e0e8fc`). Actions API has no runs/checks
  for `1605bac3` and combined
  status is pending. The push trigger targets `Qt-Rewrite` and supports
  non-default branches; manual dispatch is unavailable because the workflow
  file is absent from default `main`. The reason no run surfaced is unknown;
  user direction is needed on extending scope to `main`. F1483 and F1486 native
  results remain unverified; see the tracker for F1486's exact target list.
  Phase 4 remains In progress pending both native gates and that direction.
  Phase 7/10 rendering parity remains deferred; legacy pack removal remains
  Phase 12. Batch 1 (F1464–F1468) is pushed to `origin`; see
  the [Phase 4 progress log](05-Phase-4-Progress-Log.md) for its receipt. See
  the [inventory](../../docs/qt-rewrite/phase-4-resource-inventory.md) and
  [current batch tracker](Current-Slice-Batches.md).

### Slice batches

Use the shared [current slice-batch tracker](Current-Slice-Batches.md) for live work when Phase 4 is active. It contains only the active
batch and the next batch once discovery starts; completed batches are
not archived there.

## Progress log

Use this section only when a decision, acceptance result, verification
finding, blocker, or exit-gate detail needs a durable record. Routine
slice chronology and closed batch history do not need to be archived;
keep current status in this plan and live batches in the shared tracker.

### F1464 — Effective-resource inventory — independently verified — 2026-10-25

The reproducible inventory records 229 tracked paths under `resources/` and
the two Calendar QML sources, for 231 unique records. The 226
`resources/assets` files total 65,707,627 exact bytes; all tracked resources
total 65,708,813 bytes including three Windows templates. The separate Calendar
QML inputs total 35,154 bytes. Each record carries a stable ID, source path,
byte count and SHA-256, v2 packaging disposition, optional owner, tier and
lifetime fields, streamability status, and installed/resident measurement
method. It separates `documents.json` metadata from 51 document bodies,
records the absent `roster-designs` payload, and excludes the Office lock
artifact from v2 as transient. The legacy documents RCC currently includes
that 165-byte artifact.

The independent Tester confirmed tracked-path completeness, hashes, unique
IDs, disposition counts, document/QML separation, and non-fabricated size
reporting. The four focused utility tests and generator `--check` passed. No
runtime build was needed for this inventory-only slice. Installed and decoded
sizes remain unknown pending the v2 package/runtime. At F1464, `dyb.png` had
no identified production feature caller; F1482 later recorded it as
`excluded_v2` without inventing a v2 feature owner. No installed pack overrides
were found in the application-owned data directory. F1464 was committed as
`473e8325b9f98069c8396291d057a398a9683ebe`.

### F1465 — Offline baseline resource baker — independently verified — 2026-10-25

The offline baker consumes the pinned F1464 inventory, verifies every source
path, type, length, and SHA-256, copies the 191 direct runtime resources, and
records the seven generated runtime inputs separately. It also records 31
build-only inputs and two v2 exclusions without copying them. The deterministic
manifest includes the inventory hash and resource/QML tree identities. Existing
output is checked against expected provenance derived from the inventory and
verified sources; a tampered output and edited self-asserting manifest are
refused and preserved. Unknown `resource_kind` values are rejected.

The 14 focused utility tests passed. Independent verification baked the
committed inventory twice to temporary directories; manifests and output trees
were byte-identical, and every direct output matched its inventoried source.
Temporary mutation checks confirmed refusal without replacement. This slice
does not yet install the tree or change CMake, release packaging, or runtime
loading. F1465 was committed as
`3423e1de05f129d407004192565a79f06e572579`.

### F1466 — Generated resource catalog — independently verified — 2026-10-25

The deterministic catalog generator projects the F1464 inventory and F1465
bake manifest into 198 runtime records: 191 direct files and seven generated
outputs (five translations and two Calendar QML components). Direct records
retain inventory IDs, types/categories, loading fields, and verified source
and baked hashes. Generated records carry explicit output names and locations
without fabricated output sizes or hashes. Catalog metadata remains separate
from the 51 on-demand document bodies; build-only inputs and v2 exclusions are
omitted.

The independent Tester ran the focused nine-test suite and twice generated a
fresh bake/catalog pair. Both 198-record catalogs were byte-identical, all
191 direct files matched their inventory source, and all generated output
records remained size/hash unknown. CMake source review confirmed the Calendar
QML module's URI and `/qt/qml` resource prefix. No CMake build or installed
package was exercised. F1467 must install file resources and preserve the
separate Qt-resource location for Calendar components; catalog generation
alone does not establish package presence. F1466 was committed as
`b10e60e854f21e67ed3b3653b8c8b608c7588345`.

### F1467 — Deterministic installed-tree packaging — independently verified — 2026-10-25

The new `ClassMngrNextResources` install component packages the catalog,
191 baked direct resources, and five generated translations at the F1466
catalog paths. It is excluded from the default install so the legacy release
continues to contain only its existing application and RCC packs. The two
Calendar components are embedded at their `qrc:/qt/qml/ClassMngr/Calendar/`
keys without adding QtQml/Quick linkage to the QtCore-only Next executable.
The selected roots are Windows `${CMAKE_INSTALL_BINDIR}/resources`, macOS
`ClassMngr.app/Contents/Resources`, and Linux
`${CMAKE_INSTALL_DATADIR}/ClassMngr` (normally `share/ClassMngr`).

Independent macOS verification built `ClassMngrNextResourcePackage`, staged
the component, and audited 197 files: 191 hashes, five generated QMs, two QML
keys, and no provenance manifest. A separate default install contained no v2
assets/catalog and retained all six legacy RCC packs. The Calendar resource
key and Next launch CTests passed 2/2; all 30 focused Phase 4 Python tests
passed. The generated macOS install rules and staged package were verified;
Windows and Linux paths were source-reviewed only and remain for the
cross-platform gate. Optional Qt deployment warnings about local database
drivers did not prevent the default install.

F1467 was committed as `153b85fb7b6314baf145ddd3f9584926111049d2`.

### F1468 — Typed resource loader — independently verified — 2026-10-25

The new `src/next/resources/` boundary loads known catalog IDs from an
explicitly injected catalog and resource root. It validates record types,
locations, relative paths and file extensions; package files are confined to
the canonical root and checked against catalog byte counts and SHA-256 values.
Calendar components remain `qrc:` resources. Catalog initialization reads no
document bodies. Text, JSON, images, fonts, stylesheets, translations, and
file-backed streams return owning values or RAII handles. The source-ownership
manifest and `ClassMngrNextResources` owner record preserve the exact-one-owner
check.

The independent macOS review configured the Debug preset and confirmed
ownership for 1,618 handwritten files. The focused loader build succeeded and
`ClassMngrNextResourceLoaderTests`, `ClassMngrNextCalendarResourceKeysTests`,
and `ClassMngrNextLaunch` passed 3/3. After the test was strengthened to verify
same-size SHA-256 corruption, translation removal, and stream closure, the
loader target rebuilt and its CTest passed 1/1. Temporary probes also verified
same-size PDF corruption and translation move/release. Windows and Linux builds
were not run; the Windows descriptor-to-native-handle assertion is
source-reviewed and awaits host execution. `clang-format` was unavailable.

F1474 adds `DocumentContentResourcePort` as an independent loader-owned
boundary in `src/next/platform/`, using `ResourceLoader` from
`src/next/resources/`; the legacy `NavigationController` remains on
`LegacyDocumentContentResourcePort` until consumer migration because the v2
Resources component is excluded from the default legacy install. Batch 1 is
pushed, with the receipt in the [Phase 4 progress log](05-Phase-4-Progress-Log.md).

### F1469 — Resolve installed resource root — independently verified — 2026-10-25

`ResourceRootResolver` returns the resource root and catalog path selected by
the platform install layout. It starts from the executable directory, uses
only the CMake-provided relative root, canonicalizes the selected directory,
and fails when the selected root or catalog is absent. It does not search the
working directory, source tree, environment, or alternate roots. CMake selects
`../Resources` for macOS bundles, `resources` beside the Windows executable,
and a Linux relative path derived from the executable install directory and
`${CMAKE_INSTALL_DATADIR}/ClassMngr`.

The independent macOS run rebuilt the resolver, loader, Calendar-resource, and
Next launch targets and passed their CTests 4/4. Resolver tests cover the
configured layout, missing root/catalog, an unrelated current-directory
resource tree, and relocation. Configure-time ownership validation passed for
1,620 handwritten sources. Windows and Linux were source-reviewed but not
host-built or staged. Install-time `--prefix` overrides with mixed absolute
and relative Linux install directories were not tested; the standard preset
uses relative install directories. The resolver remains an API without a Next
runtime caller while the shell is resource-free; F1480 checks launched
relocation. F1469 is accepted and committed as `d282e7f1`; F1470 is accepted.

### F1470 — Explicit resource scopes — independently verified — 2026-10-25

Every loader method now requires a `ResourceRequest` with one of the four
resource lifetimes: Startup, Page, Feature, or Operation. Request factories
reject unknown scope kinds and empty owner labels. Each result is a
move-only `ScopedResource<T>` that retains the scope value with its owned text,
JSON, image, font, stylesheet, translation, or file stream. Moving or
destroying the handle preserves the owner metadata and releases any contained
RAII resource normally. The implementation adds no global scope registry,
cache, diagnostics singleton, startup wiring, or legacy adapter change.

Independent macOS verification rebuilt the loader, root resolver, Calendar
resource-key, and Next launch targets; their CTests passed 4/4. Tests cover all
four scope kinds, blank-owner rejection, move construction and assignment,
font deregistration, translation removal, and stream-handle closure. The full
suite and Windows/Linux builds were not run. The Next shell remains unwired to
the loader until the later runtime verification slices. F1470 is accepted and
committed; F1471 follows.

### F1471 — Byte-budgeted resource cache — accepted — 2026-10-25

`ResourceCache` has an explicit positive byte capacity, deterministic LRU
eviction, and per-instance state. `ResourceLoader` accepts an injected cache;
there is no global cache. It retains decoded text, JSON, and images only.
Document bodies, streams, fonts, and translations bypass it. Values over the
budget are skipped without evicting unrelated entries. A cache hit returns a
new scoped handle for the current request while retaining the insertion owner
in cache metadata. Loader-local input buffers are released after each load.

The cache enforces its budget in **accounted payload bytes**, which excludes
map/list, allocator, and Qt object overhead. Text is weighted by UTF-16 payload
bytes, images by decoded pixel bytes, and JSON by compact UTF-8 serialization
bytes. The JSON value is a deterministic budget weight, not a measurement of
`QJsonDocument`'s internal allocation. Every load reads and verifies the
packaged bytes and catalog SHA-256 before a cache lookup, including cache hits;
a hit avoids decoding but still performs file I/O and hashing.

Independent macOS verification rebuilt the loader and root-resolver test
targets; `ClassMngrNextResourceLoaderTests` and
`ClassMngrNextResourceRootResolverTests` passed 2/2. Tests cover positive
capacity, accounted-byte limits, deterministic LRU recency, text/JSON/image
oversize skipping without unrelated eviction, per-cache isolation, image and
JSON cache hits with the current request scope, insertion-owner metadata,
cache-hit SHA-256 failure after a same-size source mutation, and font,
translation, stream, and document-body bypass. Windows and Linux were not run.
F1471 is accepted and committed as `8ca1ddc5`.

### F1472 — Per-loader resource diagnostics — accepted — 2026-10-25

Each `ResourceLoader` can receive an explicit `ResourceDiagnosticsSink`.
Every logical loader operation emits one `ResourceDiagnostic`, including
failures; stylesheet delegation does not emit a duplicate. Events carry the
resource ID and current scope, monotonic elapsed time across the full call,
catalog source bytes, optional decoded payload bytes, outcome/error, cache-hit
state, and cache-entry, aggregate, and capacity accounted bytes when a cache
is available. Text, JSON, and image byte weights follow F1471. Fonts,
translations, and streams have no decoded-size measurement, and streams are
not read just to calculate one.

The sink is per loader and called synchronously after cache metrics are
snapshotted and outside the cache lock. Concurrent calls on one loader may
invoke it concurrently, so sink implementations must be thread-safe. Sink
exceptions are swallowed and cannot replace loader results. This adds no
process-wide singleton or startup/legacy wiring.

The macOS loader and root-resolver targets built with ownership validation for
1,625 handwritten sources; both focused CTests passed 2/2, and
`git diff --check` passed. Tests cover cache miss/hit and current-owner
reporting, source/decoded sizes and cache metrics, SHA-256 failure events, type
mismatch, stream reporting without body sizing, UTF-16 text and image weights,
font/translation events, sink isolation and exception handling, and stylesheet
event cardinality. Windows, Linux, and the full suite were not run. F1472 is
accepted and committed as `9ce7e35c`.

### F1473 — Load document catalog metadata — accepted — 2026-10-25

`ResourceDocumentCatalogPort` loads only `document.catalog-metadata` through
`loadJson` with Startup owner `DocumentCatalogMetadata`. It parses schema v1
into the bounded `DocumentCatalogProjection`, preserving seven folders, 30
documents, localized/default names, ordering, print/export flags, and
references. It is not wired to Next startup. The isolated-package test stages
only the generated catalog manifest and `documents.json`, with all document
bodies absent; it confirms one successful Startup diagnostic for the metadata
ID and no body request.

Independent macOS verification built `ClassMngrNextResourceDocumentCatalogTests`
incrementally (up to date; objects newer than sources), passed its focused
CTest 1/1, reviewed CMake ownership/dependency inclusion, and passed
`git diff --check`. Windows/Linux and malformed-metadata/error branches were
not verified; the full suite was not run. F1473 is committed as `2084cb4f`.

## Objective

Remove the resource-pack architecture and create a typed, observable resource loader that loads only what is required for the current application state.

## Product decision

Resources are delivered as part of the normal application package. A normal application update replaces the executable and its installed resources together.

There will be:

- No external or legacy runtime RCC-pack mounts; retain F1467's two Calendar
  QML resources compiled into `ClassMngrNext` with `qt_add_resources`.
- No runtime resource-pack directory.
- No resource-pack manifest download.
- No resource-pack signature download.
- No resource-pack update check.
- No resource-pack preferences.
- No resource-pack leases.
- No production source-directory fallback.

The no-pack policy applies to the new `ClassMngrNext` runtime and its package.
The legacy `ClassMngr` executable remains a temporary compatibility oracle
through Phase 12 and may keep its existing RCC implementation and package
inputs until cutover. It is not a fallback for the new loader. Remove the
legacy pack implementation and its deployment behavior when the migrated
application replaces that oracle.

## Work packages

### 4.1 Effective-resource inventory

Inventory all current resources:

- Icons.
- Styles.
- Translations.
- Fonts.
- Documents.
- Document catalog metadata separately from PDF/PPTX document bodies.
- Campus data.
- Campus maps.
- Templates.
- Roster designs.
- Speaking-evaluation report art.
- Calendar QML.
- Splash assets.

For every resource record:

- Logical identity.
- Installed size.
- Expected decoded or resident size.
- Owning feature.
- Startup necessity.
- Loading method.
- Expected lifetime.
- Whether it can be streamed.
- Whether it can be discarded after use.

### 4.2 Offline resource baking

Before deleting the runtime pack system, create an offline build utility that:

1. Resolves the currently effective baseline and approved installed resource content.
2. Extracts the content into canonical files.
3. Validates required paths and types.
4. Produces a build-time resource inventory.
5. Makes the canonical resources part of the application release.

This preserves content that users may currently receive through resource packs without preserving the runtime update mechanism.

### 4.3 Installed resource tree

Use a deterministic application-owned tree such as:

    resources/
    ├── core/icons
    ├── core/styles
    ├── core/translations
    ├── core/fonts
    ├── campuses
    ├── documents
    ├── templates
    ├── roster-designs
    ├── speaking-eval
    └── calendar

Use the correct platform installation location:

- Windows application resources.
- macOS bundle Resources.
- Linux application data directory.

The deterministic v2 resource root selected for F1467 is:

- Windows: `${CMAKE_INSTALL_BINDIR}/resources` beside the executable (the
  Windows preset uses `.` for `CMAKE_INSTALL_BINDIR`).
- macOS: `ClassMngr.app/Contents/Resources` in the final application bundle.
- Linux: `${CMAKE_INSTALL_DATADIR}/ClassMngr` (normally `share/ClassMngr`).

Install the v2 tree through the separate `ClassMngrNextResources` component,
excluded from the default install while the legacy `ClassMngr` package remains
the oracle. Put `resource-catalog.json` at the resource root beside `assets/`.
Copy catalog `resource_file` entries to their package-relative paths. Calendar
QML entries remain Qt resources at their `qrc:/qt/qml/ClassMngr/Calendar/...`
URLs and are not copied as loose files.

The loader must resolve only known package-relative paths. It must not search the source tree in a packaged build.

### 4.4 Loader interfaces

Create:

- ResourceId: typed logical identifier.
- ResourceDescriptor: path, category, expected type, and loading policy.
- ResourceCatalog: generated inventory of installed resources.
- ResourceLoader: text, JSON, image, font, stylesheet, translation, and stream loading.
- ResourceScope: startup, page, feature, and operation lifetime.
- DocumentContentHandle (or equivalent): explicit open/close ownership for a
  requested PDF/PPTX body, separate from catalog metadata.
- ResourceCache: explicit byte budget and LRU eviction.
- ResourceDiagnostics: load time, decoded size, cache residency, and owner.

Return owning objects or handles with explicit lifetimes. Do not return a path whose validity depends on hidden mounting state.

### 4.5 Loading tiers

Core startup resources:

- Application icon.
- Selected translation.
- Active theme stylesheet.
- Required UI fonts.
- Small navigation and action icons.
- Minimal catalog metadata.
- Minimal initial-workspace metadata.

Feature resources:

- Campus details and maps.
- Document viewer chrome and selected-entry state.
- Roster designs.
- Speaking report artwork.
- Calendar QML assets.
- Feature-specific templates.

Viewer/session resources:

- PDF content loaded into QtPdf only for the active requested document.
- Any rendered-page or decoded-content cache, only with an explicit byte/item
  budget and release policy.

Operation resources:

- PDF source content for print, export, or other document operations.
- Print-only assets.
- Report export images.
- PowerPoint workspaces.
- Temporary import/export buffers.

Optional decorative fonts must be loaded only when a feature uses them.

The [Qt Rewrite Memory Hotspot Remediation
Plan](memory-hotspot-remediation-plan.md) governs the resident-resource
consequences of these tiers. Large resources, decoded images, document
content, report assets, and operation buffers must have one named owner,
explicit release behavior, and a byte or item budget where they are cached.

### 4.6 Build and deployment removal

Remove production code, tests, options, and deployment behavior for:

- Resource-pack manifests.
- Resource-pack signatures.
- Resource-pack storage.
- Runtime RCC registration.
- Resource-pack leases.
- Resource-pack downloads.
- Resource-pack startup checks.
- Resource-pack settings.
- Resource-pack update dialogs.
- Splash resources and splash paths.

### 4.7 Existing installation handling

The new application must not mount old resource packs. Initially, old application-owned pack directories may be left untouched and ignored.

If an installer migration later removes them, it must:

- Target only the known application-owned directory.
- Verify canonical resources are installed first.
- Log what was removed.
- Never touch workspace files or user documents.

## Deliverables

- ResourceCatalog.
- ResourceLoader.
- ResourceCache.
- ResourceDiagnostics.
- Offline resource-baking utility.
- Canonical installed resource tree.
- Updated CMake and deployment rules.
- Resource-loader unit and integration tests.
- Resource-pack removal checklist.

## Exit gate

The application starts without mounting or downloading an external/legacy
resource pack, performs no resource update request, and displays the same
resource-backed content as the baseline. F1467's two compiled-in Calendar QML
resources remain.

Large resources are not loaded or decoded until their owning feature or operation requests them.

Document catalog initialization must not load PDF bodies or call
`QPdfDocument::load()`. A viewer/session request opens one selected document;
ending that session closes the QtPdf document and releases its content handle.

### Phase 4 gate scope

The Phase 4 application target is the `ClassMngrNext` resource runtime. It is
still a console shell and has no migrated feature pages. Phase 4 therefore
verifies baseline resource identity and content at the catalog/loader boundary,
along with no-pack startup and explicit load/release behavior. User-visible
rendering parity for migrated feature pages remains a required Phase 7/10 gate;
the legacy `ClassMngr` display alone is not evidence that the new loader passes
that later gate. Legacy theme, font, icon, campus, and report UI consumers
remain in their Phase 5–7 startup, shared-UI, and feature migrations; Phase 4
does not alter those parity-oracle paths.

## Heavy-route requirements

- For every Phase 4 slice, use the heavy route: trace the resource from its
  packaged source through loading and release, verify the required lifetime,
  and remove any temporary fallback when the slice is accepted.
- Do not replace resource packs with another hidden network-backed pack format.
- Do not preload all documents, templates, maps, or fonts.
- Do not load PDF content while building the catalog or starting the
  application, and do not retain a QtPdf document after its viewer session
  ends.
- Do not keep both raw bytes and decoded objects beyond the required lifetime.
- Do not use source-directory fallback in a packaged build.
- Do not remove visual assets merely to meet the memory target.
