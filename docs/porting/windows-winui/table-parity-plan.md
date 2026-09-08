# WinUI 3 table layout and style parity plan

## Purpose

Give every WinUI table-like surface the same information hierarchy, density,
layout, visual states, and interaction model as the retained Qt application,
while using first-party WinUI controls and the shared portable engine.

This is a follow-up to the completed Phase 4 control and virtualization work.
Phase 4 proved the WinUI primitives and performance strategy; it did not yet
accept visual parity for every Qt table family. The production implementation
belongs to Phase 6 and must be completed once for each feature slice.

The target is functional and visual parity in the table content area. Native
WinUI window chrome and unavoidable platform-control differences may remain
when they are documented and do not change the user's workflow or information
hierarchy.

The Phase 5 handoff that applies these rules to the existing Campus
surface—and records the retained Qt widths, headers, borders, shading, and
state contracts—is [phase5-table-parity-handoff.md](phase5-table-parity-handoff.md).

## Qt baseline inventory

Before implementing a table family, record its retained-Qt baseline and link
the source and capture IDs in the parity matrix. The initial inventory includes:

- roster rows, columns, grouped headers, validation decorations, selection,
  clipboard, row movement, and custom column/row operations;
- schedule rows and day cells, slot states, teacher/room lines, testing
  assignments, empty slots, and compact preview geometry;
- speaking-evaluation rows and score cells, notes affordances, pasted ranges,
  fill-down, dirty/error states, and analytics navigation;
- read-only analytics ranking tables and directory/list surfaces;
- schedule-import review, teacher/import, batch-selection, and other dialog
  tables built with `QTableWidget` or `QTableView`; and
- any additional `QAbstractTableModel`, `QTableView`, `QTableWidget`, header
  view, item delegate, or table renderer found during the audit.

Useful starting points are the retained [roster model](../../../src/features/roster/ui/roster_model.h),
[roster table view](../../../src/features/roster/ui/roster_table_view.h),
[schedule view model](../../../src/features/schedule/ui/schedule_view_model.h),
[schedule table renderer](../../../src/features/schedule/ui/schedule_table_renderer.h),
[speaking-evaluation model](../../../src/features/speaking_eval/ui/speaking_eval_model.h),
and [analytics ranking model](../../../src/features/classes/ui/class_analytics_ranking_model.h).

## Parity matrix and baseline captures

Create one row per Qt table family and state, with these fields:

- feature/page/dialog and table family;
- Qt source anchors and fixture/database identity;
- column order, header hierarchy, widths, minimums, alignment, wrapping,
  truncation, row height, cell padding, and scroll behavior;
- colors, typography, borders/gridlines, alternating fills, hover, selection,
  focus, disabled, dirty, validation, warning, and error states;
- editable cells, editor type, commit/cancel rules, keyboard traversal,
  clipboard behavior, row movement, fill/paste behavior, and undo/redo;
- empty, loading, error, conflict, large-data, Korean-text/IME, and DPI
  scenarios; and
- the matching WinUI implementation, capture paths, semantic assertions, and
  acceptance status.

Capture the Qt reference at the supported 100%, 125%, 150%, 200%, and 300%
DPI settings, light and dark themes, English and Korean text, populated and
empty states, and representative validation/dirty states. Measure geometry
from the rendered control rather than copying incidental Qt pixel values into
code. Record the intended design tokens: page/card spacing, table/header/row
density, column widths, text styles, border thickness, state colors, and focus
indicators.

## WinUI presentation architecture

Use small product-scoped table presentation components, not a general-purpose
replacement for `QTableView` or a custom drawing toolkit:

1. Define column metadata and stable row/cell keys in the feature view model.
   The model exposes display text, alignment, editability, dirty state,
   validation state, and automation names.
2. Keep engine records and drafts separate from realized visuals. Engine use
   cases and validators remain authoritative; the WinUI layer only adapts
   UTF-8 values to binding-facing values and manages presentation state.
3. Put shared table tokens and control states in the WinUI resource dictionaries
   beside the existing typography, spacing, color, and control styles.
4. Render headers and rows with templates. Use `Grid` for the layout of one
   header or row, not for materializing the entire logical dataset.
5. Use standard `TextBlock`, `TextBox`, `ComboBox`, `CheckBox`, focus, selection,
   clipboard, and automation behavior. A focused-cell editor may replace the
   display element, but scrolling must not discard the draft.
6. Keep feature-specific behavior in the feature view model/page: paste-range
   parsing, fill-down, row transfer/reorder, undo/redo, commit/cancel, and
   autosave call engine contracts and shared validators.

The accepted primitive mapping is:

