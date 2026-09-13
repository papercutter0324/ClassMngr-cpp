# Project Progress

## Latest Heavy Deployment — Schedule Import WinUI Rebuild — 2026-09-14

Deployment `schedule_import_winui_rebuild_20260914` replaced the failed WinUI
schedule-import interaction with a new source-selection flow and a separate
`Review & Reconcile` flow. The implementation keeps the native OpenXLSX
reader, shared schedule-board renderer, and engine import service as the
behavioral contracts, while using the Qt implementation only as guidance.

The source dialog now supports XLSX picking, Regular/Intensives validation,
single-sheet teacher selection, multi-sheet worksheet-then-teacher selection,
explicit `Select a name...` state, cancellation/reset, titlebar drag, and
horizontal resizing. The review dialog uses a Pivot with scrollable Classes,
Korean Teachers, and conditionally displayed `Unrecognized` panes; shared
schedule preview and class-color picking are retained; Import is gated by
complete resolutions and opens a readable confirmation list.

Verified handoff: x64 Debug WinUI build succeeded with zero errors, staged
`--phase6-schedule-test` passed, focused schedule reader/import tests passed
4/4, the final presentation assertions passed 14/14, and `git diff --check`
passed. No launchable native UI was available in the verification host, so
live visual spacing, picker behavior, drag/resize, and focus restoration remain
manual follow-up when a native UI host is available. Existing unrelated dirty
worktree changes were preserved; no commit was requested.

## Latest Completed Deployment

Deployment `winui_parity_pass_20260912` completed the dependency-ordered
Medium-route WinUI parity pass for the shell, My Information, Schedule and
Testing, Classes, and Speaking Analytics surfaces. Existing engine, service,
and persistence contracts were preserved.

The pass adds shared unsaved-navigation continuation, debounced My Information
autosave and preview fallbacks, persisted schedule hover customization,
testing-class workflow/navigation, responsive class analytics, and an actual
Speaking Analytics year-to-date line chart. Phase-6 diagnostics were expanded
to cover the new behavior.

## Verified Handoff

- Full x64 Debug CTest: 60/60 passed.
- Engine/feature subset: 18/18 passed.
- x64 WinUI Debug and Release builds and stage verification passed.
- x86/Win32 WinUI Release build and stage verification passed.
- All listed phase-3, phase-4, phase-5, and phase-6 WinUI diagnostics passed.
- The native scenario runner produced and validated
  `artifacts/phase6/winui-parity-x64-debug-final2/phase6-winui-shell-final2.png`
  and its
  metadata sidecar with process exit code 0 and no forced termination.

The local Qt reference pictures remain available under
`artifacts/phase0/windows-qt-visual`; no reattachment was required. The
current host clamps the native capture to 800x600 while the reference set is
1270x1040 at 150%, so a same-dimension pixel diff was not claimed or run.

## Active Deployment Handoff

Deployment `schedule_import_openxlsx_phase1_20260913` began Phase 1 of the
OpenXLSX schedule-workbook adapter plan. The local source at
`C:\Git\openxlsx` was verified against Codeberg commit
`ece329af84b370a8b77f5a3ee0e30509ad0f0bf9` and materialized as the pinned
submodule `third_party/openxlsx/source`.

Phase 1 also recorded the OpenXLSX, PugiXML 1.14, miniz 3.0.2, and standalone
nowide v11.3.1 provenance/license evidence under `third_party/openxlsx/` and
`licenses/openxlsx/`. The product-safe policy disables OpenXLSX samples,
documentation, tests, benchmarks, automatic fetching, and network fallback.

The no-network MSVC configuration probe detected the compiler and then stopped
because the local `nowide` target is not provisioned. No dependency was
downloaded. The remaining offline-build gate is intentionally handed to Phase
2, which owns native dependency target provisioning and the real WinUI/MSBuild
integration.

### Phase 2 handoff

Phase 1 is committed as `f960ab83`. Phase 2 added pinned PugiXML, miniz, and
standalone-nowide submodules and a controlled CMake-to-MSBuild bridge. The
generated WinUI property sheet carries configuration-specific static library
inputs and repository-local include paths into `ClassMngrWinUI.vcxproj`.

The native OpenXLSX smoke executable passed in x64 Debug, x64 Release, and
Win32 Release. The full WinUI target was attempted but the host's existing
MSBuild FileTracker access failure stopped the unrelated engine compilation
after the OpenXLSX dependency chain had built. Use the documented
`/p:TrackFileAccess=false` override for native target validation.

Phase 2 is committed as `ed80300d`. Preserve the unrelated modification to
`tests/fixtures/database-port/typical.tps`.

### Phase 5 handoff

Phase 5 adds `ScheduleWorkbookOpenXLSXReader` and a runtime-generated,
copyright-safe `.xlsx` fixture catalogue. The reader uses OpenXLSX for
worksheet traversal, PugiXML for raw OOXML style/theme/indexed-color and note
facts, and the shared Phase 4 interpreter for schedule semantics. It preserves
sheet visibility, existing cells, merges, Korean UTF-8 content, normal and
intensive schedules, and stable errors for cancellation, missing, malformed,
corrupt, and unsupported inputs. The reader test also verifies the imported
file's size and timestamp are unchanged.

