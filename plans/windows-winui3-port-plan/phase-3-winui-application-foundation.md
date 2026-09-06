# Phase 3 — WinUI Application Foundation

> Progress is tracked in [00-START-HERE.md](00-START-HERE.md).

## Goal

Build the reusable WinUI 3 application infrastructure needed by every feature
without creating a parallel general-purpose UI framework.

## Implementation Sequence

1. Establish application activation, single-instance policy, shutdown,
   suspend/resume handling, command-line activation, and fatal-error reporting.
2. Build the main window, title bar, `NavigationView`, navigation history,
   lazy page construction, modal ownership, and state restoration.
3. Define C++/WinRT view-model conventions for observable state, commands,
   asynchronous work, cancellation, validation, and error presentation.
4. Create XAML resource dictionaries for colors, typography, spacing, icons,
   control styles, light/dark themes, and supported text scaling.
5. Implement localization from the shared resource catalog using Windows App
   SDK resource facilities. Preserve English variants and Korean behavior.
6. Add Windows adapters for settings, secure storage where required, files and
   folders, clipboard, URL/process launch, notifications, logging, and crash
   diagnostics.
7. Standardize dialogs, progress/cancellation, validation summaries, dirty
   state, and unsaved-change confirmation.
8. Define UI-thread and background-work rules. Engine operations must not
   capture XAML objects or update controls from worker threads.
9. Extend the Phase 0 scenario protocol to launch, settle, capture, close, and
   release real WinUI windows and dialogs.
10. Add semantic tests for navigation, commands, focus restoration, lifecycle,
    and resource lookup in addition to visual captures.

## Guardrails

- Prefer standard WinUI controls and composition behavior.
- Do not wrap every WinUI type in a project-owned abstraction.
- Keep view code free of SQL and business validation.
- Do not use custom drawing for forms, navigation, editable text, menus, or
  standard buttons.
- Preserve built-in automation names, labels, focus cues, high-contrast
  behavior, and text scaling even where formal end-to-end certification is
  deferred.

## Exit Gate

A localized, themeable, DPI-correct WinUI shell can navigate lazily, run and
cancel engine commands, display consistent dialogs and validation, restore
state, and produce deterministic visual and semantic test evidence.
