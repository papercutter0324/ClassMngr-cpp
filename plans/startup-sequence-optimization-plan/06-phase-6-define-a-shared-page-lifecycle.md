<!-- ClassMngr Startup Optimization Plan — Phase 6 -->

> **Codex scope:** Read `00-START-HERE.md` plus this file. Implement and validate **Phase 6 only**. Do not start later phases.

# Phase 6 — Define a Shared Page Lifecycle — Complete (2026-08-27)

## Objective

Make page behavior predictable across all platforms and prevent constructors or show events from becoming hidden initialization paths.

## Completed Implementation

- `BasePage` now provides a shared stale lifecycle: `markStale()`,
  `activate()`, and `deactivate()`. Database-state changes mark a page stale;
  activation refreshes only stale content; deactivation releases temporary
  feature resources.
- `PageManager` marks newly created pages stale, then activates only the
  selected page once database state is available. Global refreshes and
  preference changes now mark affected pages stale and refresh only the active
  target.
- My Workspace delegates stale/activation/deactivation state to its active
  child page. Calendar construction remains deferred until first needed;
  Calendar preferences can obtain its provider without activating its data
  load.
- Removed data work from constructors and full data reloads from heavy
  `showEvent()` paths. ScheduleWidget construction now builds its UI shell
  only; the first visible Schedule activation performs the single data-backed
  render. Clearing database state also clears its model without rendering.
- Calendar, Personal Details, My Classes, Sub Prep, Campus Dashboard, Classes,
  Testing Classes, and Staff Directory now load through the shared lifecycle.
  The remaining page `showEvent()` handlers perform layout-only work.
- Schedule, testing-class, and import flows mark hidden pages stale instead of
  immediately querying or rendering them. Focused tests now explicitly
  activate direct page/widget fixtures before asserting loaded content.

## Validation

- Windows x64 Debug builds completed for the application and all focused page,
  lifecycle, schedule, and startup-performance test targets.
- `ClassMngrBasePageTests`, `ClassMngrPageManagerTests`,
  `ClassMngrMyWorkspacePageTests`, `ClassMngrScheduleWidgetTests`,
  `ClassMngrSubPrepPageTests`, `ClassMngrTestingClassesPageTests`,
  `ClassMngrClassesPageTests`, `ClassMngrCampusDashboardPageTests`,
  `ClassMngrStaffDirectoryPageTests`, and
  `ClassMngrStartupPerformanceTests` passed with the offscreen platform.
- The startup-performance regression now asserts zero ScheduleWidget renders
  for a no-database minimal start and exactly one render for the representative
  database start.
- A representative Windows Debug capture using `Testing-copy.tps` recorded:

| Startup complete | Peak working set | Working set | Private usage | Widgets | Pages | ScheduleWidgets | Schedule renders |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 3.419 s | 707.1 MiB | 704.8 MiB | 598.5 MiB | 211 | 1 / 11 | 1 live / 1 created | 1 |

  The one-render startup diagnostic passed. These are raw Debug-host capture
  values; the structural assertion is the single created ScheduleWidget and
  its single data-backed render.

## Tasks
### 6.1 Establish lifecycle responsibilities
Use existing architecture where possible, but enforce these semantics.

#### Construction
Allowed:

- create lightweight UI structure;

- connect signals;

- initialize trivial local state;

- create lightweight models.

Avoid:

- expensive database queries;

- resource-pack acquisition;

- full dataset loading;

- expensive rendering.

#### First-use preparation
Allowed:

- acquire feature resources;

- create expensive child widgets;

- initialize feature-specific infrastructure.

#### Activation
Allowed:

- load visible data if stale;

- update content required for the current navigation context.

#### Refresh
Use only when data or relevant preferences changed.

#### Deactivation
Where beneficial:

- pause timers;

- release temporary resources;

- discard short-lived caches.

Do not destroy/recreate pages simply because the user navigates away.

### 6.2 Audit heavy `showEvent()` work
Review all page `showEvent()` implementations.

A `showEvent()` should not cause a complete data reload when activation has already loaded current data.

In particular, remove duplicate schedule refresh behavior.

### 6.3 Introduce stale/dirty state where appropriate
Examples:
```cpp
bool m_needsRefresh = true;
```
or a data-generation/version mechanism.

When hidden data changes:
```text
mark page stale
```
When the page becomes active:
```text
refresh only if stale
```
### 6.4 Keep behavior shared
Do not create Windows-only lazy behavior.

All pages should follow the same lifecycle regardless of platform.

## Acceptance Criteria
First use:
```text
construct once

→ prepare once

→ load/render once
```
Returning to an unchanged page:
```text
activate

→ no unnecessary query

→ no unnecessary full render
```
---
