# Phase 5 Progress Log

Historical progress entries moved from [06-Phase-5-Startup-and-Bootstrap-Rewrite.md](06-Phase-5-Startup-and-Bootstrap-Rewrite.md).
Entries are preserved in their original document order.

### F1501 — complete — 2026-10-25

`ClassMngrNext` now enters its real Qt Widgets path through
`ApplicationBootstrap::run()`, which owns the side-effect-free `--version`
preflight, `QApplication`, app metadata, startup, event loop, and shutdown
lifetime. The shell owns a visible minimal initial page; accepted close reaches
the idempotent `ShutdownCoordinator`. The typed startup result reports only
`InitialExperienceAvailable`, not the full Phase 5 `StartupReady`. No legacy
`MainWindow`, `ResourcePackManager`, splash/progress path, legacy application
services, or network work enters this composition.

Independent macOS Debug verification: `cmake --preset macos-clang-debug` and
the affected target build passed; focused CTest passed 3/3
(`ClassMngrNextBootstrapTests`, `ClassMngrNextLaunch`, and
`ClassMngrNextLaunchFilesystemSideEffectsTests`). The bootstrap test binary
reported 5/5 QtTest cases. Source ownership validated 1,640 handwritten files.
The full suite and an external subprocess close of the ordinary GUI path were
not run; shell readiness and close were tested through the production
bootstrap API under the offscreen platform.

### F1502 — complete — 2026-10-25

`StartupCoordinator` now models all ten planned Phase 5 stages, typed
completed/warning/failed outcomes, diagnostic codes, and monotonic transitions.
Duplicate, backward, invalid, failed, and early `StartupReady` outcomes are
rejected; readiness requires a non-failed outcome for every prerequisite.
`ApplicationBootstrap` emits only the real `ProcessCreated`, `ShellCreated`,
and `InitialPageReady` events and remains `InitialExperienceAvailable` until
later slices compose the missing owners. Its callback carries Qt-free typed
events; no widget scan or false ready stage is introduced.

Independent macOS Debug verification passed: `cmake --preset
macos-clang-debug`; the `ClassMngrNext`, bootstrap test, and launch side-effect
targets built; the three focused CTests passed 3/3. The bootstrap tests cover
the full state sequence, invalid transitions, warning and terminal failure,
the factual three-event production sequence, and retained close/shutdown
assertions. `git diff --check` passed for the changed test file. No full suite
ran.

### F1503 — complete — 2026-10-25

Production bootstrap now owns a narrow Qt startup diagnostics sink and routes
typed stage/outcome/diagnostic records through Qt logging. The sink is created
before `CoreServicesCreated`; startup records the factual order
`ProcessCreated` → `CoreServicesCreated` → `ShellCreated` → `InitialPageReady`.
If sink construction fails, startup returns a typed failure attributed to
`CoreServicesCreated`, logs through the early fallback, and creates no shell.
This uses Qt's configured logging destination and creates no persistent log
file, global message handler, or legacy `StartupProfiler` dependency.

Independent macOS Debug verification passed: the bootstrap test target built,
the `ClassMngrNext` target was current, and bootstrap/launch/filesystem CTests
passed 3/3. Tests directly checked the four ordered observer events and Qt log
records, plus the typed core-service failure, absence of a shell, and retained
startup/shutdown behavior. `git diff --check` passed. No full suite ran.
F1504 is next.

### F1504 — complete — 2026-10-25

The bootstrap now uses `ResourceRootResolver`'s executable-directory-relative
package lookup, loads only the resource catalog metadata, constructs and
retains `ResourceLoader`, and retains a named `Startup` scope. It records
`CoreServicesCreated` only after the complete resource owner exists. No payload
API is called. Resolver, catalog, or scope construction failure produces a
typed `CoreServicesCreated` failure before shell construction. Production has
no working-directory, source-tree, legacy RCC, or resource-pack fallback.

Independent macOS Debug verification passed: the preset configured, affected
bootstrap/resource/launch targets built, and six focused CTests passed 6/6
(bootstrap, resource loader, root resolver, document catalog, launch, and
filesystem side effects). Success used the generated catalog with an empty
payload root; failure cases covered resolver error, missing root, and invalid
catalog. Source review confirms the bootstrap retains its Startup scope and
does not call a loader payload method. The full suite and a native install
matrix were not rerun; Phase 4 retains the package-root matrix evidence.

### F1505 — complete — 2026-10-25

The bootstrap now reads the existing typed language and theme preferences
before shell construction, preserving the PaperCloud/ClassMngr settings
identity, settings-root override, canonical keys, and legacy language migration.
The existing settings implementation is shared with Next through a Qt Core-only
object target; the executable still links only Qt Core, Widgets, Next
Application, and Next Resources. Startup resolves and retains at most one
packaged translation, applies the selected catalog stylesheet, and registers
exactly Inter and Pretendard under its Startup resource scope. SystemDefault
uses the first supported system UI locale; no match is a warning and retains
source-English strings. System theme selection follows the current Qt color
scheme. The four real stages precede the shell; failures are typed and roll
back installed presentation resources. WorkspaceOpened and StartupReady remain
absent.

