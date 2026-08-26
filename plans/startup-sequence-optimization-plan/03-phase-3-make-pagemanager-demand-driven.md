<!-- ClassMngr Startup Optimization Plan — Phase 3 -->

> **Codex scope:** Read `00-START-HERE.md` plus this file. Implement and validate **Phase 3 only**. Do not start later phases.

# Phase 3 — Make PageManager Demand-Driven — Complete (2026-08-27)
## Objective
Register all application pages without constructing pages that are not required for the first visible screen.

This must be shared behavior on all operating systems.

## Completed Implementation

- `PageManager::initialize()` registers all 11 page factories, then constructs
  only the default `MyWorkspace` route. Its visible Schedule child remains the
  single required initial content widget.
- My Classes, standalone Schedule, Sub Prep, Testing Classes, Teacher Info,
  Native English Teachers, GS Team, Classes, Campus Dashboard, and PDF Viewer
  remain unconstructed until their route is selected.
- Lazy pages inherit the stored database, save-mode, and document-viewer state
  through `applyCurrentState()` immediately after factory construction. Theme
  and font continue to be inherited from the application.
- Main-window page-specific signal wiring now attaches when a page is created,
  so standalone Schedule, Testing Classes, Teacher Info, Classes, and Campus
  Dashboard retain their existing behavior after deferred construction.
- Added focused `ensure...Page()` accessors for pages that must receive content
  before becoming visible, preserving first-use teacher, staff-directory, and
  testing-class navigation.

## Validation

- Windows x64 Debug build completed successfully.
- `ClassMngrPageManagerTests`, `ClassMngrMyWorkspacePageTests`, and
  `ClassMngrStartupPerformanceTests` passed.
- `ClassMngrPageManagerTests` now verifies that startup registers all 11 pages
  while constructing only My Workspace, and that every deferred route can be
  constructed and activated on first use.
- Three native Windows representative captures using the supplied
  `Testing-copy.tps` fixture all recorded `1 / 11` instantiated/registered
  pages, one live `ScheduleWidget`, and four schedule renders at
  startup-complete:

| Run | Startup complete | Peak working set | Working set | Private usage | Widgets | Pages | ScheduleWidgets | Schedule renders |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | 4.385 s | 712.3 MiB | 710.0 MiB | 596.9 MiB | 241 | 1 / 11 | 1 | 4 |
| 2 | 3.464 s | 712.8 MiB | 710.5 MiB | 597.0 MiB | 241 | 1 / 11 | 1 | 4 |
| 3 | 3.467 s | 712.0 MiB | 709.7 MiB | 597.6 MiB | 241 | 1 / 11 | 1 | 4 |

The Phase 1 representative baseline constructed `8 / 11` pages with three
ScheduleWidgets. The remaining repeated rendering is intentionally left for
Phases 4, 5, and 7; no hidden top-level feature is constructed during this
phase's startup route.

## Tasks
### 3.1 Audit eager PageManager construction
Review all pages currently created by `PageManager::initialize()` or equivalent startup paths.

At minimum, evaluate these for lazy construction:

- My Classes;

- standalone Schedule;

- Sub Prep;

- Testing Classes;

- Teacher Info;

- Native English Teachers;

- GS Team;

- Classes;

- Campus Dashboard;

- PDF Viewer.

Calendar should remain lazy.

### 3.2 Register factories without instantiating pages
Desired pattern:
```cpp
registerFactory(PageType::SubPrep, ...);

registerFactory(PageType::TeacherInfo, ...);

...
```
Do not call `ensurePage()` during startup unless that page is necessary for the selected initial view.

### 3.3 Minimize the startup page graph
If startup lands on ****My Workspace → Schedule****, aim for approximately:
```text
MainWindow

├─ navigation/sidebar shell

└─ PageManager

   └─ MyWorkspacePage

      └─ visible Schedule content
```
Any hidden My Workspace subpage should also be considered for lazy child creation where practical.

### 3.4 Ensure database state works for lazy pages
A page created after startup must correctly receive:

- database-open state;

- active database/service references;

- current theme/font inherited from the application;

- current navigation context;

- any required feature state.

Do not require eager page construction simply to propagate state.

### 3.5 Verify navigation behavior
All navigation destinations should remain visible and functional before their widgets are constructed.

## Acceptance Criteria
At startup:
```text
registered pages = all required application pages

instantiated pages = only those required for initial view
```
No hidden top-level feature should be created merely because it exists in navigation.

---
