# Project Diary

## OpenXLSX Schedule Import Phase 1 - 2026-09-13

- The local `C:\Git\openxlsx` tree is content-identical to Codeberg's
  `development-aral` commit `ece329af84b370a8b77f5a3ee0e30509ad0f0bf9`; its
  local CMake version is 0.5.2 development work, while its vcpkg manifest says
  0.5.1. Pin the commit, not a moving branch or ambiguous version label.
- OpenXLSX is now represented by a repository-owned submodule at
  `third_party/openxlsx/source`. The developer-local source path remains audit
  evidence only.
- The initial dependency policy is static OpenXLSX with docs, samples, sample
  source installation, tests, benchmarks, libzip, automatic fetching, and
  forced fetching disabled. The initial ZIP path is miniz/Zippy and Windows
  Unicode support is the standalone nowide archive declared by OpenXLSX's CMake.
- Exact refs and notices were recorded for PugiXML 1.14, miniz 3.0.2, and
  standalone nowide v11.3.1 under `licenses/openxlsx/`; switching to Boost.Nowide
  or libzip later requires a new provenance/license entry.
- The offline MSVC configuration probe confirmed that network fetching is
  disabled, then failed on the intentionally unprovisioned local `nowide`
  target. Treat this as a Phase 2 build-integration dependency, not as evidence
  that the offline build passes.

### Phase 1 lesson

A pinned top-level source is insufficient for reproducible native builds when
OpenXLSX's dependency manager can fetch transitive libraries. The build bridge
must provision exact native targets and preserve the no-network policy before
the reader implementation is connected.

## OpenXLSX Schedule Import Phase 2 - 2026-09-13

- The WinUI route is a CMake custom target that invokes PowerShell and then a
  separate MSBuild project. A CMake target link interface does not cross that
  project boundary, so the durable bridge is a generated, target-local MSBuild
  property sheet passed by the existing wrapper.
- PugiXML, miniz, and standalone-nowide are pinned as submodules and added as
  local CMake targets before OpenXLSX. OpenXLSX's FetchContent path remains
  disabled. Static library outputs are placed in an architecture-specific
  CMake build tree and split by Debug/Release configuration.
- miniz's upstream CMake project would enable a C language probe that is
  incompatible with this host's restricted MSBuild FileTracker environment.
  Keeping the sources in C language mode via MSVC `/TC` avoids a source fork and
  still produced the correct static library.
- x64 Debug, x64 Release, and Win32 Release native smoke builds and executions
  passed. A full WinUI target attempt reached the existing engine FileTracker
  failure after the new dependency chain built, so no loader claim is made for
  the application until that host issue is cleared.

## Schedule Import Qt Workflow Audit — 2026-09-13

- The retained Qt flow is two-stage. `ScheduleImportDialog` is modal and
  owns workbook path/browse, Regular versus Intensives, visible worksheet,
  detected-user selection, profile-name mismatch confirmation, asynchronous
  load progress, timeout, cancellation, and source errors. Its button is
  `Load` until a workbook is loaded, then `Next`.
- `Next` opens a separate modal `ScheduleImportReviewDialog`. Review owns the
  read-only schedule preview, Classes and Korean Teachers resolution tabs,
  optional Unrecognized Cells acknowledgement, intensive update/replace
  choice, dynamic match details/colors, conflict warnings, validation status,
  summary, and Import confirmation. Back closes only review and returns to the
  source state; Cancel leaves data unchanged.
- The retained Qt parser is intentionally the workbook/OOXML adapter. WinUI
  currently has only a normalized single-class form inside a ContentDialog;
  matching the full Qt flow therefore requires a deliberate WinUI codec or
  adapter boundary in addition to dialog-state work.
- After successful Qt import, MainWindow refreshes both schedule pages and the
  teacher/sidebar state. Import errors keep the review dialog open; success
  shows a result message and closes the workflow.

### Implementation handoff

- The WinUI import action now stays in an owned `ContentDialog`; it does not
  select or load an Import page. The source state follows the Qt progression:
  initial file/type prompt, ready-to-read path after Browse, valid workbook and
  worksheet state after Load, then Next into review.
- Review is a second state of the same modal with a read-only schedule board,
  Classes and Korean Teachers tabs, dynamic resolution cards, live validation,
  summary text, and Back/Import/Cancel footer semantics. ContentDialog
  intermediate button events explicitly cancel auto-close so Load, Next, and
  Back preserve the workflow.
- Preview and apply continue to use the existing engine service and plan
  contracts; successful apply refreshes the schedule, while validation/apply
  failures leave review visible.
