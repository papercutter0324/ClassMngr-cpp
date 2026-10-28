# Phase 4 Progress Log

Implementation, verification, and gate history is recorded here. The Phase 4
plan retains current status, requirements, and product decisions.

## Batch 1 — F1464–F1468 — pushed 2026-10-25

Batch 1 was pushed from branch `Qt-Rewrite` to `origin` after F1468 passed its
independent loader review. The push advanced the remote from `58de51bf` to
`eb473fab`:

```text
git push origin Qt-Rewrite
58de51bf..eb473fab  Qt-Rewrite -> Qt-Rewrite
```

Commits in the pushed batch:

- F1464 — `473e8325b9f98069c8396291d057a398a9683ebe`
- F1465 — `3423e1de05f129d407004192565a79f06e572579`
- F1466 — `b10e60e854f21e67ed3b3653b8c8b608c7588345`
- F1467 — `153b85fb7b6314baf145ddd3f9584926111049d2`
- F1468 — `eb473fab89410e6dc5f5527d872b56f150f7f6fd`

Detailed acceptance records for F1464–F1468 follow. Windows and Linux
resource staging remained for F1483; Batch 1 packaging was host-staged on
macOS.

## F1464 — Effective-resource inventory — independently verified — 2026-10-25

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

## F1465 — Offline baseline resource baker — independently verified — 2026-10-25

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

## F1466 — Generated resource catalog — independently verified — 2026-10-25

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

## F1467 — Deterministic installed-tree packaging — independently verified — 2026-10-25

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

## F1468 — Typed resource loader — independently verified — 2026-10-25

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

## F1469 — Resolve installed resource root — independently verified — 2026-10-25

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

## F1470 — Explicit resource scopes — independently verified — 2026-10-25

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

## F1471 — Byte-budgeted resource cache — accepted — 2026-10-25

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

## F1472 — Per-loader resource diagnostics — accepted — 2026-10-25

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

## F1473 — Load document catalog metadata — accepted — 2026-10-25

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

## F1474 — Loader-owned document content handles — accepted — 2026-10-25

`DocumentContentResourcePort` is an independent Next loader boundary. It opens
only the selected PDF body through `ResourceLoader` under `ViewerSession`
scope and returns a move-only stream with explicit close and no pack fallback.
The optional export reference is not opened at primary selection. The port is
not wired to legacy UI: `NavigationController` continues to use
`LegacyDocumentContentResourcePort` because the Phase 4 v2 Resources component
from F1467 is excluded from the default legacy install. Preserve that legacy
viewer path until consumer migration. F1475 verifies QtPdf release in an
isolated test and does not wire the port into the production viewer.

The initial attempt to connect the new port to legacy navigation failed three
parity cases because the default install has no v2 resources. Keeping the port
independent resolved that boundary mismatch. Independent macOS Debug review
built the port, ResourceLoader, catalog, document-catalog, navigation-parity,
and PageManager targets incrementally; the port, loader, catalog, parity, and
PageManager targets were up to date, while the document-catalog target rebuilt
and linked. Focused CTest passed 4/4; four affected PageManager PDF lifecycle
slots passed individually, and the final port-test delta passed 1/1. Coverage
includes unsafe-reference rejection before load, selected PDF ID and
ViewerSession scope, one body request, explicit close, no pack fallback,
non-PDF/wrong-category/wrong-kind references, wrong catalog package path,
missing body, and parser rejection of `expected_type` inconsistent with the
`.pdf` extension. The adapter expected-type check was source-reviewed because
`ResourceCatalog` prevents loading a mismatched type/path record.

The full PageManager suite stalled in `registeredPagesAreCreatedOnFirstUse`;
the affected PDF cases passed individually. Windows/Linux were not run, and
`git diff --check` passed. F1474 is committed as `34926f4b`.

## F1475 — QtPdf session-release verification — accepted — 2026-10-25

`NextPlatformQtPdfSessionRelease` exercises the F1474 loader-owned stream only
in an isolated test. It loads the selected PDF from the borrowed stream device,
waits for QtPdf Ready/page count, verifies that closing the document reaches
Null while the stream remains open, then explicitly closes/releases the first
handle and opens again with a fresh handle and `ViewerSession` generation.
Destroying the active owner verifies `document-null-while-stream-open` before
`stream-about-to-close`. This does not wire the port into the production
viewer; F1474's independent Next boundary and retained legacy adapter remain
unchanged.

