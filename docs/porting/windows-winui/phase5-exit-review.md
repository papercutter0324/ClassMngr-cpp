# Phase 5 interim exit review

Date: 2026-09-08 (Asia/Seoul)

Status: **In progress — the Phase 5 exit gate is not accepted.** The shell,
native database/output boundary, and engine-backed Campus Information slice are
implemented. Host-level x64/x86 Debug/Release builds, staged verifiers,
interactive WinUI captures, and x64 runtime measurements are recorded. The
shared catalog bridge now preserves the WinUI-only Campus Information context
and fails generation early if its required fallback keys are removed. The
owner decisions recorded on 2026-09-08 approve the five captured WinUI
scenarios, approve the interactive desktop review, and approve an explicit
Phase 5 exception for the missing Qt empty/populated/error fixtures. The
remaining gate work is the Phase 5-to-6 table-parity handoff; neither approval
waives the requirement for
Qt-derived visual parity in Phase 6.

## Implementation and evidence matrix

| Gate | Current result | Evidence or remaining action |
| --- | --- | --- |
| Shell, startup, menu/sidebar, navigation history, theme, language, and dirty/error state | Implemented and staged-verified | Existing Phase 3/4 semantic and staged evidence remains the baseline. The latest Phase 5 source is included in the four fresh x64/x86 Debug/Release staged verifier passes. |
| Database create/open/recent and native picker policy | Implemented; interactive review approved | `FileOpenPicker`, `FileSavePicker`, and `FolderPicker` are wired through engine/file-system boundaries. Latest WinUI translation units pass strict direct MSVC compilation. The owner-approved picker and unsaved-change review is recorded below. |
| Campus Information read-only feature | Implemented and captured; owner review approved | Uses `CampusRecordService`, retains list/detail state, renders all record fields, loads optional staged map images through `WindowsResourceProvider`, and localizes the feature catalogs. The shared `.ts` to `.resw` bridge requires the `CampusInformationPage` fallback keys so a catalog refresh cannot silently remove the feature resources. Focused engine tests, staged Phase 5 checks, and five WinUI scenario sidecars pass. The capture review is approved as Phase 5 evidence; the current list/detail geometry is still subject to the Qt-derived handoff in [phase5-table-parity-handoff.md](phase5-table-parity-handoff.md). |
| Paired Qt/WinUI scenarios | WinUI captured; approved Qt-evidence exception | The [paired-scenario manifest](../../../artifacts/phase5/paired-20260908-x64-debug-clean2/phase5-paired-scenarios.json) covers startup, no-database, empty, populated, and error. All five WinUI sidecars validate; startup and no-database link to matching Qt evidence. The owner-approved exception in this review accepts the missing matching Qt fixtures for empty, populated, and error for the Phase 5 gate only. It does not waive Phase 6 table/list-detail parity or require treating a missing Qt artifact as a visual match. |
| Cold/warm startup and first paint | Captured for x64 Debug/Release | The [Debug report](../../../artifacts/phase5/measurements-20260908-x64/phase5-measurement-x64-debug.json) and [Release report](../../../artifacts/phase5/measurements-20260908-x64/phase5-measurement-x64-release.json) contain three successful iterations each. Visible-window timing is explicitly a first-paint proxy; it is not the Phase 0 window-constructed or ready checkpoint. |
| First navigation | Measured; provisional guardrails passed | The [x64 Release first-navigation report](../../../artifacts/phase5/measurements-20260908-x64/phase5-first-navigation-x64-release.json) contains 20 independent fresh-process samples, 20/20 valid, and 0 failures. The 19th sorted sample is p95 `52.881 ms`; minimum is `38.2 ms`, median `44 ms`, and maximum `63.1 ms`. Every raw sample reaches the populated Campus page with the selected detail panel and expected record count, then exits cleanly. The provisional p95 `<= 500 ms` and maximum `<= 750 ms` guardrails pass. The final Phase 0 cap remains the same-definition Qt p95 plus 20% once a Qt baseline exists. |
| Resize, memory, and handles | Captured for x64 Debug/Release | Debug: cold visible 207 ms, warm visible 184-190 ms, working set 126.8-127.8 MiB, private bytes 109.2-110.6 MiB, and resize latency 0.1-11.9 ms. Release: cold visible 207 ms, warm visible 184-192 ms, working set 124.4-125.2 MiB, private bytes 107.1-108.6 MiB, and resize latency 0.1-6.2 ms. All iterations closed cleanly; the evaluated 200 MiB steady-state working-set target passes. |
| x64 WinUI stage | Passed on host-level build | Host-level x64 Debug and Release targets build successfully, stage campus resources correctly, and pass the complete Phase 1-5 staged verifier. The restricted local sandbox still reproduces the documented MIDL/FileTracker `E_ACCESSDENIED` failure. |
| x86 engine lanes | Passed | Focused `ClassMngrEngineCampusRecordServiceTests` passed in x86 Debug and Release. |
| x86 WinUI Debug/Release stages | Passed on host-level build | Both host-level targets build successfully and their complete Phase 1-5 staged verifiers pass. No x86 runtime or memory budget claim is made because the recorded runtime measurement set is x64 only. |

## Relevant Phase 0 comparisons

The frozen Phase 0 performance record defines provisional x64 budgets of
2,200 ms to window construction, 4,500 ms to ready, 900 MiB ready working set,
800 MiB ready private bytes, and p95 resize latency of 32 ms. First-navigation
and output caps are intentionally decided after representative capture. The
Phase 5 helper reports comparable process/window observations but does not
pretend that a visible-window event is the same as the Qt ready checkpoint, and
does not assert budgets that it cannot measure or that Phase 0 did not freeze.

