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
- Last updated: 2026-10-25
- Current note: Phase 5 is active on the Heavy route. F1501 is complete: `ClassMngrNext` now uses `ApplicationBootstrap` as its production root, displays a minimal shell/page, reports only `InitialExperienceAvailable`, and coordinates close/shutdown. Independent macOS Debug verification passed 3/3 focused CTests. F1502 is next; complete startup stages and diagnostics remain open. The ordered batch is in `Current-Slice-Batches.md`.

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
bootstrap API under the offscreen platform. F1502 is next.

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
