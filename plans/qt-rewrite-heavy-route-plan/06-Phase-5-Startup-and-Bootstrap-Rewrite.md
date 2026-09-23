# Phase 5 — Startup and Bootstrap Rewrite

## Status

- Status: Not started
- Default route: Heavy
- Depends on: Phases 2, 3, and 4
- Blocks: Shared UI and feature migration
- Owner: Unassigned
- Last updated: 2026-09-16
- Current note: Replace monolithic startup and eliminate the splash screen from every path.

## Progress log

Record this phase's progress here. Add a dated entry when work starts, a
milestone is reached, a blocker appears, or the exit gate passes. Append entries
in date order and include what changed, what remains, evidence or a verification
command, and any new risk or blocker.

Entry format:

### YYYY-MM-DD — <milestone or update>

- Changed:
- Remaining:
- Evidence:
- Risks or blockers:

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