The standard `build/macos-clang-debug` incremental target succeeded. Independent
build review saw the package catalog regenerate and the test target remain up
to date; focused CTest passed 1/1, and `git diff --check` passed. The full suite
and Windows/Linux were not run. Existing cache metadata reports Qt Core 6.12.0
and QtPdf 6.140.0; configure, build, link, and test succeeded without an
observed compatibility failure. The metadata mismatch remains unresolved, and
fresh cross-platform compatibility is not established. F1475 is committed as
`8a2163d2`.

## F1476 — Scope Office exports to operations — accepted — 2026-10-25

`DocumentExportResourcePort` validates production document-export references,
maps them to `document.body:<path>`, and requires a cataloged
`documents`/`document_body` PPTX or DOCX descriptor with a matching package
path. It requests only that body under explicit Operation scope and returns a
move-only loader-owned stream with explicit close and RAII release. It has no
`ResourcePackManager` fallback or startup preload and is not wired to legacy
report/workspace UI. Primary document selection still does not open export
bodies. The legacy Speaking Evaluation workspace and cleanup tests were
untouched. F1482 later classified the three callerless roster workflow files
and `dyb.png` as `excluded_v2`.

`cmake --build --preset macos-clang-debug` passed incrementally on the standard
macOS universal Debug configuration. Focused export/PDF port build and CTest
passed 2/2; after the RAII pointer assertion delta, independent export-port
CTest passed 1/1. Tests confirm metadata/catalog setup requests no bodies,
unselected Office bodies are absent, selected PPTX and DOCX each emit one
Operation diagnostic, explicit close and owner destruction release handles,
and malformed, non-Office, or missing inputs fail without fallback.
`git diff --check` passed. Windows/Linux and the full suite were not run. F1476
is committed as `2bbbd23d`.

## F1477 — Define resource failure behavior — accepted — 2026-10-25

Direct lookup of an unknown catalog ID returns `ResourceError::NotFound` with
one failed diagnostic. Same-size SHA-256 corruption of the selected PDF or
PPTX, exercised through the document-content and document-export `openStream`
ports while an alternate body is available, returns no handle, maps to
nonrecoverable `Domain::Validation`, and emits exactly one failed
`CorruptResource` diagnostic for the selected body. Existing missing-file,
missing-record, type-mismatch, invalid-catalog, and no-pack-fallback coverage
remains. This is a test-only slice; no production or legacy code changed.

The incremental macOS Debug build of `ClassMngrNextResourceLoaderTests`,
`ClassMngrNextPlatformDocumentContentResourcePortTests`, and
`ClassMngrNextPlatformDocumentExportResourcePortTests` succeeded with all
targets up to date; the resource catalog regenerated. Independent focused
CTest passed 3/3, and `git diff --check` passed. This verifies macOS Debug only;
the full suite and Windows/Linux were not run. F1477 is committed as
`535dc382`.

## F1478 — Verify type-specific lazy loads — accepted — 2026-10-25

This verification-only acceptance reuses F1468's existing `testLoader()`;
independent review found no additional test code was needed, so no separate
source-code delta was required. The test stages a temporary package with
selected representative examples and exercises these APIs at request time:
text (Calendar QML), JSON, image, font, stylesheet, translation, and stream.
Unselected and unrelated document bodies remain
absent. Assertions cover current scope through owned/moved handles,
font/translation release, stream file closure, and wrong-type rejection.

Source review confirms `ResourceCatalog` parses manifest metadata only and
`ResourceLoader` construction does not open package bodies. There is no new
loader or document-port use under legacy app/UI/feature paths; those consumers
remain on pack paths.

The independent macOS Debug build of `ClassMngrNextResourceLoaderTests`
succeeded; the target was up to date and the resource catalog regenerated.
Focused CTest passed 1/1, and `git diff --check` passed. The full suite and
Windows/Linux were not run. F1478 is accepted as verification-only.

## F1479 — Verify no-pack startup side effects — accepted — 2026-10-25

`ClassMngrNextLaunchFilesystemSideEffectsTests` launches the actual executable
from a fresh working directory with isolated HOME and XDG config/data/cache/state
roots; the test setup defines Windows profile equivalents. It verifies normal
exit, the exact version line, no created storage roots, and a recursive scan
of the working directory with `AllEntries|Hidden|System|NoDotAndDotDot` finding
no files or directories.
The existing `ClassMngrNextLaunch` gate remains in place.

