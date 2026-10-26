# Current Slice Batches

Phase 5 is active on the mandatory Heavy route. Phase 4 completed at F1500;
its native matrix passed before this batch began.

## Batch 1 — active

1. **F1501 — Complete:** Make `ClassMngrNext` a real Widgets
   startup composition with an owned minimal shell/page, typed diagnostics for
   the implemented stages, and coordinated shutdown. Keep `--version`
   side-effect-free and keep legacy startup out of the new bootstrap.
   Independent macOS Debug CTest passed 3/3; see the Phase 5 progress log.
2. **F1502 — Complete:** Define the full Phase 5 stage catalog,
   typed completion/warning/failure events, and monotonic legal transitions.
   Wire actual process/shell/page stages into the current bootstrap, but do not
   synthesize missing resource, settings, language, theme, font, workspace, or
   runtime events and do not emit `StartupReady` before those prerequisites are
   composed. Independent macOS Debug focused CTest passed 3/3.
3. **F1503 — Complete:** Compose the bootstrap-owned Qt startup diagnostics
   sink before resource initialization; log typed stage outcomes and attribute
   sink-construction failure to `CoreServicesCreated`. Independent focused
   CTest passed 3/3.
4. **F1504 — Complete:** Resolve the installed resource package
   from the executable directory, parse metadata only, and retain the Next
   `ResourceLoader` with an explicit `Startup` scope. Do not request payloads or
   fall back to the working directory, source tree, legacy RCCs, or resource
   packs. Resolution/catalog failures must fail `CoreServicesCreated` before
   the shell is built; verify success against the generated catalog with an
   empty payload root and verify the typed failure path. Six focused macOS
   Debug CTests passed 6/6.
5. **F1505 — Complete:** Preserve the existing settings identity,
   path override, preference keys, and migration semantics through a narrow
   Qt Core settings owner shared with the legacy runtime. Before shell creation,
   read startup preferences, install only the selected Next catalog `.qm`,
   apply only the selected packaged theme, and register/retain only Inter and
   Pretendard under the Startup scope. Emit SettingsLoaded → LanguageLoaded →
   ThemeLoaded → RequiredFontsLoaded only after each real operation. Do not use
   legacy LanguageService/ThemeService/FontManager RCC paths, optional fonts,
   or the broad Platform/Network target; do not emit WorkspaceOpened or
   StartupReady in this slice. Failure must be attributed to its stage and
   must not leave a partial translator/font/theme registration or shell.
   Independent macOS Debug verification passed bootstrap QtTest 22/22, focused
   bootstrap/preference CTest 3/3, and launch/filesystem CTest 2/2. Locale
   fallback and regional selection were exercised through a deterministic
   locale-list seam. No full suite ran.
6. **F1506 — Complete:** Preserve legacy workspace choice order:
   first supported command-line path; otherwise the first recent workspace
   when recent auto-open is enabled; use last-file only when the recent list is
   empty; no candidate is a valid no-workspace start. Preserve stale-first-MRU
   behavior by pruning the missing entry and stopping that attempt. Use one
   Next-owned `WorkspaceStore` through a dedicated store-backed adapter and the
   existing Next workspace contracts; do not construct `ApplicationServices`,
   link broad `Platform`/Network, or create a second store. Typed no-workspace
   and open-error warnings permit the empty shell to appear; retain a successful
   `WorkspaceSession` in Next state. Do not record `StartupReady` until the
   runtime, PageHost, and startup catalog are composed. Independent checks must
   cover policy order, session retention, no-workspace/open failure, and the
   no-create-on-missing-path boundary. Independent macOS Debug verification
   passed six focused QtTest cases (8/8 including setup/cleanup), the bootstrap
   and dependency-boundary CTests (2/2), and builds of ClassMngrNext and
   ClassMngr. Link inspection confirmed no Network or broad legacy target.
7. **F1507 — Complete:** Add a Qt-free `ApplicationRuntime` that
   owns long-lived application state and use-case composition: the F1506
   workspace state/session, selection state, workspace use case, and workspace
   coordinator. It receives the abstract `WorkspaceGateway` port; the bootstrap
   remains the sole root that builds the concrete store adapter and runtime.
   Make the runtime the single session authority rather than keeping a second
   copy in `ApplicationBootstrap`. Do not add feature-page state, PageHost,
   document-catalog policy, update work, broad platform dependencies, or an
   early `StartupReady`. Verify ownership order, startup-session transfer,
   ordinary workspace operations through the retained coordinator, and
   bootstrap shutdown integration. Independent macOS Debug verification
   passed the runtime/bootstrap/dependency CTests 3/3; the runtime tests link
   without Qt libraries. `ClassMngrNext` and the affected runtime test targets
   rebuilt. No full suite or other platform ran.