Direct MSVC Debug compile/link/run of the reader test passed. The configured
WinUI/MSBuild test target was generated but remains blocked before source
compilation by the host's existing `Microsoft.Build.Utilities.FileTracker`
access-denied failure. Phase 5 is committed; Phase 6
should wire this factory into the existing dialog-owned workflow and remove
the synthetic provider fallback. Preserve the unrelated fixture modification.

### Phase 6 handoff

Phase 6 is committed as `ea03663d`. `loadScheduleImportSource()` calls the
native reader factory on a worker thread, stores the returned
`ScheduleImportWorkbook` in dialog-local state, and populates visible
worksheet and compatible-user selectors from the decoded workbook. The source
dialog follows the Qt sequence from file-only, through schedule-kind selection
and workbook validation, to `Next` and the existing `Review & Reconcile`
dialog. The synthetic normalized provider is no longer reachable from real
file import.

Generation tokens and cooperative cancellation discard stale or closed-dialog
results. The staged x64 Debug WinUI executable built and the
`--phase6-schedule-test` diagnostic exited 0. Phase 7 hardens this path and
preserves the unrelated modification to `tests/fixtures/database-port/typical.tps`.

### Phase 7 handoff

Phase 7 removes the remaining collapsed normalized import controls and the
unused legacy schedule-import state. Profile-name mismatch is an explicit
confirmation dialog before review; changing the loaded source, worksheet, or
user clears the confirmation. The reader enforces bounded workbook/XML,
worksheet, cell, merge, style, notes, and text limits before or during model
materialization and converts limit failures to the stable invalid-format
reader error.

The native reader target built and its runtime fixture test exited 0,
including Unicode/regular/intensive/cancellation/malformed/corrupt cases and
the oversized-workbook check. The full elevated x64 Debug WinUI wrapper build
and staging succeeded with 0 errors, and staged `--phase6-schedule-test`
exited 0. The reader translation unit is now self-contained for native tests;
the WinUI project explicitly excludes it from the application PCH.

At the time of the Phase 7 handoff, the host's non-elevated CMake/MSBuild route
still reported the MSBuild `FileTracker` access-denied initializer failure;
that follow-up is recorded below. No cross-machine performance claim was made
without a representative benchmark.
The Phase 7 completion is recorded in this handoff. Preserve the unrelated
modification to `tests/fixtures/database-port/typical.tps`.

### Build-error follow-up

The non-elevated CMake Tools build failure is fixed. Visual Studio 2026's C++
tracked tasks can initialize `Microsoft.Build.Utilities.FileTracker` even when
`TrackFileAccess=false` unless their per-item `MinimalRebuildFromTracking`
metadata is also disabled. `CMakeLists.txt` now defaults
`CLASSMNGR_ENABLE_MSVC_FILE_TRACKING` to `OFF`, applies the setting before
`project()` and to generated Visual Studio globals, and imports
`cmake/msvc_file_tracking_compat.props`. The hand-authored WinUI project uses
the same compatibility import, including a target-time override for the
generated manifest resource item whose metadata is otherwise reset by the
Visual Studio targets.

After reconfiguration, the ordinary non-elevated command
`cmake --build build/windows-x64-winui-debug --config Debug --target
ClassMngrWindowsWinUI -- /m:1` passed both as a full rebuild and as an
incremental rebuild with 0 errors. Existing OpenXLSX conversion warnings,
CMake deprecation warnings, and the offline NuGet vulnerability lookup warning
remain non-blocking. File tracking can be opted back in with
`CLASSMNGR_ENABLE_MSVC_FILE_TRACKING=ON` on hosts that support it.

### Phase 4 handoff

Phase 4 is implemented: `ScheduleWorkbookInterpreter` is now the single
Qt-free semantic path for raw workbook layouts. The retained Qt parser is a
decode/layout-conversion/result-conversion adapter, so its existing public API
and Qt UI model remain stable while the same logic is available to OpenXLSX.
The interpreter covers merged cells, visible sheets, weekday aliases,
regular/intensive time rules, UTF-8 Hangul extraction, course/room parsing,
style colors, intensive slot states, occurrence partitioning, and diagnostics.

The focused interpreter test was compiled and linked directly with MSVC and
passed. The CMake/MSBuild target still stops before compilation on the host's
existing FileTracker access-denied failure. Phase 5 should implement the
OpenXLSX reader against this interpreter and retain the unrelated fixture
modification.

### Phase 3 handoff

Phase 3 adds the Qt-free `ScheduleWorkbookLayout` value model and
`ScheduleWorkbookReader` substitution contract under the native engine include
boundary. The contract returns the existing engine import workbook and result
types, carries cooperative cancellation, and keeps all codec/UI types out of
the public surface. A fake-reader contract test covers replacement,
cancellation, worksheet visibility, cells, styles, and merges.

