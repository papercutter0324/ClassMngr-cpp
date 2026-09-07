# Phase 5 interim exit review

Date: 2026-09-08 (Asia/Seoul)

Status: **In progress — the Phase 5 exit gate is not accepted.** The shell,
native database/output boundary, and engine-backed Campus Information slice are
implemented. Host-level x64/x86 Debug/Release builds, staged verifiers,
interactive WinUI captures, and x64 runtime measurements are recorded. The
shared catalog bridge now preserves the WinUI-only Campus Information context
and fails generation early if its required fallback keys are removed. The
remaining acceptance work is complete paired Qt coverage or an approved
exception, owner review, and the final picker/unsaved-change, keyboard/Korean
IME, focus, DPI, and accessibility review.

## Implementation and evidence matrix

| Gate | Current result | Evidence or remaining action |
| --- | --- | --- |
| Shell, startup, menu/sidebar, navigation history, theme, language, and dirty/error state | Implemented and staged-verified | Existing Phase 3/4 semantic and staged evidence remains the baseline. The latest Phase 5 source is included in the four fresh x64/x86 Debug/Release staged verifier passes. |
| Database create/open/recent and native picker policy | Implemented | `FileOpenPicker`, `FileSavePicker`, and `FolderPicker` are wired through engine/file-system boundaries. Latest WinUI translation units pass strict direct MSVC compilation. Interactive picker/unsaved-change review remains part of the final desktop pass. |
| Campus Information read-only feature | Implemented and captured | Uses `CampusRecordService`, retains list/detail state, renders all record fields, loads optional staged map images through `WindowsResourceProvider`, and localizes the feature catalogs. The shared `.ts` to `.resw` bridge requires the `CampusInformationPage` fallback keys so a catalog refresh cannot silently remove the feature resources. Focused engine tests, staged Phase 5 checks, and five WinUI scenario sidecars pass. Owner visual review remains. |
| Paired Qt/WinUI scenarios | WinUI captured; Qt coverage partial | The [paired-scenario manifest](../../../artifacts/phase5/paired-20260908-x64-debug-clean2/phase5-paired-scenarios.json) covers startup, no-database, empty, populated, and error. All five WinUI sidecars validate; the startup and no-database records link to matching Qt evidence. The empty, populated, and error records explicitly report missing matching Qt fixtures. Obtain owner review and either add those Qt fixtures or record an approved exception. |
| Cold/warm startup and first paint | Captured for x64 Debug/Release | The [Debug report](../../../artifacts/phase5/measurements-20260908-x64/phase5-measurement-x64-debug.json) and [Release report](../../../artifacts/phase5/measurements-20260908-x64/phase5-measurement-x64-release.json) contain three successful iterations each. Visible-window timing is explicitly a first-paint proxy; it is not the Phase 0 window-constructed or ready checkpoint. |
| First navigation | Tooling-ready proxy | The requested Campus launch argument records a scenario-ready proxy. Phase 0 leaves the first-navigation cap to be agreed after representative capture, so no pass/fail claim is made. |
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

ctest --test-dir build/windows-x64-winui-debug -C Debug -R "ClassMngrEngine(CampusRecordService|ResourcePackPolicy)Tests" --output-on-failure
ctest --test-dir build/windows-x86-winui-debug -C Debug -R ClassMngrEngineCampusRecordServiceTests --output-on-failure
ctest --test-dir build/windows-x86-winui-release -C Release -R ClassMngrEngineCampusRecordServiceTests --output-on-failure
```

The four host-level builds and four staged verifiers passed against the guarded
catalog generation. The paired
manifest validator reported five valid WinUI metadata sidecars, and the two
measurement reports each contain three successful iterations with clean
process/window release. The source scripts pass PowerShell AST parsing and
`git diff --check`. A restricted-sandbox build still fails during MIDL's
static `Microsoft.Build.Utilities.FileTracker` initialization; the workaround
and required host permissions are recorded in
[`00-START-HERE.md`](../../../plans/windows-winui3-port-plan/00-START-HERE.md).

## Exit decision

Do not mark Phase 5 complete or begin Phase 6. The implementation and runtime
work are sufficiently recorded for owner review, but the exit gate still needs
the missing paired Qt fixtures (or an explicit approved exception), review of
the five captured WinUI scenarios, and interactive verification of picker and
unsaved-change behavior, keyboard/Korean IME, focus, DPI, and accessibility.