8. **F1508 — Complete:** Add a Next-only, lazy factory PageHost
   and connect `ApplicationShell` through it; registration must not construct
   pages. Activation creates once, switch-away suspends, returning reuses, and
   explicit release destroys the page and releases its page-scoped handles.
   Factories receive a page resource context; page code owns feature handles
   and clears them on suspension because `ResourceScope` labels requests but
   does not expire handles. Keep `PageManager` out of Next. Register the current
   initial page as a factory and preserve its visible shell behavior. A failed
   factory/activation must not retain a partial page or leak scoped handles.
   Verify lazy construction, lifecycle call order, reuse/release, feature vs
   page handle lifetimes, failure cleanup, and bootstrap initial-page display.
   Independent incremental macOS Debug verification passed PageHost QtTest
   4/4, bootstrap QtTest 30/30, and focused CTest 2/2. Source ownership
   validated 1,653 files. No full suite or other platform ran.
9. **F1509 — Complete:** Load the localized Next
   `DocumentCatalogProjection` from the `document.catalog-metadata` resource
   and retain it in `ApplicationRuntime` before shell/page readiness. Reuse
   `ResourceDocumentCatalogPort` through a narrow target that does not pull
   `ClassMngrNext::Platform` or Network. Catalog failure is a typed
   `InitialPageReady` failure and creates no shell; on success emit
   `InitialPageReady` and then `StartupReady`. Use the existing metadata-only
   test, plus an integrated bootstrap test whose package fixture omits every
   document body and checks that no QtPdf viewer is present. Do not construct
   a viewer, `QPdfDocument`, body session, or feature page during startup.
   Independent incremental macOS Debug verification passed bootstrap QtTest
   32/32 and six focused CTests 6/6. The generated boundary report excludes
   Platform, Network, QtPdf, and broad legacy dependencies. No full suite or
   other platform ran.
10. **F1510 — Complete:** Reuse `StartupUpdateCheckUseCase` and
    the existing startup-check port after successful `StartupReady`, scheduled
    on the first queued event-loop callback so the shell/page is already
    interactive. Do not construct `UpdateService`/network manager or dispatch
    a request during `ApplicationBootstrap::start()`. Preserve build-config,
    settings preference, API URL, and one-shot eligibility checks. Keep the
    Next dependency narrow with dedicated updater/startup-update targets, not
    the Platform umbrella or broad Core/Runtime. No resource-update service
    is called from startup; preserve manual update behavior. Verify zero
    service/port calls before the callback, one eligible dispatch afterward,
    disabled/unconfigured gates, and no resource-update call. Independent
    incremental macOS Debug verification passed the final focused bootstrap
    test, CTest 8/8, and the updater/controller loopback suites 2/2 with
    elevated local-socket permissions. No full suite or other platform ran.
11. **F1511 — Discovered:** Complete shutdown coordination for settings, jobs,
    workspace, resource scopes, external automation, and development diagnostics.
12. **F1512 — Discovered:** Separate legacy startup/performance workflow
    automation from production startup and use a semantic prompt seam. Begin
    next-batch discovery when work on this second-to-last candidate starts.
13. **F1513 — Discovered:** Remove the legacy splash class/image/progress,
    leases, minimum duration, resource paths, and startup pack initialization
    after the workflow compatibility path is isolated.

No other candidates beyond the 20-slice batch limit were identified in the
initial Phase 5 discovery.

## Batch 2 — pushed

F1485 closed the Batch 2 commit set. The 25 commits span F1469 `d282e7f1`
through F1485 `1475a46d`. The successful push from `Qt-Rewrite` to `origin`
advanced `Qt-Rewrite` from
`eb473fab89410e6dc5f5527d872b56f150f7f6fd` to
`1475a46dae77d8970852681dae7a2032ff907edf`; at the time of the receipt, local
`HEAD` and `origin/Qt-Rewrite` matched at that SHA.

The push was intended to trigger F1483's native Windows/Linux Actions legs.
First and subsequent authenticated GitHub Actions API queries found no workflow
run or checks for head `1475a46d`. No run or result is claimed; the native
package/layout verification remains pending.

## Batch 3 — pushed