Source and CMake review confirms `src/next/main.cpp` only constructs
`QCoreApplication`, sets app name/version, prints the version, and exits;
`ClassMngrNext` keeps its QtCore-only direct link. Startup has no resource-port
request, `ResourcePackManager`, settings/storage creation, explicit external or
legacy runtime RCC-pack mount, or splash path. This review retains F1467's two
Calendar QML resources embedded with `qt_add_resources`: they are compiled-in
resources, not runtime registration of an external pack. The subprocess test
proves filesystem behavior; the source/dependency review supports the other
claims because there is no global loader-call counter.

Standard macOS Debug CMake regeneration completed in about 99 seconds with
developer-policy warnings and no configure failure; the incremental focused
build succeeded. An independent rebuild after the hidden/system scan change
found the target up to date. Focused CTest passed 2/2 (`ClassMngrNextLaunch` and
`ClassMngrNextLaunchFilesystemSideEffectsTests`), and `git diff --check` passed.
The full suite and Windows/Linux were not run. No production startup code
changed. F1479 is committed as `295d238d`.

## F1480 — Verify relocated package lookup — accepted — 2026-10-25

The macOS Debug-only probe copies the executable into a temporary
`.app/Contents/MacOS` and stages only the generated catalog and
`assets/campuses/bundang.json` under the selected `Contents/Resources`. Launched
from an unrelated working directory, the probe calls the actual
`ResourceRootResolver::resolve()`, selects the executable-relative root, and
loads the integrity-checked JSON through `ResourceCatalog` and `ResourceLoader`.
A negative child has no selected root but valid catalog/file decoys in its
working directory and alternate app roots; it fails with `MissingFile` rather
than using those decoys. The probe is test-only; production startup remains
unwired to the resolver/loader. F1469's resolver tests remain in place.

The standard `build/macos-clang-debug` incremental build of
`ClassMngrNextResourceRootResolverTests` and
`ClassMngrNextRelocatedResourceRootTests` succeeded; both targets were already
built and catalog generation was the only build action. Independent focused
CTest passed 2/2, and `git diff --check` passed. This verifies macOS Debug only;
the Windows/Linux install-layout matrix remains for F1483. The full suite was
not run. F1480 is committed as `cc60849d`.

## F1481 — Audit staged resource completeness — accepted — 2026-10-25

The standard macOS universal Debug incremental build of
`ClassMngrNextResourcePackage` succeeded. It freshly installed the
`ClassMngrNextResources` component under
`build/macos-clang-debug/installer-stage/f1481`, and audited
`ClassMngr.app/Contents/Resources`. The catalog has 198 records: 196
`resource_file` entries (191 direct files and five generated QMs) plus two
`qt_resource` entries. The staged tree has 197 physical files—the catalog and
196 resource files—with no symlinks or manifest. The audit passed all 191 SHA
checks and confirmed five nonempty QMs and the two exact Calendar QML keys.
Those QML keys are compiled resources, not staged files. Independent Tester
confirmed the counts and audit.

F1481 is verification-only; no code or implementation-document delta was
needed. Focused Calendar key CTest passed 1/1, and `git diff --check` passed.
The full suite and Windows/Linux were not run; F1483 owns the platform layout
matrix. This acceptance record is the F1481 slice commit; Batch 2 remains open.

## F1482 — Classify callerless resource assets — accepted — 2026-10-25

F1482 marks four tracked callerless assets `excluded_v2`: `dyb.png`,
`Class Register.odt`, `Roster Template 1 - By Day.ods`, and
`Roster Template 1 - By Day.xlsx`. Their exact inventory IDs, paths, source
sizes, and hashes remain for provenance; they stay in the legacy resource-pack
oracle and are omitted from the v2 package. No `resources/` source changed.
`roster-designs` remains a zero-source absence observation without placeholder
records. The inventory has 231 records: 187 direct runtime resources, seven
generated runtime resources, six v2 exclusions, and 31 build-only inputs. The
226 asset files total 65,707,627 bytes; all tracked resources total 65,708,813
bytes, and the two separate Calendar QML inputs total 35,154 bytes. The 194
runtime catalog records comprise 187 direct files, five generated QMs, and two
compiled QML keys.

Independent macOS Debug verification passed all 32 Phase 4 Python utility
tests, inventory `--check`, the staged-resource audit (193 files, 187 SHA
checks, five generated QMs, and two Calendar QML keys), and Calendar key CTest
1/1. QML keys are compiled resources, not staged files. Windows/Linux and the
full suite were not run; F1483 owns the platform matrix. F1482 is accepted and
Batch 2 remains open.

## F1483 — Verify platform install roots — committed — 2026-10-25

