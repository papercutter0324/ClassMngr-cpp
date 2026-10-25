# Phase 4 Progress Log

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

Per-slice implementation and verification evidence is recorded in the
[Phase 4 plan](05-Phase-4-Resource-Loader-and-Packaging.md). Windows and Linux
resource staging remain for F1483; Batch 1 packaging was host-staged on macOS.

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
untouched; unreferenced PPTX and roster XLSX remain for F1482.

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
