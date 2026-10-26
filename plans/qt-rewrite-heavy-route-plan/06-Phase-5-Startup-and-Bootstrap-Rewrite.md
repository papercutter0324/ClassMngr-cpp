# Phase 5 — Startup and Bootstrap Rewrite

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
- Depends on: Phases 2, 3, and 4
- Blocks: Shared UI and feature migration
- Owner: Unassigned
- Last updated: 2026-10-26
- Current note: Phase 5 is active on the Heavy route. F1501–F1510 are complete; F1511 is next to complete shutdown coordination for settings, jobs, workspace, resource scopes, external automation, and development diagnostics. F1510 defers app-update service creation and dispatch until the first queued event-loop callback after `StartupReady`; resource updates remain absent from startup.

### Slice batches

Use the shared [current slice-batch tracker](Current-Slice-Batches.md) for live work when Phase 5 is active. It contains only the active
batch and the next batch once discovery starts; completed batches are
not archived there.

## Progress log

Use this section only when a decision, acceptance result, verification
finding, blocker, or exit-gate detail needs a durable record. Routine
slice chronology and closed batch history do not need to be archived;
keep current status in this plan and live batches in the shared tracker.

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

## Objective

Make startup explicit, short, observable, and free of splash-screen behavior.

## Target startup sequence

    QApplication
    → platform services
    → logger and diagnostics
    → ResourceCatalog and ResourceLoader
    → settings
    → localization
    → active theme
    → required fonts
    → workspace session
    → application runtime
    → AppShell
    → initial page
    → deferred application update check

## Work packages

### 5.1 ApplicationBootstrap

Create ApplicationBootstrap as the only composition root.

It is responsible for:

- Creating process-wide Qt objects.
- Creating platform adapters.
- Creating logging and diagnostics.
- Creating the resource system.
- Loading settings.
- Loading language and theme.
- Opening the workspace session.
- Creating application use cases.
- Creating the application shell.
- Coordinating shutdown.

It must not own feature-specific page state.

### 5.2 StartupCoordinator

Create explicit startup stages:

- Process created.
- Core services created.
- Settings loaded.
- Language loaded.
- Theme loaded.
- Required fonts loaded.
- Workspace opened.
- Shell created.
- Initial page ready.
- Startup-ready.

Every stage must emit diagnostics without requiring a visible splash screen.

### 5.3 Splash removal

Remove:

- SplashScreen class.
- Splash image.
- Splash progress bar.
- Fade animation.
- Splash leases.
- Minimum splash duration.
- Startup progress callback.
- Splash-related resource paths.
- Any hidden startup path that still creates the splash.

The normal main window appears as soon as the shell and initial page prerequisites are ready.

Do not replace the splash with another full-screen startup surface. If a page needs data, use its ordinary loading or empty state inside the main window.

### 5.4 Startup data policy

Only open the data and resources needed for the initial page.

Do not construct:

- All top-level pages.
- All nested class sections.
- All document bodies.
- Any PDF loaded into QtPdf or any rendered document pages.
- All campus maps.
- All report assets.
- All optional fonts.
- All large table models.

Apply the [Qt Rewrite Memory Hotspot Remediation
Plan](memory-hotspot-remediation-plan.md) to the startup boundary: My
Workspace must create only the visible child page, class and schedule
features must not materialize all records for registration, and feature
resources must be acquired on activation rather than during bootstrap.

### 5.5 Deferred application updates

Retain normal application update support, but start checking only after the main window is interactive.

Resource update checking is removed completely.

The update system must not be a dependency of the initial page.

### 5.6 Shutdown

Create an explicit shutdown coordinator that:

- Flushes settings.
- Completes or cancels application jobs.
- Closes the workspace transaction.
- Releases resource scopes.
- Closes external automation objects.
- Produces shutdown diagnostics in development builds.

### 5.7 Memory-remediation lifecycle handoff

Bootstrap must pass explicit page and resource scopes to PageHost and the
feature application services. It must not become a permanent owner of class
details, schedule/import workbooks, report data, PDF documents, or hidden child
pages. The lifecycle and release obligations are defined in the [Qt Rewrite
Memory Hotspot Remediation Plan](memory-hotspot-remediation-plan.md).

### 5.8 Startup workflow and dialog-automation boundary

Separate production startup from the startup/performance workflow driver.
`ApplicationBootstrap` and `StartupCoordinator` must not discover, inspect, or
activate prompts by scanning top-level widgets. They may publish typed stage,
operation, warning, and failure results through the application boundary.

The legacy executable may temporarily use the Phase 1 compatibility driver for
visual and behavioral parity captures. That driver belongs to the Qt
presentation/test boundary, must not return `QMessageBox*` or other widget
types, and must not become part of `ApplicationRuntime` or domain contracts.
When a v2 workflow is migrated, the performance harness should drive semantic
prompt requests through a fake or test adapter and assert the resulting
request, choice, trace, and capture rather than reintroducing widget searches.

## Deliverables

- ApplicationBootstrap.
- StartupCoordinator.
- ApplicationRuntime composition.
- ShutdownCoordinator.
- Splash-free startup.
- Startup diagnostics.
- Startup-negative test proving the document catalog does not load QtPdf
  content.
- Startup integration tests.
- Startup/performance workflow tests with a separate dialog-automation seam.
- Deferred application-update initialization.

## Exit gate

Startup has no progress callback, splash lease, minimum splash duration, or resource-pack initialization.

The main window appears with the correct theme, language, and initial page. Every startup allocation is attributable to a named stage.

Startup-ready includes document-catalog metadata only: no PDF body is loaded,
no QtPdf page is rendered, and no active viewer document exists until the user
requests one.

No v2 bootstrap or startup coordinator depends on widget ownership or
visibility to control a prompt. Legacy prompt automation, if still present,
is explicitly isolated and has a documented removal point.

## Heavy-route requirements

- For every Phase 5 slice, use the heavy route: connect the slice through the
  real bootstrap and lifecycle boundaries, verify startup and shutdown
  behavior, and remove temporary legacy startup paths after acceptance.
- Do not hide legacy startup work behind a new bootstrap class.
- Do not construct pages simply to register them.
- Do not perform network activity before the application is interactive.
- Do not trade the splash for an equally expensive replacement.
- Do not call `QPdfDocument::load()` as part of catalog initialization or
  startup page construction.
- Do not declare startup complete until the initial page is actually usable.
