# Phase 6 — WinUI Dialog Integration

**Previous:** [Phase 5](05-phase-5-openxlsx-reader-and-fixture-parity.md)

**Next:** [Phase 7](07-phase-7-hardening-performance-and-cutover.md)

## Goal

Replace the source dialog’s normalized fallback with asynchronous native reader
results while preserving the existing modal, step-by-step Qt-shaped workflow.

## Required User Flow

1. The first `Import Schedule` dialog opens with only spreadsheet selection;
   Load is disabled until a path is chosen.
2. Choosing a readable XLSX enables import-kind selection; Load remains disabled
   until a kind is selected.
3. Load opens and validates the workbook. On success, the dialog shows the
   compatible schedule/user selector, selects a safe default when unambiguous,
   and enables Next only after selection.
4. Next builds the real preview and opens the existing `Review & Reconcile`
   dialog. It remains a dialog, never a navigated page.
5. Back returns to the source dialog with the decoded workbook and previous
   choices preserved; Cancel discards dialog-local state; Import applies the
   reviewed engine plan atomically.

## Scope

- Replace `loadScheduleImportSource()` fallback construction in
  `MainWindow_schedule_editor.cpp` with the Phase 5 reader.
- Add dialog-local real workbook state, selected compatible sheet/user, and
  reader diagnostics.
- Update existing controls only as needed to reflect loading, failure, ready,
  and selection states.
- Move file decoding and preview preparation off the UI thread, then marshal
  only current results back to the UI thread.
- Preserve existing profile-mismatch/name-confirmation/review behavior where it
  already applies.

## Non-Goals

- Redesigning the supplied import dialogs.
- Replacing `ScheduleImportService`, reconciliation choices, or transaction
  behavior.
- Adding a page-based import workflow.

## State Model

| State | Visible/Enabled behavior | Allowed transition |
| --- | --- | --- |
| Initial | path picker; Load disabled | choose path |
| Source selected | import kind visible; Load enabled only with valid required selections | Load / choose another path |
| Loading | source controls and Load disabled; progress/status shown | success / failure / cancel |
| Loaded compatible workbook | compatible schedule/user selector visible; Next gated on valid selection | Next / reload / cancel |
| Load failure | source controls restored; diagnostic shown; Next unavailable | choose/retry/cancel |
| Review | existing reconciliation dialog with Back/Import/Cancel | Back / Import / Cancel |

Every asynchronous result carries a request generation/token. A result is
ignored if the dialog closed, the file path changed, or a newer Load request was
started.

## Data Ownership

- `m_scheduleImportFilePath` remains source-dialog input, not evidence that a
  workbook was parsed.
- Add a dialog-local loaded `engine::ScheduleImportWorkbook` (or equivalent
  reader result) only after successful reader completion.
- Populate user/schedule controls from decoded compatible sheets/users only.
- Clear loaded workbook, preview, selections, and diagnostics when the path or
  import kind changes if they no longer correspond to that input.
- Do not persist a reader object or OpenXLSX document in `MainWindow` state.

## Work Breakdown

1. Trace current source/review control creation in
   `MainWindow_schedule_page.cpp`, state declarations in `MainWindow.xaml.h`,
   and event handlers in `MainWindow_schedule_editor.cpp`.
2. Introduce the reader/factory dependency and a test fake without exposing
   OpenXLSX through `MainWindow.xaml.h`.
3. Replace fallback data construction with a background reader request.
4. Populate source dialog controls from the real result and implement the state
   table above, including precise button labels/enabled state.
5. Surface reader diagnostics as concise user messages while retaining detailed
   diagnostic data for logs/tests as project conventions allow.
6. Pass the selected decoded result into the existing preview/reconciliation
   service path.
7. Confirm Back preserves the selected file, kind, decoded workbook, and
   selected schedule; confirm Cancel clears only dialog-local work.
8. Exercise success, no-compatible-schedule, malformed workbook, profile
   mismatch, and rapid reselect/close scenarios using a fake reader and real
   fixtures.

## Acceptance Criteria

- A valid selected workbook drives real schedule/user choices; no normalized
  fallback schedule is presented.
- Load failure leaves the user in the source dialog with a retry path and does
  not enable Next.
- Review contains classes derived from the decoded selected workbook and still
  exposes reconciliation controls.
- Back/Cancel behavior matches the illustrated dialog sequence.
- Slow/failed/stale loads cannot mutate a closed or superseded dialog.
- UI event handlers do not block on workbook parsing.

## Validation

- Unit/component tests with fake reader results cover every state transition.
- Fixture-backed smoke tests cover a regular and intensive workbook from file
  selection through preview construction.
- Manual WinUI smoke test captures the four illustrated modal states: initial,
  path+kind ready, validated schedule selection, and review/reconcile.
- Existing schedule board and import-service tests remain green.

## Exit Criteria

The WinUI dialogs use real native workbook data end to end. The old fallback is
removed from normal import paths, except for a deliberately isolated test fake
where needed.

## Phase 6 Result

Completed on 2026-09-13. The WinUI source dialog now loads
`ScheduleImportWorkbook` through `makeScheduleWorkbookReader()` on a worker
thread and keeps the decoded workbook in dialog-local state. The source state
uses the Qt-shaped sequence: file-only initial state, file-plus-kind ready
state, workbook/worksheet/user selection, and `Next` into the existing
`Review & Reconcile` dialog. The old normalized provider is no longer a
production fallback for file imports.

The load request carries a generation and cooperative cancellation flag.
Results from a cancelled, superseded, or closed dialog are discarded before
they can update controls. Visible worksheet and compatible-user choices are
populated from the native reader result, reader failures remain in the source
dialog with a retry path, and Back retains the decoded selection.

Verification completed:

- Direct MSVC Debug compile/link of the current WinUI sources succeeded using
  a validation engine archive containing the current Phase 4 interpreter.
- The staged executable's `--phase6-schedule-test` diagnostic exited 0.
- `git diff --check` passed.
- The configured CMake/MSBuild route still hits the host's pre-existing
  `Microsoft.Build.Utilities.FileTracker` access-denied failure before source
  compilation; the direct wrapper build was used to validate this phase.

Phase 7 owns removal of the remaining collapsed legacy controls, explicit
profile-mismatch confirmation before review, input limits, and final release
hardening.
