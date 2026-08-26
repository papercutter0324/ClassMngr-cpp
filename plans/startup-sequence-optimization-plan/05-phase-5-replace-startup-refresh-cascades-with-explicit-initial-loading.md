<!-- ClassMngr Startup Optimization Plan — Phase 5 -->

> **Codex scope:** Read `00-START-HERE.md` plus this file. Implement and validate **Phase 5 only**. Do not start later phases.

# Phase 5 — Replace Startup Refresh Cascades With Explicit Initial Loading — Complete (2026-08-27)
## Objective
Make startup a single controlled state transition instead of a series of global refreshes and temporary page/tab changes.

## Completed Implementation

- `applyDatabaseLoadedState()` now owns the normal database startup route. It
  applies database state, populates the sidebar once, selects My Workspace →
  Schedule, and explicitly loads that Schedule page's initial content.
- Removed the duplicate `showStartupDatabasePage()` pass, the temporary My
  Details tab selection, and startup's `PageManager::refreshAll()` call.
- My Workspace and Schedule no longer perform full refreshes from their
  `showEvent()` handlers. Page activation therefore does not repeat the
  initial visible-page load.
- `FileController::closeActiveDatabase()` clears page state only when a
  database was actually open, preventing startup from rendering an unnecessary
  cleared Schedule before the selected database opens.
- `ClassMngrStartupPerformanceTests` now runs the representative fixture and
  asserts exactly two ScheduleWidget render passes: the empty shell render and
  the single data-backed visible schedule render.

## Validation

- Windows x64 Debug build completed successfully.
- `ClassMngrStartupVisualSettingsTests`, `ClassMngrScheduleWidgetTests`,
  `ClassMngrPageManagerTests`, `ClassMngrMyWorkspacePageTests`,
  `ClassMngrSubPrepPageTests`, `ClassMngrBasePageTests`, and
  `ClassMngrStartupPerformanceTests` passed.
- A representative Windows debug capture using `Testing-copy.tps` recorded:

| Startup complete | Peak working set | Working set | Private usage | Widgets | Pages | ScheduleWidgets | Schedule renders |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 2.924 s | 202.1 MiB | 200.9 MiB | 130.0 MiB | 213 | 1 / 11 | 1 live / 1 created | 2 |

The startup-complete ScheduleWidget diagnostic passed with one live and one
created instance.

## Current Problem
The current startup path can perform overlapping work resembling:
```text
construct

→ load database

→ apply database state

→ refresh all

→ open My Workspace

→ open one tab

→ later switch tab

→ show

→ showEvent refresh

→ post-show refresh all
```
## Tasks
### 5.1 Resolve the intended startup destination early
Before loading page content, resolve:

- startup database;

- initial top-level page;

- initial subpage/tab;

- required navigation selection.

A lightweight `StartupContext` or existing equivalent may be used if it simplifies the flow.

Example:
```cpp
struct StartupContext {

    QString databasePath;

    PageType initialPage;

    WorkspaceTab initialWorkspaceTab;

};
```
Do not introduce this abstraction if existing types can express the same state cleanly.

### 5.2 Simplify normal database startup
Target:
```text
resolve database

→ open database

→ establish application database-open state

→ load minimal navigation/sidebar data

→ create intended initial page

→ select intended tab

→ load visible page once

→ show window
```
Do not load one tab and immediately replace it with another before the user sees it.

### 5.3 Remove `PageManager::refreshAll()` from normal startup
Do not globally refresh every instantiated page after opening the database.

Only load:

- the visible page;

- navigation data required to display the shell.

Reserve `refreshAll()` for genuine global invalidation scenarios.

### 5.4 Remove duplicate sidebar rebuilds
Do not rebuild sidebar/navigation because of:

- font startup;

- theme startup;

- first show;

- page activation;

unless sidebar data changed.

### 5.5 Avoid duplicate visible-page refreshes
Audit startup interactions among:

- `applyDatabaseLoadedState()`;

- `showStartupDatabasePage()`;

- page `showEvent()`;

- `refresh()`;

- post-show timers.

Ensure only one path owns initial visible-page loading.

## Acceptance Criteria
Normal startup performs:

- one database open;

- one initial navigation-data load;

- one sidebar population;

- one initial page selection;

- one initial tab selection;

- one visible-page data load/render;

- no startup `refreshAll()`.

---