| Qt table family | WinUI presentation | Required parity focus |
| --- | --- | --- |
| Simple class, directory, or ranking rows | `ListView` with `ItemsStackPanel` and a row template | Row density, selection, sorting/filter state, headers, empty/error states |
| Roster | `ListView` with a row template and bounded selection/editor state | Grouped/header layout, column widths, validation, clipboard, transfer, row movement |
| Schedule/time slots | `ItemsRepeater` rows inside a bounded scrolling region | Fixed time/day geometry, cell states, teacher/room wrapping, testing overlays, compact mode |
| Speaking evaluation | Virtualized `ItemsRepeater` rows with cell templates | Score-cell appearance, focused-cell editing, pasted ranges, fill-down, notes, dirty/error states |
| Small review or batch dialog table | A bounded `ListView`, `ItemsRepeater`, or ordinary `Grid` as appropriate | Dialog geometry, column alignment, selection and confirmation behavior |

`ListView` uses its internal scrolling and an `ItemsStackPanel`. An
`ItemsRepeater` uses a bounded `ScrollViewer`, a vertical layout, and a small
cache. Do not place a table repeater in an unconstrained vertical
`StackPanel`. Realized row/cell presentations must remain bounded by the
viewport plus cache, as required by the [virtualization decision](phase4-virtualization-decision.md)
and [large-data gate](phase4-large-data-gate.md).

## Feature implementation sequence

### 1. Shared table styling and geometry

- Add the table-specific resource keys for header, row, cell, selection, hover,
  focus, disabled, dirty, validation, warning, and error states.
- Implement the shared header/row/cell template patterns and a column-metrics
  policy that supports the widths and wrapping identified in the Qt baseline.
- Define how headers stay aligned with rows during resize, DPI changes,
  localization, and horizontal scrolling.
- Add a small table gallery with deterministic rows, long Korean/English text,
  validation, selection, focus, and empty/error states.

### 2. Roster parity

- Translate the Qt column/group-header structure and row density into a
  virtualized `ListView` row template.
- Reproduce required/editable column styling, duplicate-name and cell-error
  indicators, selected-row treatment, row-number/indicator treatment, and
  custom-column behavior.
- Preserve keyboard traversal, direct typing, clipboard copy/cut/paste,
  clear, row transfer, row reorder, and unsaved-change behavior.
- Verify that an edit remains associated with its stable row/column key after
  the row is recycled.

### 3. Schedule parity

- Translate the Qt schedule renderer's header, time column, day columns, row
  height, wrapping, slot backgrounds, and compact-preview geometry into the
  row/cell templates.
- Preserve empty/essay/lunch/testing slot visuals, teacher and room lines,
  intensive/regular modes, testing assignments, and cell-level actions.
- Keep slot-state transitions and conflict rules in the engine-backed feature
  model; templates only present state and dispatch commands.
- Verify header/body alignment, scrolling, row-height measurement, and the
  no-data/empty schedule presentation.

### 4. Speaking-evaluation parity

- Translate the Qt header view, delegate appearance, score/comment cell
  states, notes affordance, and analytics navigation into virtualized row and
  cell templates.
- Render display cells cheaply and activate no more than one editor at a time.
- Preserve immediate typing, tab/arrow traversal, score normalization,
  pasted tab/newline ranges, fill-down, clear, undo/redo, notes, validation,
  and dirty-state presentation.
- Keep edited values in the view-model draft/engine-facing model so recycling
  cannot lose an edit or create one editor per logical cell.

### 5. Other table-like surfaces

- Audit and migrate analytics rankings, staff/teacher directories, import
  review tables, batch-selection/review tables, assignment tables, and any
  remaining table-like Qt dialogs.
- Select the simplest primitive that preserves the baseline: `ListView` for
  row collections, `ItemsRepeater` for dense repeated rows/cells, and a normal
  `Grid` only for genuinely small fixed content.
- Apply the same resource tokens, column-metrics policy, automation naming,
  empty/error states, and paired visual evidence.

## Validation and acceptance

Each table family is accepted only when all of the following are recorded:

- paired Qt/WinUI captures show equivalent layout, density, hierarchy, styling,
  text handling, and visual states at the required DPI/theme/language points;
- semantic tests cover headers, row/cell selection, focus, keyboard traversal,
  editing, commit/cancel, validation, clipboard, scrolling, dirty state, and
  failure recovery;
- Korean IME composition and pasted-range behavior are verified without
  replacing text during composition;
- representative edits survive virtualization and database round trips work
  Windows-to-Qt and Qt-to-Windows;
- large workloads meet the established realization, frame, memory, and
  release budgets; and
- x64 and x86 Debug/Release builds and the slice's engine, semantic, and
  visual tests pass.

The table workstream is complete only when the inventory has no unreviewed
Qt table family, every parity-matrix row has accepted evidence, and no table
requires an unapproved external grid dependency. If a first-party primitive
cannot meet a documented requirement, record the gap and run the existing
external-dependency review before adding a package.