F1483 is committed as `6428b2ca`. The native install-root workflow is
triggered by pushes to `Qt-Rewrite`. Independent macOS verification installed
the resource and probe components together; the installed probe resolved
`../Resources` to the app's actual `Contents/Resources`. The stage audit passed
193 files, 187 SHA checks, five generated QMs, and two Calendar QML keys.
Focused CTest passed 3/3, and the Linux install-prefix guard fixtures passed
5/5. The guard rejects a changed `--prefix` only when bindir and datadir are
mixed absolute/relative paths; both-relative, both-absolute, same-prefix mixed,
and `DESTDIR` cases remain permitted.

At F1483 acceptance, the Windows/Linux Actions legs awaited the Batch 2 push;
their current post-push status is recorded under F1485. The local macOS
verification does not claim native results.

## F1484 — Verify Next launch removal boundary — accepted — 2026-10-25

F1484 is verification-only; no implementation delta was needed. Independent
macOS Debug CTest passed 2/2 (`ClassMngrNextLaunchFilesystemSideEffectsTests`
and `ClassMngrNextLaunch`). Source review confirms `src/next/main.cpp` is a
`QCoreApplication` version-print shell and `cmake/next.cmake` links it only to
Qt Core. The test confirms normal exit, exact version output, and no storage-root
or working-directory side effects. This evidence applies to the
`ClassMngrNext` shell/package, not every `src/next` adapter: legacy
`NavigationController` still uses `LegacyDocumentContentResourcePort`, which
obtains content from the legacy `documents` pack.

Keep the RCC implementation and pack inputs as the legacy compatibility oracle
through Phase 12. Calendar QML is compiled with `qt_add_resources`, not an
external pack. Windows/Linux are not verified by this slice; F1483's native
package/layout legs and the planned F1486 checks remain open.

## F1485 — Assemble Phase 4 evidence; close Batch 2 commit set — accepted — 2026-10-25

F1485 assembles the accepted evidence and closes the Batch 2 commit set; it does
not close Phase 4. The evidence map is: F1464–F1467 inventory, catalog, and
package; F1468 and F1470–F1478
loader contracts, scopes, cache/diagnostics, document loading, and failures;
F1474/F1475 selected PDF session release; F1479/F1484 no-pack shell behavior;
F1481/F1482 staged completeness and exclusion provenance; and F1483 local
macOS package/root verification plus guard fixtures.

F1485 is committed as `1475a46dae77d8970852681dae7a2032ff907edf`. Batch 2
was pushed successfully from `Qt-Rewrite` to `origin`; the push advanced
`Qt-Rewrite` from `eb473fab89410e6dc5f5527d872b56f150f7f6fd` to
`1475a46dae77d8970852681dae7a2032ff907edf`. The batch contains 25 commits,
from F1469 `d282e7f1` through F1485 `1475a46d`. At that push, `HEAD` and
`origin/Qt-Rewrite` both resolved to the new full SHA. F1486 later added its
workflow commit `094cda87`; the Batch 3 receipt follows below.

The push was intended to trigger F1483's Windows/Linux Actions legs. First and
subsequent authenticated GitHub Actions API queries found no workflow run or
checks for head `1475a46d`; no run or pass is claimed, and native results remain
pending. F1486 was queued for Batch 3 at this point; its committed workflow
expansion and subsequent push are recorded below.

Batch 2 closure is separate from the Phase 4 exit: Phase 4 remains In progress
until F1483's native jobs and F1486 pass. Phase 7/10 rendering parity remains
deferred. Legacy RCC implementation and pack inputs remain the compatibility
oracle until Phase 12.

## F1486 — Add native resource lifetime checks — committed — 2026-10-25

F1486 is committed as `094cda87` and expands the Batch 3 workflow using
existing tests only. After the Batch 3 push, the Windows x64 Debug and Linux
GCC Debug jobs will run:

- `ClassMngrNextResourceLoaderTests` (native handle/file-descriptor close)
- `ClassMngrNextPlatformDocumentContentResourcePortTests`
- `ClassMngrNextPlatformDocumentExportResourcePortTests`
- `ClassMngrNextPlatformQtPdfSessionReleaseTests`
- `ClassMngrNextLaunchFilesystemSideEffectsTests`
- `ClassMngrNextLaunch`