Independent macOS Debug verification passed: the affected bootstrap and legacy
runtime targets built; bootstrap QtTest passed 22/22, focused bootstrap and
preference CTests passed 3/3, and launch/filesystem side-effect CTests passed
2/2. Tests verify ordered stages, Korean and regional English selection,
unsupported-locale fallback, selected theme/translation, exactly the required
fonts, and translation/theme/font failure cleanup before shell creation. The
Next link commands have no Platform target or Qt Network reference. No full
suite or other-platform build ran. Settings-read failure has no injectable
test seam and was not exercised.
### F1506 — complete — 2026-10-25

Startup now applies the legacy workspace-selection order: a supported command
line path, then the first recent path when enabled, then the last-file path
only when the recent list is empty. A stale first MRU path is pruned and ends
that attempt. The dedicated Next workspace composition owns one
`WorkspaceStore`, reuses the Next workspace state/use-case contracts, and does
not construct `ApplicationServices` or link Network, the broad legacy runtime,
or Next Platform. Successful open retains the returned session. No candidate
and typed open errors each produce a non-fatal `WorkspaceOpened` warning so
the empty shell can appear; open errors do not fall through to another path.
Shutdown closes through the same store. `StartupReady` remains deferred until
runtime, PageHost, and startup catalog composition.

Independent macOS Debug verification passed: bootstrap QtTest passed 8/8 for
the six focused new cases, focused bootstrap/dependency-boundary CTest passed
2/2, and `ClassMngrNextBootstrapTests`, `ClassMngrNext`, and `ClassMngr` built.
The test verifies candidate order, no-workspace/open-error results, stale-MRU
behavior, missing-path no-create, session retention, stage order, and shutdown
release. The generated Next link has no Qt Network, Next Platform, Runtime,
Core, AppServices, or ApplicationServices dependency. Source ownership
validated 1,647 files. The full suite and other platforms were not run.
### F1507 — complete — 2026-10-25

The bootstrap now composes a Qt-free `ApplicationRuntime` from the abstract
`WorkspaceGateway`. The runtime owns `WorkspaceState`, `SelectionState`,
`WorkspaceUseCase`, and `WorkspaceCoordinator` in reference-safe member order;
the concrete store gateway outlives the runtime. Bootstrap routes startup
opening, state snapshots, workspace operations, and close through that one
runtime. A runtime exists even when there is no workspace or startup opening
fails, with its workspace state closed. There is no feature-page ownership,
platform type, extra startup stage, or early `StartupReady`.

Independent macOS Debug verification passed: runtime/bootstrap/dependency CTests
3/3; the affected runtime and bootstrap tests and `ClassMngrNext` rebuilt. The
runtime test links without Qt libraries. No full suite or other platform ran.

### F1508 — complete — 2026-10-26

`ApplicationShell` now hosts a Next-only `PageHost`; the initial page is
registered as a factory and created only when activated. The host suspends the
outgoing page, reuses it on return, and destroys it on explicit release while
retaining the factory for recreation. Factories receive a page context with
the Next loader and a named Page scope. Scope labels do not expire resources:
pages retain page-scoped RAII handles while suspended, release feature-scoped
handles in their suspension hook, and release all remaining handles when
destroyed. Typed factory/activation failures clean up the attempted page while
preserving the previous active page. Bootstrap activates the initial page
before showing the shell and recording `InitialPageReady`; it does not emit
`StartupReady`.

Independent incremental macOS Debug verification passed PageHost QtTest 4/4,
the bootstrap QtTest 30/30, and focused CTest 2/2. Affected production and test
targets built; CMake source ownership validated 1,653 handwritten files.
The full suite and other platforms were not run.

### F1509 — complete — 2026-10-26

Bootstrap now projects the localized document-catalog metadata through the
Next `ResourceDocumentCatalogPort` and retains the immutable projection in
`ApplicationRuntime`. The adapter belongs to a narrow target using Qt Core,
Next Application, and Next Resources; `ClassMngrNext` still avoids Platform,
Network, QtPdf, and broad legacy targets. Metadata failure records a typed
`InitialPageReady` failure before shell creation. The coordinator permits that
ordered failure while still requiring `ShellCreated` for successful or warning
`InitialPageReady` results. On success, bootstrap activates and shows the
initial page, records `InitialPageReady`, then records `StartupReady`. It does
not create a body session or viewer.

Independent incremental macOS Debug verification passed the bootstrap
QtTest 32/32 and six focused CTests 6/6 (resource catalog, loader, bootstrap,
runtime, PageHost, and dependency boundary). The integrated fixture omits all
document bodies; metadata failure creates no shell or ready stage. Link checks
confirmed no Platform, Network, QtPdf, or broad legacy dependency. The full
suite and other platforms were not run.

### F1510 — complete — 2026-10-26