The MSVC contract-test build was attempted after reconfiguration, but the
host's existing `Microsoft.Build.Utilities.FileTracker` access-denied failure
stopped `ClassMngrEngine` compilation before the test source was compiled.
Phase 4 should implement the shared Qt-free schedule interpreter against this
layout and preserve the unrelated fixture modification.

## Previous Deployment Handoff

Deployment `schedule_import_qt_workflow_audit_20260913` examined the retained
Qt schedule-import workflow and implemented its two-stage modal shape in the
WinUI presentation layer. Import no longer navigates to an Import page: the
source state owns file/type/worksheet/user selection, and `Next` changes the
same modal to a `Review & Reconcile` state with preview, Classes, Korean
Teachers, live resolution validation, and `Back`/`Import`/`Cancel` footer
actions.

The implementation preserves the engine-owned preview, resolution, validation,
and atomic apply contracts. Intermediate ContentDialog actions cancel the
default close behavior so Load, Next, and Back keep the workflow open while
changing state; successful apply refreshes the schedule and closes the modal.

The Qt workbook parser remains an intentionally retained adapter. The current
WinUI source load validates the selected `.xlsx` path/readability and feeds the
staged normalized provider into the dialog; it does not yet decode OOXML
workbook contents. A native or bridged WinUI workbook adapter is the explicit
follow-on required for full real-workbook parity.

## Previous Goal

Define the WinUI schedule-import dialog state machine from the retained Qt
workflow: asynchronous workbook load and selection, user/profile resolution,
review tabs and conflicts, confirmation, success/error handling, and refresh
without page navigation.

## Previous Overall Progress

The Qt workflow is mapped to dialog-owned WinUI controls and focused
diagnostics. The source starts at `Choose a file and schedule type.`, enables
type selection after Browse, exposes worksheet/user selection only after the
source is loaded, and changes Load to Next. Review builds a read-only schedule
board plus dynamic class/teacher resolution cards while keeping errors in the
modal. Existing normalized diagnostics continue to exercise the engine-backed
preview/apply path.

The presentation state machine is complete for the staged provider. Workbook
decoding is intentionally not duplicated in the Qt-free engine or silently
ported into WinUI.

## Previous Next Milestone

Implement and integrate the explicit WinUI workbook adapter (or a supported
bridge to the retained Qt reader), then add real multi-sheet/user mismatch and
confirmation coverage against workbook fixtures. Keep XLSX decoding out of the
Qt-free engine and do not broaden this work into Phase-7 adapters.

Verified for this deployment: x64 Debug WinUI build/link, staged
`--phase6-schedule-test`, focused `ClassMngrEngineScheduleImportServiceTests`
(1/1), and `git diff --check`.

## Current Deployment Handoff

The WinUI `Review & Reconcile` state requests a 1240x780 review surface while
the initial file-selection state remains compact at 420 pixels wide. The
review transition explicitly enables `ContentDialog.FullSizeDesired`, avoiding
the default content-sized inner surface; the source/reset transition disables
it again. The review root is a stretchable Grid whose empty/collapsed rows are
`Auto` and whose preview/resolution host row is the only `Star` row, so the
available vertical space is owned by the actual review content.

Because an owned `ContentDialog` has no native non-client frame, the review
uses a transparent top drag surface and `RenderTransform` movement, plus
transparent edge/corner resize zones with pointer capture. Pointer positions
use the popup-local/null frame rather than an element outside the dialog's
visual tree. Width and height remain bounded at 520--1800 and 480--1080 DIPs
and clamped to the host viewport. Source, Back, reset, and cancel paths hide
the zones, release captures, clear the transform, and restore the compact
layout.

The preview column is 540 DIPs wide and uses the same schedule report/board
renderer as the schedule page. The body heading now contains the Qt review
description below the single `Review & Reconcile` ContentDialog title. Match
explanations and the `Color` label use the smaller review text size.

Verified for this repair: x64 Debug WinUI build/link/staging with 0 errors,
staged `--phase6-schedule-test` (exit 0), focused static checks, and
`git diff --check`. The existing OpenXLSX conversion warnings and offline
NuGet warning remain unchanged. The CUA surface inventory exposed no
launchable native app, so live visual click-drag/resize behavior remains the
explicit next manual check. The unrelated
`tests/fixtures/database-port/typical.tps` modification remains unstaged.

The import reset path now recomputes source-control state after cancelling a
workbook load. This re-enables Browse and restores the initial source step when
the dialog is opened again, while request-generation cancellation still
prevents stale background results from repopulating the new session. Phase 6
now simulates the disabled-loading state before reset and verifies Browse is
interactive afterward.

Verified for this reset follow-up: elevated x64 Debug WinUI build/link/staging,
staged `--phase6-schedule-test` (exit 0), and `git diff --check`. The existing
OpenXLSX conversion warnings remain unchanged. The unrelated
`tests/fixtures/database-port/typical.tps` modification remains unstaged.

Current goal: preserve Qt-aligned schedule-import behavior while making the
WinUI review dialog readable, resizable through standard edge/corner dragging,
and independently scrollable only within its resolution tabs.

Next milestone: perform a manual visual check at supported window sizes when a
native UI session is available; keep native workbook and dialog state
contracts unchanged.
