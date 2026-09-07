# Phase 5 — Shell and First Feature Slice

> Progress is tracked in [00-START-HERE.md](00-START-HERE.md).

## Goal

Deliver the complete Windows shell and one representative read-only feature
through the shared engine.

## Implementation Sequence

1. Port startup, splash/progress, settings resolution, main window, menu and
   accelerators, sidebar, navigation history, theme/language switching, and
   update notifications.
2. Port database create/open/recent-file flows through engine use cases and
   native Windows file/folder pickers (`FileOpenPicker`, `FileSavePicker`, and
   `FolderPicker`), rather than recreating the Qt custom `QFileDialog` UI.
3. Mirror `PageManager` lazy construction and state-retention semantics without
   creating pages to query navigation state.
4. Port the Getting Started experience and global error/dirty-state behavior.
5. Port one representative read-only slice, preferably campus or staff
   directory, including queries, images, scrolling, details, localization,
   and resource loading.
6. Add paired Qt/WinUI scenarios for startup, shell states, database open,
   navigation, empty/populated/error states, and the selected feature.
7. Measure cold/warm startup, first paint, first navigation, resize, memory,
   and handle counts against the Phase 0 budgets.

## Revisit before Phase 5 handoff

The completed Phase 5 read-only list/detail slice and the Phase 4 control
gallery predate the full cross-feature Qt table parity review. Before Phase 6
starts, record the table-parity handoff and revisit any table-like list or
detail surface already implemented, including its density, selection, focus,
empty/error states, localization, and DPI behavior. Use the shared
[WinUI table layout and style parity plan](../../docs/porting/windows-winui/table-parity-plan.md);
do not treat the existing prototype captures as acceptance evidence for the
production table families.

## File-dialog decision

The WinUI port uses the operating system's native file-selection surfaces for
file and folder workflows. The existing Phase 5 Open path uses an HWND-bound
`FileOpenPicker` and keeps the picker outside the XAML shell. Future create,
save, import, export, and directory-selection slices must use the matching
native picker instead of porting the Qt-only `QFileDialog` customization and
icon/sidebar styling. The current Phase 5 slice implements the create,
save/export, close, and folder-export paths through those native boundaries;
their interactive picker and unsaved-change review remains part of the exit
gate.

## Validation

- Users can launch, create/open a copied database, navigate, change theme and
  language, and complete the selected read-only workflow.
- Keyboard, Korean IME preservation, focus restoration, DPI, text scaling,
  and screen-reader semantics are tested where applicable.
- The selected page uses engine use cases and cannot issue ad hoc SQL.
- Paired evidence confirms equivalent content and workflow without requiring
  pixel-identical Qt styling.

## Exit Gate

The WinUI application is a usable read-only ClassMngr product slice with a
complete shell, safe database opening, accepted parity evidence, and measured
startup/runtime behavior.
