# Phase 5 — Startup and Bootstrap Rewrite

## Status

- Status: Not started
- Default route: Heavy
- Depends on: Phases 2, 3, and 4
- Blocks: Shared UI and feature migration
- Owner: Unassigned
- Last updated: 2026-09-15
- Current note: Replace monolithic startup and eliminate the splash screen from every path.

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
- All campus maps.
- All report assets.
- All optional fonts.
- All large table models.

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

## Deliverables

- ApplicationBootstrap.
- StartupCoordinator.
- ApplicationRuntime composition.
- ShutdownCoordinator.
- Splash-free startup.
- Startup diagnostics.
- Startup integration tests.
- Deferred application-update initialization.

## Exit gate

Startup has no progress callback, splash lease, minimum splash duration, or resource-pack initialization.

The main window appears with the correct theme, language, and initial page. Every startup allocation is attributable to a named stage.

## Heavy-route requirements

- Do not hide legacy startup work behind a new bootstrap class.
- Do not construct pages simply to register them.
- Do not perform network activity before the application is interactive.
- Do not trade the splash for an equally expensive replacement.
- Do not declare startup complete until the initial page is actually usable.