Application update support now uses the existing `StartupUpdateCheckUseCase`
and `UpdateServiceStartupCheckPort` through a dedicated `ClassMngrUpdater` and
`ClassMngrNextStartupUpdates` boundary. `ApplicationBootstrap::start()` does
not construct the update service or network manager. After successful
`StartupReady`, a one-shot queued event-loop callback checks that the shell is
visible and the initial page active, then creates the dispatcher and evaluates
the existing build, preference, URL, and one-shot gates. Native foreground
focus is not required. The updater remains owned through shutdown. No resource
update service is called from startup; manual update behavior remains covered.

Independent incremental macOS Debug verification passed: the final focused
bootstrap QtTest case passed 3/3, focused update/bootstrap/manual/preference/
dependency CTests passed 8/8, and the two socket-backed updater/controller
suites passed 2/2 using local ephemeral loopback under escalated test
permissions. `ClassMngr`, `ClassMngrNext`, and the affected test targets built;
source ownership validated 1,655 files. No full suite or other platform ran.

### F1511 — complete — 2026-10-26

Shutdown now flushes settings, explicitly cancels the deferred startup update
request, then closes the workspace. Settings-flush and workspace-close failures
remain retryable while the bootstrap is alive; a close failure rejects the
shell close and retains its runtime/session and gateway. Successful close
queues finalization after `closeEvent` returns, destroys the shell/page and
startup resource owners while `QApplication` is alive, restores application
policy, and requests quit once. The updater aborts its active `QNetworkReply`
and suppresses the cancellation outcome; legacy/manual update behavior remains
unchanged. No other application jobs or external automation objects are
currently instantiated in Next, so no registry or placeholder owner was added.

Independent incremental macOS Debug verification built `ClassMngr`,
`ClassMngrNext`, `ClassMngrNextBootstrapTests`, and `ClassMngrUpdaterTests`.
The focused bootstrap CTest passed 1/1; its QtTest run passed 7/7 cases before
the final direct page-lifetime assertion, and the final CTest passed again
after that assertion. The updater CTest passed 1/1 using local loopback after
the sandbox blocked its initial bind. The final build had no ignored-result
warning, and `git diff --check` passed. No full suite or other platform ran.

`SettingsManager::sync()` returns no QSettings status, so shutdown verifies
that flush is called and ordered but does not claim to detect an underlying
settings-write error. If a caller forcibly destroys `ApplicationBootstrap`
after a failed workspace close, its owned runtime cannot outlive the bootstrap
for another retry; the normal shell-close path prevents that by keeping the
bootstrap alive and vetoing close.
### Phase 5 exit gate — complete — 2026-10-26

F1501–F1513 are complete at implementation head
`6bbff54abc524723e23911d65130b1b198dde2b3`, pushed on `Qt-Rewrite`. The
explicit behavior gate passed: `ClassMngrNext` has no splash/progress path,
resource-pack initialization, PDF body load, rendered page, or active viewer at
startup; the initial page is usable with the selected language and theme, and
startup allocation is attributed to named stages. Prompt automation remains
isolated at the Qt presentation/test boundary. Legacy startup has no splash or
splash-specific eager pack initialization and retains only permitted lazy
feature-owned packs. The tracked splash PNG remains a Phase 4 audit input.

Independent macOS Debug verification passed the eight-target incremental
build, focused CTest 5/5, startup dependency boundary 1/1, resource-pack
QtTest 12/12, and eight direct startup/workflow selectors without skips.
Windows x64 and ARM64 Release builds passed; the packaged x64 smoke asserted one
`startup-complete`, ordered `startup-page-loaded < window-shown < startup-complete`,
and absence of splash, resource-system, and progress fields.
The Phase 4 resource-install matrix passed on macOS, Windows x64, and Linux x64.
See [Refactoring baseline run 40301659920](https://github.com/papercutter0324/ClassMngr-cpp/actions/runs/40301659920),
[Windows Release run 40301659949](https://github.com/papercutter0324/ClassMngr-cpp/actions/runs/40301659949),
and [Phase 4 resource-install run 40301659977](https://github.com/papercutter0324/ClassMngr-cpp/actions/runs/40301659977).

The Linux and Windows full Debug CTest suites did not pass: Linux recorded 11
and Windows 14 failures/timeouts. Linux `ClassMngrStartupPerformanceTests`
passed in 40.67 seconds; the Windows target timed out at 120 seconds after
emitting startup metrics. The Windows suite had zero `0xc0000135` loader
failures after the CTest Qt DLL PATH fix. The Windows
`ClassMngrNextPlatformApplicationServicesDocumentCatalogPortTests` failure is
a stale source assertion at
`tests/next_platform_application_services_document_catalog_port_tests.cpp:235`:
it expects `m_documentContentResourcePort`, while Phase 4 commit `34926f4b`
renamed the retained legacy navigation adapter to
`m_legacyDocumentContentResourcePort`. Independent review found this test checks
the legacy navigation source path, not Next startup. These full-suite results
remain limitations and are not reported as passes; the documented Phase 5 gate
is behavior-based and does not require a full-suite pass.

Batch 1 is complete; Batch 2 discovery found no other slices. Phase 6 is the
next planned phase. Visual parity remains a Phase 7/10 gate and legacy pack
removal remains a Phase 12 gate.
