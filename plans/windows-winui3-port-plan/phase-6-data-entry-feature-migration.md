# Phase 6 — Data-Entry Feature Migration

> Cross-phase progress history is tracked in [progress-log.md](progress-log.md);
> the active-phase handoff is in [00-START-HERE.md](00-START-HERE.md).

## Goal

Port all interactive feature slices to WinUI 3 while preserving validation,
autosave, transactions, imports, keyboard/IME behavior, and cross-platform data
compatibility.

The [WinUI table layout and style parity plan](../../docs/porting/windows-winui/table-parity-plan.md)
is a required Phase 6 workstream. It covers the production layout, design,
styling, and interaction parity of schedules, rosters, evaluations, and every
other table-like Qt surface. Complete the shared table-parity revisit before
accepting the first table-heavy slice; then close the parity matrix one feature
family at a time.

## Migration Order

Port and accept each slice end to end before beginning several more:

1. Personal details and teacher directories.
2. Class details, class information, and notes.
3. Calendar viewing/editing and preferences.
4. Rosters, student transfers, and roster templates.
5. Schedules, imports, testing classes, and assignment dialogs.
6. Speaking-evaluation grid, notes, analytics, AI-comment workflow, and batch
   operations.
7. Substitute-preparation and bundled-document workflows.

## Table-parity workstream

Execute this work across the migration order, with the shared styling and
geometry pass first:

1. Reopen the completed Phase 4 table prototypes and the Phase 5 read-only
   slice. Establish the retained-Qt inventory, baseline captures, parity
   matrix, shared WinUI table resources, and row/header/cell template pattern.
2. Apply the shared pattern to roster tables, including grouped headers,
   validation, selection, clipboard, transfer, row movement, and editing.
3. Apply it to schedule tables, including time/day geometry, slot states,
   wrapping, compact preview, testing assignments, and empty states.
4. Apply it to speaking-evaluation tables, including score/comment states,
   one focused editor, pasted ranges, fill-down, notes, analytics navigation,
   undo/redo, dirty state, and validation.
5. Audit and migrate all remaining table-like directories, rankings, import
   reviews, batch dialogs, and assignment surfaces.
6. Accept each family with paired Qt/WinUI visual evidence and the semantic,
   IME, persistence, DPI, and large-data checks in the linked plan.

The revisit is a prerequisite to the Phase 6 exit gate even when the original
prototype or feature step was already marked complete. A completed prototype
may be reused as an interaction or performance baseline, but it does not close
the corresponding Qt visual-parity row.

## Per-Slice Requirements

- Load, create, edit, validate, save/autosave, cancel, undo/redo where present,
  and recover from engine or platform failures.
- Cover empty, populated, large, dirty, validation, conflict, and error states.
- Use engine use cases and shared validators for both manual entry and imports.
- Preserve Korean IME composition, keyboard selection/editing, clipboard,
  focus restoration, and unsaved-change rules.
- Keep localization within the existing catalog set: regional English
  (`en-US`, `en-GB`, `en-CA`, and `en-AU`) plus Korean (`ko-KR`). No additional
  language catalogs or resource qualifiers are in scope; unsupported system
  locales continue to use the existing English fallback policy.
- Use virtualized controls for large row/cell collections.
- Match the retained Qt table layout, density, column/header geometry,
  typography, colors, borders, selection/focus/validation/dirty states, and
  editing behavior through the table-parity matrix; screenshots alone are not
  sufficient without semantic and persistence evidence.
- Add paired Qt/WinUI visual scenarios plus semantic and persistence tests.
- Keep x64 and x86 feature builds and integration tests green for every
  migrated slice; architecture-specific code requires matching coverage.
- Verify Windows-to-Qt and Qt-to-Windows database round trips.

## Exit Gate

Every interactive parity-matrix row has accepted data, input, error, visual,
and performance evidence. The WinUI app safely edits databases also used by
the macOS and Linux Qt products without duplicated rules or schema behavior.
