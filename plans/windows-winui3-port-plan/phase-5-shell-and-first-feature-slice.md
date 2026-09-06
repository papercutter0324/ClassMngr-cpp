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
   Windows file/folder pickers.
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
