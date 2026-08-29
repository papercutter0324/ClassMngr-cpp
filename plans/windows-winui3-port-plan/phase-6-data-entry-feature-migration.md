# Phase 6 — Data-Entry Feature Migration

> Progress is tracked in [00-START-HERE.md](00-START-HERE.md).

## Goal

Port all interactive feature slices to WinUI 3 while preserving validation,
autosave, transactions, imports, keyboard/IME behavior, and cross-platform data
compatibility.

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

## Per-Slice Requirements

- Load, create, edit, validate, save/autosave, cancel, undo/redo where present,
  and recover from engine or platform failures.
- Cover empty, populated, large, dirty, validation, conflict, and error states.
- Use engine use cases and shared validators for both manual entry and imports.
- Preserve Korean IME composition, keyboard selection/editing, clipboard,
  focus restoration, and unsaved-change rules.
- Use virtualized controls for large row/cell collections.
- Add paired Qt/WinUI visual scenarios plus semantic and persistence tests.
- Keep x64 and x86 feature builds and integration tests green for every
  migrated slice; architecture-specific code requires matching coverage.
- Verify Windows-to-Qt and Qt-to-Windows database round trips.

## Exit Gate

Every interactive parity-matrix row has accepted data, input, error, visual,
and performance evidence. The WinUI app safely edits databases also used by
the macOS and Linux Qt products without duplicated rules or schema behavior.
