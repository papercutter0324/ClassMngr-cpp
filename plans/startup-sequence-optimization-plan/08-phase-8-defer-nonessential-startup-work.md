<!-- ClassMngr Startup Optimization Plan — Phase 8 -->

> **Codex scope:** Read `00-START-HERE.md` plus this file. Implement and validate **Phase 8 only**. Do not start later phases.

# Phase 8 — Defer Nonessential Startup Work — Complete (2026-08-27)
## Objective
Make the first usable screen available before performing tasks unrelated to that screen.

This phase must reduce actual startup work, not merely postpone it.

## Completed Implementation

- The updater service and controller are no longer constructed before the
  main window. After the window is visible and the splash has been released,
  they are created by an idle event-loop callback, then the automatic update
  check begins. This preserves the automatic-check setting and its manual
  Help-menu action without allowing an update dialog to interrupt startup.
- Orphaned updater-download cleanup moved out of `UpdateController`'s
  constructor. It now runs when an automatic or manual update check actually
  starts, so it is no longer startup maintenance work.
- The automatic upcoming-birthdays database queries and modal prompt were
  removed from `MainWindow::showEvent()`. The existing **Upcoming
  Birthdays...** action remains the first-use path for that information.
- No hidden page, feature service, document payload, or resource-pack work is
  queued by this phase. The existing feature-scoped resource-pack behavior is
  intentionally unchanged.

## Validation

- Windows x64 Debug builds completed for `ClassMngr`; the updater, upcoming
  birthdays, and startup-performance tests all passed with the offscreen
  platform.
- `ClassMngrStartupPerformanceTests` now runs a representative startup for
  five seconds and verifies that its 1-second and 5-second checkpoints retain
  the startup-complete widget count, instantiated/registered page counts, and
  schedule-render count. This prevents a hidden page or a repeated schedule
  render from being introduced immediately after startup.
- Representative Windows Debug startup capture using `Testing-copy.tps`:
  startup-complete at 2.854 s, 189.7 MiB working set, 126.8 MiB private
  usage, 213 widgets, one instantiated page of 11 registered pages, and one
  schedule render. The 1-second and 5-second checkpoints held the same
  structural counts and reached a stable 208.6 MiB working set / 145.6 MiB
  private usage. The first-frame memory increase therefore has no associated
  page construction, widget growth, or schedule work; no optional feature
  initialization is being shifted into the settled interval.

## Classification
### Required before first usable window
Likely:

- QApplication;

- preferences;

- locale;

- font;

- theme;

- minimal resource infrastructure;

- core services;

- requested database;

- navigation shell;

- initial page;

- visible initial data.

### Optional after startup-complete
Evaluate:

- automatic update check;

- birthday/notification checks;

- noncritical maintenance;

- nonessential metadata refresh.

### First-use only
Keep feature-specific initialization lazy:

- PDF;

- Calendar/QML;

- Sub Prep;

- Campus;

- analytics;

- speaking evaluations;

- report/template rendering;

- document payloads;

- other heavy feature resources.

## Important Rule
Do not schedule all hidden-feature initialization at `+1s` or `+5s`.

If the feature is not used, its startup cost should never occur.

## Acceptance Criteria
The main window becomes usable before optional work begins.

No second major allocation spike appears shortly after startup because deferred eager initialization was simply moved later.

---