F1486 is committed as `094cda87` and expands the workflow using existing tests
only. The successful push from `Qt-Rewrite` to `origin` advanced the ref in two
commits, from Batch 2 tip
`1475a46dae77d8970852681dae7a2032ff907edf` to
`a2e506a74c316bf902d6a4b5cd956970286471b3` (F1486 workflow commit
`094cda87`, followed by its documentation commit `a2e506a7`). Local `HEAD` and
`origin/Qt-Rewrite` matched at the new tip at that time. Current local `HEAD`
and `origin/Qt-Rewrite` both resolve to
`1605bac36d4192bf027440a2b83dce8a5e2d99d1`, parent
`a2e506a74c316bf902d6a4b5cd956970286471b3`, tree
`12f431a05225f30aecb076fd88886403a2e0e8fc`. This is the same tree and parent
as the former local receipt commit `9fe1f26fe3ace50996d58b87cf045f9afab31afb`,
now superseded by the published commit. The intended Windows x64 Debug and
Linux GCC Debug jobs cover loader native resource-handle assertions, document
content/export ports, QtPdf session release, and no-pack startup:

- `ClassMngrNextResourceLoaderTests`
- `ClassMngrNextPlatformDocumentContentResourcePortTests`
- `ClassMngrNextPlatformDocumentExportResourcePortTests`
- `ClassMngrNextPlatformQtPdfSessionReleaseTests`
- `ClassMngrNextLaunchFilesystemSideEffectsTests`
- `ClassMngrNextLaunch`

Supplementary standard macOS Debug verification ran six targeted F1486 CTests:
5/6 passed. Only `ClassMngrNextResourceLoaderTests` failed: the generated
catalog has 194 records, classified by the generator as 187 runtime-packaged
plus 7 generated. C++ `directCount` counts `PackageFile`: current totals are
194 records (192 `PackageFile`, 2 `QtResource`). The five generated QM files
are among the 192 package files; the other two generated records are Qt QML.
Correct expectations are 192 `PackageFile` plus 2 `QtResource`, 194 total.
The assertion at `cmake/tests/next_resource_loader.cpp:333` expects 198 total
and 196 `PackageFile`. Explorer confirmed these expectations are stale; F1482's
four `excluded_v2` records account for the 198-to-194 change. Executor
corrected counts while preserving coverage; its standard macOS Debug build
and focused CTest passed 1/1. Independent Tester reran all six targeted F1486
CTests on the standard macOS Debug preset; 6/6 passed. F1488 is accepted, and
F1487 is committed as `f86c79fc`. Required Windows/Linux Actions results
remain open; the macOS pass does not replace them.

Linux QtPdf is mandatory and fails clearly if unavailable. Independent workflow
review passed. Authenticated Actions API queries found no workflow runs/checks
for these heads: Batch 2 `1475a46d`, Batch 3 `a2e506a7`, and current remote
`1605bac3`. Combined status for `1605bac3` is pending. No run or result is
claimed, and F1483/F1486 native outcomes remain pending. The push filter targets
`Qt-Rewrite` and push workflows can run on non-default branches. The repository
default is `main`, where this workflow file is absent; `workflow_dispatch`
requires it on the default branch. No dispatch tool is exposed. The missing run
cause is unknown; do not assume credential suppression or Actions settings.
User direction is needed on whether to extend workflow scope to `main`. F1483's
root-resolver and prefix-guard tests remain in its separate package/layout
matrix. Phase 4 remains In progress pending both native gates and that
direction.
Phase 7/10 rendering parity remains deferred; legacy RCC implementation and
pack inputs remain the compatibility oracle through Phase 12.

## Batch 4 — pushed

Batch 4 advanced `origin/Qt-Rewrite` from
`1605bac36d4192bf027440a2b83dce8a5e2d99d1` to
`ea46415a93e8285e4cb1f81e9aec5b79f7533286` in two commits: F1487
`f86c79fc81a2b2fd34c6993509b4cd314aacaec1`, then F1488
`ea46415a93e8285e4cb1f81e9aec5b79f7533286`. Local `HEAD` and
`origin/Qt-Rewrite` matched at that tip at the time of the push.
Actions API returned zero runs and zero check-runs for this exact head; combined
status is pending. No run is claimed.

