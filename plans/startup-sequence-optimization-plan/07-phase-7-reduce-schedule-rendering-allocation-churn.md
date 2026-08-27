<!-- ClassMngr Startup Optimization Plan — Phase 7 -->

> **Codex scope:** Read `00-START-HERE.md` plus this file. Implement and validate **Phase 7 only**. Do not start later phases.

# Phase 7 — Reduce Schedule Rendering Allocation Churn — Complete (2026-08-27)
## Objective
Reduce transient allocations, QWidget creation, deferred deletion, and repeated full-table rebuilding.

Do this after unnecessary schedule construction and refreshes have been removed.

## Completed Implementation

- `ScheduleTableRenderer` keeps a render snapshot per table. An identical
  view model and cell appearance returns immediately, with `fullRender=false`
  and zero created/removed items or cell widgets. A change to only the maximum
  visible rows updates table geometry without rebuilding cells.
- Same-shape changes are now incremental. Stable time-column items are reused,
  and only cells whose rendered input changed have their widget removed with
  `deleteLater()` and recreated. Structural changes (new row count, preview
  mode, or teacher-name language) still use the established safe full rebuild.
- Language changes explicitly invalidate the snapshot so translated headers and
  cell text rebuild correctly. Hidden standalone and Sub Prep pages mark their
  schedule stale during translation and rebuild it only after activation.
- Startup profiling now records `fullRender=true|false` for each schedule
  render and counts only full table renders in `scheduleRenderCount`.
- The shared lifecycle from Phase 6 already marks hidden SchedulePages stale
  and activates only the current page. `ClassMngrPageManagerTests` continues
  to validate that behavior; this phase adds no platform-specific lifecycle.

## Renderer Modernization Follow-up

Keep the current `QTableWidget` renderer for this phase. A future dedicated
migration should replace it with `QTableView`, a `QAbstractTableModel`, and a
delegate that paints schedule cells without a QWidget per cell. That work
should preserve the current interaction/accessibility behavior and include
light/dark-theme, compact-preview, translation, and print visual regressions.

## Validation

- Windows x64 Debug builds completed for `ClassMngrScheduleWidgetTests`,
  `ClassMngrPageManagerTests`, `ClassMngrSubPrepPageTests`, and
  `ClassMngrStartupPerformanceTests`; all passed with the offscreen platform.
  The new renderer test verifies both a no-op render (zero
  table-item/cell-widget allocations) and a one-cell change that preserves
  stable cells.
- A representative Windows Debug startup capture using `Testing-copy.tps`
  recorded 3.020 s startup, 202.8 MiB peak working set, 213 widgets, one live
  and created ScheduleWidget, and one initial full render (6 table items,
  30 cell widgets, no deferred deletions). The initial visible render remains
  necessary; this phase removes subsequent unchanged work.

## Tasks
### 7.1 Skip unchanged renders
Determine the effective schedule render input, such as:

- schedule data;

- display mode;

- relevant date/day selection;

- schedule-related preferences.

If it is unchanged, return without rebuilding.

### 7.2 Do not render hidden ScheduleWidgets
When schedule data changes:

- visible schedule → refresh if needed;

- hidden schedule → mark stale.

Render the hidden schedule only when activated.

### 7.3 Reduce clear-and-rebuild behavior
Audit:

- `ScheduleTableRenderer::render()`;

- `clearCellWidgets()`;

- `QTableWidgetItem` recreation;

- `setCellWidget()` usage;

- `deleteLater()` volume.

Where practical:

- update only changed cells;

- reuse stable items/widgets;

- avoid replacing identical cell contents;

- avoid unnecessary widget destruction/recreation.

Do not replace safe `deleteLater()` usage with unsafe immediate deletion just to reduce a metric.

### 7.4 Long-term renderer modernization
Plan a separate migration from:
```text
QTableWidget

\+ QTableWidgetItem

\+ QWidget per schedule cell
```
to:
```text
QTableView

\+ QAbstractTableModel

\+ QStyledItemDelegate/custom delegate painting
```
Treat this as a separate implementation step with visual regression checks.

## Acceptance Criteria
An unchanged schedule refresh request results in:
```text
full render = false

new table items = 0

new cell widgets = 0
```
Hidden schedules do not render until activated.

---
