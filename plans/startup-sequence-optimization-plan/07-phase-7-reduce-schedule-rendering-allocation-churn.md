<!-- ClassMngr Startup Optimization Plan — Phase 7 -->

> **Codex scope:** Read `00-START-HERE.md` plus this file. Implement and validate **Phase 7 only**. Do not start later phases.

# Phase 7 — Reduce Schedule Rendering Allocation Churn
## Objective
Reduce transient allocations, QWidget creation, deferred deletion, and repeated full-table rebuilding.

Do this after unnecessary schedule construction and refreshes have been removed.

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