- The WinUI loader currently checks `.xlsx` extension/readability and supplies a
  normalized staged provider. The retained Qt OOXML reader remains the adapter
  boundary, so actual workbook decoding is a known follow-on rather than a new
  Qt dependency in the engine.
- Verification passed: x64 Debug WinUI build/link, staged
  `--phase6-schedule-test` (exit 0), focused schedule-import engine test (1/1),
  and `git diff --check`.

## Schedule Import Dialog Follow-up

- The WinUI Schedule Import action is now modal: the import control tree is
  retained for the existing normalized engine workflow but is hosted in an
  owned `ContentDialog`, and the Import button no longer selects a page.
- Removing the Import Pivot item changes Testing Classes to Pivot index 1;
  both the toolbar action and the testing-slot navigation path must use that
  index.
- The dialog follow-up was completed by the Qt-workflow implementation above.
  The final elevated WinUI build linked successfully and the staged schedule
  diagnostic exited 0; no process was terminated.

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

## OpenXLSX Schedule Adapter — Phase 3

- The existing native `classmngr::engine::ScheduleImportWorkbook` and
  `classmngr::engine::Result` types are the correct reader result boundary;
  introducing a second Qt-free domain model would create an unnecessary
  conversion seam.
- `ScheduleWorkbookLayout` is a value-owned intermediate model containing
  sheet visibility, cells, merged ranges, normalized style facts, and
  diagnostics. Reader adapters must finish this mapping while their workbook
  document is alive.
- `ScheduleWorkbookReader` is a virtual, codec-neutral substitution point in
  the Qt-free engine include boundary. It carries `std::filesystem::path`,
  `ScheduleImportKind`, and a cooperative cancellation callback only.
- The host MSVC `FileTracker` access-denied failure also blocks the new
  contract-test target before compilation; this is an environment limitation,
  not evidence against the contract.

## OpenXLSX Schedule Adapter — Phase 4

- Schedule-format interpretation is now a Qt-free engine component. The
  existing Qt byte reader is an adapter that maps its workbook snapshot into
  `ScheduleWorkbookLayout`, calls the shared interpreter, and converts the
  native result back for the retained Qt UI.
- The interpreter deliberately uses explicit UTF-8 handling for Hangul
  teacher names and weekday syllables, while keeping the existing template's
  ASCII course/room/time grammar and native meeting-pattern rules.
- The direct MSVC interpreter test passed. The CMake/MSBuild route continues to
  fail in the host FileTracker static initializer before source compilation;
  this remains an environment limitation to carry into later validation.

## OpenXLSX Schedule Adapter — Phase 5

- OpenXLSX's public worksheet API is appropriate for read-only cell, merge, and
  visibility traversal, but raw OOXML is still required for style/theme/indexed
  color facts. Keep that XML detail private to the Windows reader.
- `XLWorksheet::rows()` plus `row.findCell(column)` preserves existing sparse
  cells without creating gaps. Avoid the convenience cell-range traversal when
  importing because it can materialize missing cells.
- A root OOXML node must be handled as the root: `styles.xml` is rooted at
  `<styleSheet>`, not a child named `styleSheet`. This was caught by the native
  fixture test and fixed before integration.
- Cancellation must be inside the reader's exception boundary. A cancellation
  check before the `try` block escapes as an uncaught exception instead of the
  contract's `ErrorCode::Cancelled` result.
- The runtime fixture uses OpenXLSX to write a real ZIP/XML workbook and the
  native reader to read it, keeping the catalogue copyright-safe and
  reviewable while covering Unicode paths, Korean content, hidden sheets,
  merges, styles, normal/intensive semantics, and safe failures.
- Direct MSVC Debug compile/link/run passed. The configured WinUI/MSBuild route
  remains blocked before source compilation by the host FileTracker
  access-denied failure.

## OpenXLSX Schedule Adapter — Phase 6

- The native reader factory can be integrated without exposing OpenXLSX or
  codec types through `MainWindow.xaml.h`; only the native reader contract and
  value-owned workbook are retained by the dialog.
- A generation token plus an atomic cancellation flag is sufficient to make
  asynchronous reader results safe for the dialog lifecycle: cancellation is
  cooperative while parsing, and stale results are rejected again on the UI
  thread before any control mutation.
- The WinUI source dialog can mirror the Qt workflow without page navigation by
  switching roots inside one ContentDialog and preserving workbook/selection
  state across Back.
- The existing engine archive in the build directory may be stale after an
  engine source change even when the WinUI sources compile; direct validation
  must include the current interpreter object or a freshly rebuilt engine
  library. This is separate from the host CMake FileTracker failure.
- Phase 7 must remove the collapsed legacy normalized controls rather than
  leave them as a latent production path, and must make profile mismatch a
  real confirmation gate before review.

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