F1488 is accepted. The corrected resource-loader counts are 194 total: 192
`PackageFile` and 2 `QtResource`; all six focused F1486 CTests passed on the
standard macOS Debug preset. F1483/F1486 native Windows/Linux results remain
open. The push filter targets `Qt-Rewrite` and supports non-default branches;
the workflow file is absent from default `main`, and no dispatch tool is
available. The cause of missing runs is unknown. User direction on extending
workflow scope to `main` remains pending; Phase 4 remains In progress.

## F1490 — Fresh push trigger receipt — startup failure

F1490's push at `fd9ee27a955bce7f4a5b13df33245a25a4e1101b` created Actions run
`40220179197` for `.github/workflows/phase4-resource-layout.yml` (event `push`).
Current local `HEAD` and `origin/Qt-Rewrite` are at that exact SHA.
It ended `startup_failure` at 18:13:01 UTC with zero jobs, checks, or artifacts;
log requests returned 404. The connected API exposes no exact cause or full-run
rerun; with zero jobs, failed-job rerun is inapplicable. Before this push, the
last branch run was October 18, and re-enabling Actions did not create a run for
already-pushed head `1579fa7d`. Static Ruby YAML/structure review at the F1490
SHA found no evident defect; a GitHub-specific linter was unavailable. CMake
research confirms `-G Visual Studio 17 2022` is supported for the Visual Studio
18 preset, so it is not a generator conflict. The issue is reported solved and
the user requests a retry. Workflow remains `on.push` filtered to `Qt-Rewrite`,
absent from default `main`, with no dispatch. The cause of startup failure is
unknown.

## F1492 — Retry resource-layout workflow — failed overall

Push head `cc776f1564e54cdc2492f9594d0580dd4c21e7d3` created Actions run
`40220727653`, which failed overall. macOS passed install/probe, audit
(193 files, 187 SHA, five QMs, two QML keys), and CTest 3/3. Windows resource
bake failed on `bundang.json` bytes differing from inventory; later install,
audit, and tests were skipped. Explorer confirmed the local blob/hash matches
inventory and there was no repository `.gitattributes`; Windows `autocrlf` was
not observed, so EOL conversion is likely but unconfirmed. Linux failed
compiling `class_details_page.cpp:302` on ambiguous long-to-QVariant conversion.

## Retry batch — F1496 results; F1498 native rerun passed

F1493 (`64fcedcd`) adds `resources/** -text` and Calendar QML `-text` rules;
Tester confirmed all 232 inventory paths are covered. F1494 (`b7878846`) makes
the QVariant conversion explicitly `qlonglong`. Executor's standard macOS
`ClassMngrFeatures` target build succeeded, and Tester confirmed the semantics
and current object. CMake confirms the VS 17 generator override is supported;
Windows configure had succeeded. F1496 run `40221771224` at `59a8e5da` passed
macOS audit (193 files, 187 SHA, five QMs, two QML keys) and CTest 3/3; Linux
passed the same audit and focused tests 8/8. Windows build, install/probe, and
audit passed, but CTest was 7/8 because the symlink-escape fixture did not
create a real symlink. Explorer confirmed `QFile::link` creates a `.lnk`
shortcut for a `.json` path on Windows. F1497 changes only the fixture to use
Windows `CreateSymbolicLinkW` with the unprivileged flag or POSIX `symlink()`,
verifies a real link with an outside canonical target, keeps the `UnsafePath`
assertion, and fails clearly instead of skipping if setup fails. Executor and
independent Tester macOS loader CTests each passed 1/1.

Windows confirmation was completed by Actions run `40223401741` at head
`2470d756300254522b823a86fe3977382e868055`. macOS, Windows x64, and Linux x64
resource install/probe audits passed; macOS focused layout tests passed 3/3,
and Windows/Linux each passed all eight CTests. The F1483/F1486 native gates
are closed.

## F1500 — Retry after Actions reenabled — passed

The user confirmed Actions were reenabled and requested this retry.
Commit `277de9057168e67dc0b9ebc2e39719acc204a60f` on `Qt-Rewrite` created
[Actions run 40224983452](https://github.com/papercutter0324/ClassMngr-cpp/actions/runs/40224983452),
which completed successfully at that exact head. macOS, Windows x64, and Linux
x64 passed install/probe and the 193-file audit (187 SHA-256 checks, five QM
files, two QML keys); macOS CTest passed 3/3 and Windows/Linux each passed 8/8.
The Windows real-symlink containment fixture passed with `CreateSymbolicLinkW`
and no skip. Phase 4 is Complete. Phase 5 is next and remains Not started.
Rendering parity remains a Phase 7/10 gate; legacy-pack removal remains a
Phase 12 gate.
