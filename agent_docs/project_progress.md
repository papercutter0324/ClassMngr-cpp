# Project Progress

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

Phase 6 is implemented in the WinUI dialog path and is ready for its separate
commit. `loadScheduleImportSource()` now calls the native reader factory on a
worker thread, stores the returned `ScheduleImportWorkbook` in dialog-local
state, and populates visible worksheet and compatible-user selectors from the
decoded workbook. The source dialog follows the Qt sequence from file-only,
through schedule-kind selection and workbook validation, to `Next` and the
existing `Review & Reconcile` dialog. The synthetic normalized provider is no
longer reachable from real file import.

Generation tokens and cooperative cancellation discard stale or closed-dialog
results. The staged x64 Debug WinUI executable built and the
`--phase6-schedule-test` diagnostic exited 0. The configured CMake/MSBuild
route remains blocked before source compilation by the host's existing
FileTracker access-denied failure; direct MSVC/wrapper validation used a
generated validation engine archive containing the current interpreter. Phase
7 should remove the remaining collapsed legacy controls, enforce explicit
profile-mismatch confirmation, add reader limits, and complete cutover
verification. Preserve the unrelated modification to
`tests/fixtures/database-port/typical.tps`.

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
