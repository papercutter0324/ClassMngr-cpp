# Phase 5 — Shell and First Feature Slice

> Cross-phase progress history is tracked in [progress-log.md](progress-log.md);
> the active-phase handoff is in [00-START-HERE.md](00-START-HERE.md).

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
gallery predate the full cross-feature Qt table parity review. The owner-
approved table-parity handoff records the retained Qt contract and closes this
Phase 5 gate. Phase 6 must still revisit any table-like list or detail surface
already implemented, including its density, selection, focus,
empty/error states, localization, and DPI behavior. Use the shared
[WinUI table layout and style parity plan](../../docs/porting/windows-winui/table-parity-plan.md);
the Phase 5 handoff is recorded in
[phase5-table-parity-handoff.md](../../docs/porting/windows-winui/phase5-table-parity-handoff.md).
Do not treat the existing prototype captures as acceptance evidence for the
production table families, and do not call the current Campus list/detail
geometry parity-accepted until it has been reconciled with the retained Qt
selector-plus-tabbed-form design or has a separate approved exception. This
remaining reconciliation is Phase 6 work, not a blocker on the accepted Phase
5 handoff.

## File-dialog decision

The WinUI port uses the operating system's native file-selection surfaces for
file and folder workflows. The existing Phase 5 Open path uses an HWND-bound
`FileOpenPicker` and keeps the picker outside the XAML shell. Future create,
save, import, export, and directory-selection slices must use the matching
native picker instead of porting the Qt-only `QFileDialog` customization and
icon/sidebar styling. The current Phase 5 slice implements the create,
save/export, close, and folder-export paths through those native boundaries;
the owner-approved interactive picker and unsaved-change review is recorded in
the [Phase 5 exit review](../../docs/porting/windows-winui/phase5-exit-review.md).

## Validation

- Users can launch, create/open a copied database, navigate, change theme and
  language, and complete the selected read-only workflow.
- Keyboard, Korean IME preservation, focus restoration, DPI, text scaling,
  and screen-reader semantics are tested where applicable.
- The selected page uses engine use cases and cannot issue ad hoc SQL.
- Paired evidence confirms retained Qt content-area hierarchy, geometry,
  density, styling, and workflow. Native WinUI chrome differences are
  documented separately; the current list/detail prototype is not a waiver of
  Qt-derived visual parity.

## Exit Gate

The WinUI application is a usable read-only ClassMngr product slice with a
complete shell, safe database opening, accepted parity evidence, and measured
startup/runtime behavior.