The separate bootstrap target is a 200 MiB steady-state working-set limit. The
helper evaluates that limit only when it has numeric samples; it does not turn
missing runtime data into a pass. The recorded x64 samples are below that
target.

## Commands used for this review

```text
cmake --build build/windows-x64-winui-debug --config Debug --target ClassMngrWindowsWinUI --parallel 2
cmake --build build/windows-x64-winui-release --config Release --target ClassMngrWindowsWinUI --parallel 2
cmake --build build/windows-x86-winui-debug --config Debug --target ClassMngrWindowsWinUI --parallel 2
cmake --build build/windows-x86-winui-release --config Release --target ClassMngrWindowsWinUI --parallel 2

powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File scripts/verify_windows_winui_stage.ps1 -StageDirectory <stage> -Platform <x64-or-Win32>
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File scripts/porting/windows/run_phase5_paired_scenarios.ps1 -Executable <x64-debug-executable> -OutputDirectory <paired-output> -QtArtifactRoot artifacts/phase0/windows-qt-visual
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File scripts/porting/windows/validate_winui_scenario_artifacts.ps1 -ArtifactRoot artifacts/phase5/paired-20260908-x64-debug-clean2 -RequirePassed
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File scripts/porting/windows/measure_phase5_winui.ps1 -StageDirectory <x64-stage> -Platform x64 -Iterations 3 -ScenarioArguments '--phase5-campus-populated' -SettleMilliseconds 1000
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File scripts/porting/windows/measure_phase5_winui.ps1 -StageDirectory <x64-release-stage> -Platform x64 -FirstNavigation -Iterations 20 -ReportPath artifacts/phase5/measurements-20260908-x64/phase5-first-navigation-x64-release.json

ctest --test-dir build/windows-x64-winui-debug -C Debug -R "ClassMngrEngine(CampusRecordService|ResourcePackPolicy)Tests" --output-on-failure
ctest --test-dir build/windows-x86-winui-debug -C Debug -R ClassMngrEngineCampusRecordServiceTests --output-on-failure
ctest --test-dir build/windows-x86-winui-release -C Release -R ClassMngrEngineCampusRecordServiceTests --output-on-failure
```

The four host-level builds and four staged verifiers passed against the guarded
catalog generation. The paired
manifest validator reported five valid WinUI metadata sidecars, and the two
measurement reports each contain three successful iterations with clean
process/window release. The dedicated x64 Release first-navigation report
contains 20/20 valid samples with p95 `52.881 ms` and maximum `63.1 ms`; its
20 raw application results are retained alongside the report. The source
scripts pass PowerShell AST parsing and `git diff --check`. A
restricted-sandbox build still fails during MIDL's
static `Microsoft.Build.Utilities.FileTracker` initialization; the workaround
and required host permissions are recorded in
[`00-START-HERE.md`](../../../plans/windows-winui3-port-plan/00-START-HERE.md).

## Owner decisions recorded 2026-09-08

The owner-approved decisions for the remaining Phase 5 criteria are:

1. The current paired-evidence state is approved through an explicit exception:
   the missing Qt empty, populated, and error fixtures are accepted for the
   Phase 5 gate. Startup and no-database still retain their matching Qt links.
2. The five captured WinUI scenarios in the paired manifest are approved for
   the recorded Phase 5 evidence review.
3. Interactive review is approved for native pickers, unsaved-change behavior,
   keyboard and Korean IME behavior, focus restoration, DPI, and accessibility.

These are owner decisions, not claims that the missing Qt files exist or that
the current Campus list/detail prototype already matches the retained Qt
layout.

## First-navigation recommendation

The Terra High recommendation is to use the following repeatable checkpoint:

- Gate configuration: x64 Release only; Debug and x86 are diagnostic lanes.
- Start from a rendered Home page in a fresh process with a deterministic,
  populated in-memory Campus fixture already prepared and no Campus page in
  the `Frame` cache.
- Start the timer immediately before `Frame.Navigate` to Campus Information.
- Stop at the first `CompositionTarget::Rendering` callback after navigation
  has set the current page, synchronously populated the Campus list and
  selected detail panel, and exposed the expected record count.
- Run 20 independent samples. Sort them and use sample 19 as p95. Any timeout,
  navigation failure, semantic-readiness mismatch, failed close, or short
  sample set fails the measurement; retain raw samples and failures.
- Use p95 `<= 500 ms` and maximum `<= 750 ms` as the provisional Phase 5
  guardrails. The optional map image is deferred work and is not part of the
  readiness checkpoint.

The current `scenarioReadyProxyMs` values are visible-window observations and
must not be substituted for this navigation-ready measurement. The final cap
still needs a directly comparable Qt first-navigation capture and the Phase 0
`Qt p95 + 20%` rule.

The 2026-09-08 measurement passed the provisional Phase 5 guardrails: all 20
requested samples were valid, the nearest-rank p95 was `52.881 ms`, and the
maximum was `63.1 ms`. The raw sample files are retained in the
`phase5-first-navigation-x64-release.json.first-navigation-raw` directory next
to the report.

## Table-parity handoff

The required handoff is recorded in
[phase5-table-parity-handoff.md](phase5-table-parity-handoff.md). It documents
the Qt Campus selector/tab/form geometry and the retained Qt table-family
contracts for roster, speaking evaluation, schedule, staff directory, and
analytics ranking. It also records the current WinUI Campus list/detail values
as prototype-only values. Review and reconcile that handoff before Phase 6
begins; the current Campus surface is not a parity-accepted table/list-detail
row.

## Exit decision

Do not mark Phase 5 complete or begin Phase 6 yet. Criteria 1–3 are approved,
and the first-navigation measurement now passes the provisional guardrails.
The Qt-derived table-parity handoff must still be reviewed and reconciled with
the existing Campus list/detail prototype before Phase 6 can start.