Linux QtPdf is mandatory; the job fails clearly if QtPdf is unavailable. F1483's
root-resolver and install-prefix guard tests remain in its separate package and
layout workflow. Independent workflow review passed. Batch 3 was pushed
successfully from `Qt-Rewrite` to `origin` in two commits, advancing
`1475a46dae77d8970852681dae7a2032ff907edf` to
`a2e506a74c316bf902d6a4b5cd956970286471b3`; the commits are F1486 workflow
`094cda87` and documentation `a2e506a7`. Local `HEAD` and `origin/Qt-Rewrite`
matched at the new tip at the time of that receipt. Current local `HEAD` and
`origin/Qt-Rewrite` both resolve to
`1605bac36d4192bf027440a2b83dce8a5e2d99d1`, parent
`a2e506a74c316bf902d6a4b5cd956970286471b3`, tree
`12f431a05225f30aecb076fd88886403a2e0e8fc`. This is the same tree and parent
as the former local receipt commit `9fe1f26fe3ace50996d58b87cf045f9afab31afb`,
which has been superseded by the published commit. Actions API queries found
no runs/checks for Batch 2 head `1475a46d`, Batch 3 head `a2e506a7`, or current
head `1605bac3`; combined status for `1605bac3` is pending. No run or pass is
claimed. F1483 and F1486 native results remain pending.

## F1487 — Reconcile missing native workflow runs — committed — 2026-10-25

F1487 is committed as `f86c79fc81a2b2fd34c6993509b4cd314aacaec1`. The F1483
push filter targets `Qt-Rewrite`, and push workflows can run on non-default
branches. The repository default is
`main`, where this workflow file is absent; `workflow_dispatch` requires the
workflow file on the default branch. No dispatch tool is exposed. These facts
do not explain why no push run or checks surfaced, and credential suppression
or Actions settings remain hypotheses only. Do not infer a cause. F1483/F1486
native results remain open; user direction is needed on whether to extend
workflow scope to `main`. Phase 4 remains In progress pending native jobs and
that direction.

## F1488 — Correct stale loader catalog assertions — accepted — 2026-10-25

The initial standard `macos-clang-debug` run of six targeted F1486 CTests
passed 5/6; only `ClassMngrNextResourceLoaderTests` failed. The generator
classifies the 194 catalog records as 187 runtime-packaged plus 7 generated.
C++ `directCount` counts `PackageFile` records: the correct counts are 194
total (192 `PackageFile`, 2 `QtResource`). Five generated QM files are included
among the 192 package files; the other two generated records are Qt QML. The
pre-correction assertions at `cmake/tests/next_resource_loader.cpp:333`
expected 198 total and 196 `PackageFile`. Explorer confirmed these counts were
stale: F1482's four `excluded_v2` records account for the 198-to-194 change.
Executor corrected the counts while preserving coverage; its standard macOS
Debug build and focused CTest passed 1/1. Independent Tester verified the
standard `macos-clang-debug` build and reran all six targeted F1486 CTests;
6/6 passed. F1488 is accepted. Windows/Linux Actions results remain required
and open; macOS acceptance does not replace them.

Batch 4 was pushed successfully to `origin/Qt-Rewrite` in two commits, from
`1605bac36d4192bf027440a2b83dce8a5e2d99d1` to
`ea46415a93e8285e4cb1f81e9aec5b79f7533286`: F1487
`f86c79fc81a2b2fd34c6993509b4cd314aacaec1` and F1488
`ea46415a93e8285e4cb1f81e9aec5b79f7533286`. Local `HEAD` and
`origin/Qt-Rewrite` matched at the new tip at that time. Actions API for this
exact head found zero runs and zero check-runs; combined status is pending. No
workflow result is claimed. F1483/F1486 Windows/Linux native results remain required and open; the
user's direction on extending workflow scope to `main` is still pending.

## F1490 — Fresh push trigger receipt — startup failure — 2026-10-25

F1490's push at `fd9ee27a955bce7f4a5b13df33245a25a4e1101b` created Actions run
`40220179197` for `.github/workflows/phase4-resource-layout.yml`, event `push`.
Current local `HEAD` and `origin/Qt-Rewrite` are at that exact SHA.
It ended `startup_failure` at 18:13:01 UTC with zero jobs, checks, or artifacts;
log requests returned 404. The connected API exposes no exact cause or full-run
rerun tool. With zero jobs, failed-job rerun is inapplicable. The previous
branch run was October 18; re-enabling Actions had not created a run for the
already-pushed `1579fa7d` head.

