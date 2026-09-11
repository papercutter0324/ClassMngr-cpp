# Project Diary

## WinUI Parity Pass

- Deployment `winui_parity_pass_20260912` used the Medium route for one
  dependency-ordered parity slice. The required Qt reference pictures were
  already tracked locally under `artifacts/phase0/windows-qt-visual`; no
  reattachment was needed.
- Navigation now funnels feature dirty flags through one
  Save/Discard/Keep-editing continuation. It restores the prior selection when
  a user keeps editing and preserves unrelated dirty feature state when one
  feature is saved or discarded.
- Schedule hover customization is persisted as
  `schedule/hoverBorderColor`, defaulting to the existing gold/yellow color.
  It changes the pointer-over border only, leaving class-cell fills intact.
- Schedule testing controls are member-owned because controls appended to a
  retained page fragment are not reliably discoverable through a parent-tree
  lookup during diagnostics. Member ownership keeps the visible control and
  its state synchronized.
- Speaking Analytics keeps the engine dashboard as the source of truth and
  renders the year-to-date trend as a WinUI Canvas line chart with guide lines,
  markers, and evaluation labels. This remains a presentation adapter; no
  engine contract was changed.
- The native capture runner passed, but this host produced an 800x600 window
  while the Qt references are 1270x1040 at 150%. The evidence therefore
  records lifecycle/capture success and source/layout review, not a
  same-dimension pixel-match claim.
- The workflow intake searched for the configured
  `codex_workflow/medium_route.md` but that file was unavailable in the user
  profile. The deployment still followed the repository's Medium-route and
  orchestration instructions.

## Sub Prep Tabs and Frame Lifecycle

- Deployment `sub_prep_tabs_return_20260911` confirmed the WinUI navigation
  failure was a visual-host lifecycle issue: Sub Prep retained member-owned
  controls while Frame navigation supplied a fresh `Page`, leaving the new
  page empty. Disabling page caching and reattaching a retained ScrollViewer
  mirrors the successful My Information fix.
- Top-level Pivot and SelectorBar labels now use shared `Phase3TopTab*`
  resources, and `buildTopTabHeader` applies the same typography to Pivot
  headers on My Workspace, Sub Prep, and Campus Directory.
- The phase-6 Sub Prep check explicitly navigates away and back before testing
  the populated workflow; it passed with the targeted WinUI build and focused
  Classes/Campus runtime verifiers.

## Classes Navigation Layout

- The Qt Classes page keeps section navigation and class/grade navigation outside
  the selected editor stack. When section navigation is shown above the class
  selectors, its placeholder page spacing must be zero; otherwise the unused
  stacked-page area creates an uneven gap and makes the navigation rows appear
  to shift.

## Decisions and Lessons

- For the WinUI My Workspace lifecycle bug, do not rely on late Pivot
  selection callbacks to restore a stateful editor. Disable Home-page caching
  and explicitly reattach the retained personal-details view to the new host.
  The later parity pass included the focused phase verifiers and native
  capture validation.

- `ClassMngrEngine` is intentionally Qt-free and configured before Qt discovery; keep portable engine changes independent of Qt where possible.
- Product selection happens before Qt package discovery so the Windows WinUI lane can configure without a Qt installation.
- Windows CMake presets use Visual Studio 18 2026 with the `v145` toolset; existing build directories created with another generator require a fresh reconfigure.
- The Qt Report Editor is more than a score form: it stores two private-note lists in
  one marked field, keeps them out of the printed report, gates AI prompts on both
  lists plus an E4--E6 class, and supports per-student cyclic navigation. The WinUI
  editor preserves that storage contract and prompt gate rather than inventing a new
  persistence format.
- A visual/native report renderer and actual PDF/print execution are explicitly
  Phase 7 adapters. The Phase 6 WinUI editor should provide an editable report-like
  surface and retain the existing renderer-neutral batch-plan handoff, not claim
  output parity it cannot yet deliver.
- A WinUI control stored in a member remains ineffective until it is appended to
  the active visual tree; review the visible control path as well as the prompt
  service input. Editable review rows must update their displayed validity and
  apply eligibility in the same text-change path as their backing model.