Static review at the F1490 SHA passed Ruby YAML parsing and workflow-structure
checks, with no evident YAML defect; a GitHub-specific linter was unavailable.
CMake research confirms `-G Visual Studio 17 2022` is a supported override for
the Visual Studio 18 preset, not a generator conflict. The user reports the
issue solved and requests a retry; F1492's resulting run is recorded next.
Workflow remains `on.push` filtered to `Qt-Rewrite`, absent from default `main`,
with no dispatch. The cause of the startup failure remains unknown.

## F1492 — Retry resource-layout workflow — failed overall — 2026-10-25

Push head `cc776f1564e54cdc2492f9594d0580dd4c21e7d3` created Actions run
`40220727653`; the overall run failed. macOS passed the install/probe path,
staged audit (193 files, 187 SHA checks, five QMs, two QML keys), and CTest
3/3. Windows resource bake rejected `bundang.json` because its bytes differed
from inventory; installation, audit, and tests were skipped. Explorer confirmed
the local blob/hash matches inventory and that no repository `.gitattributes`
existed; Windows `autocrlf` was not observed, so line-ending conversion is a
likely but unconfirmed explanation. Linux failed compiling
`class_details_page.cpp:302` because long-to-QVariant conversion was ambiguous.

## F1493 — Preserve byte-exact resource inputs — accepted locally — 2026-10-25

Commit `64fcedcd` adds `resources/** -text` and Calendar QML `-text` rules to
`.gitattributes`. Tester confirmed the attributes cover all 232 inventoried
paths. Windows/Linux reruns remain required.

## F1494 — Make QVariant integer conversion explicit — accepted locally — 2026-10-25

Commit `b7878846` uses explicit `qlonglong` QVariant conversion at the failing
feature path. Executor's standard macOS `ClassMngrFeatures` target build
succeeded; Tester confirmed conversion semantics and the current object. CMake
research confirms `-G Visual Studio 17 2022` is supported as an override for
the preset, and Windows configure had succeeded. Windows/Linux reruns remain
required; these local checks do not close the native gates.

## F1496 — Retry resource-layout workflow — failed Windows test — 2026-10-25

Actions run `40221771224` at head `59a8e5da7281d4f53d3e6020b8cff15b72c14fd8`
passed macOS install/probe, audit (193 files, 187 SHA, five QMs, two QML keys),
and CTest 3/3. Linux passed the same audit and focused tests 8/8. Windows build,
install/probe, and audit passed; CTest was 7/8 because
`ClassMngrNextResourceLoaderTests` reported that a resource symlink escape was
not rejected.

Explorer found the test used `QFile::link` on a `.json` fixture path, which
creates a `.lnk` shortcut on Windows rather than a real symlink. F1497 changes
only the test fixture: it creates links with Windows `CreateSymbolicLinkW` and
the unprivileged flag or POSIX `symlink()`, explicitly checks that the path is
a real symlink and its canonical target is outside the resource root, retains
the `UnsafePath` assertion, and fails clearly rather than skipping if setup
fails. Executor and independent Tester each passed the macOS loader CTest 1/1.
Native Windows confirmation remains pending.

## F1498 — Fresh native rerun receipt — passed — 2026-10-25

Actions run `40223401741` completed successfully for a push to `Qt-Rewrite` at
`2470d756300254522b823a86fe3977382e868055`; the branch head matched the tested
SHA. macOS, Windows x64, and Linux x64 install/probe audits passed. The staged
resource audit found 193 files, 187 SHA-256 checks, five QM files, and two QML
catalog keys on each platform. The focused macOS layout tests passed 3/3, and
all eight loader/layout CTests passed on Windows and Linux. This closes the
F1483/F1486 native gates.

## F1500 — Retry after Actions reenabled — passed — 2026-10-25

The user confirmed Actions were reenabled and requested this retry.
Commit `277de9057168e67dc0b9ebc2e39719acc204a60f` on `Qt-Rewrite` created
[Actions run 40224983452](https://github.com/papercutter0324/ClassMngr-cpp/actions/runs/40224983452),
which completed successfully for that exact SHA. macOS, Windows x64, and Linux
x64 passed resource install/probe and the 193-file audit (187 SHA-256 checks,
five QM files, two QML keys). macOS CTest passed 3/3; Windows and Linux each
passed 8/8, including the Windows real-symlink containment fixture using
`CreateSymbolicLinkW` with no skip path. The Phase 4 exit gate is satisfied.
Phase 4 is Complete; Phase 5 Startup and Bootstrap Rewrite is next. Rendering
parity remains a Phase 7/10 gate, and legacy-pack removal remains a Phase 12
gate.
