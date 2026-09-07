# Phase 5 interim exit review

Date: 2026-09-08 (Asia/Seoul)

Status: **In progress — the Phase 5 exit gate is not accepted.** The shell,
native database/output boundary, and engine-backed Campus Information slice are
implemented. The remaining acceptance work is interactive paired evidence,
runtime measurements, and a clean rebuild of the x64/x86 WinUI stages.

## Implementation and evidence matrix

| Gate | Current result | Evidence or remaining action |
| --- | --- | --- |
| Shell, startup, menu/sidebar, navigation history, theme/language, and dirty/error state | Implemented | Existing Phase 3/4 semantic and staged evidence remains the baseline; the latest Phase 5 source changes require a fresh staged build before final acceptance. |
| Database create/open/recent and native picker policy | Implemented | `FileOpenPicker`, `FileSavePicker`, and `FolderPicker` are wired through engine/file-system boundaries. Latest WinUI translation units pass strict direct MSVC compilation. Interactive picker/unsaved-change review remains part of the final desktop pass. |
| Campus Information read-only feature | Implemented | Uses `CampusRecordService`, retains list/detail state, renders all record fields, loads optional staged map images through `WindowsResourceProvider`, and localizes the feature catalogs. Focused `CampusRecordService` and `ResourcePackPolicy` tests pass 2/2; TS and generated `.resw` XML validation pass. Fresh visual review is pending. |
| Paired Qt/WinUI scenarios | Tooling ready; evidence pending | `run_phase5_paired_scenarios.ps1` covers startup, no-database, empty, populated, and error. It refuses overwrite and marks absent Qt evidence as `winui-captured-qt-evidence-missing`. Run it on an interactive desktop, validate each sidecar, and obtain owner review. |
| Cold/warm startup and first paint | Tooling ready; evidence pending | `measure_phase5_winui.ps1` records cold/warm launch-to-visible-window timing. Visible-window timing is explicitly a first-paint proxy; it is not the Phase 0 window-constructed or ready checkpoint. |
| First navigation | Tooling ready; proxy only | A requested Phase 5 launch argument records a scenario-ready proxy. Phase 0 leaves the first-navigation cap to be agreed after capture, so no pass/fail claim is made. |
| Resize, memory, and handles | Tooling ready; evidence pending | The helper records resize latency/bounds, working set, private bytes, peak working set, and `GetProcessHandleCount`. It evaluates only the shared 200 MiB working-set target when numeric samples exist. |
| x64 WinUI stage | Blocked in this host | CMake WinUI build reaches the native project but MIDL fails while initializing `Microsoft.Build.Utilities.FileTracker` with `UnauthorizedAccessException` (`E_ACCESSDENIED`). A fresh runnable stage is therefore unavailable for capture. |
| x86 engine lanes | Passed | Focused `ClassMngrEngineCampusRecordServiceTests` passed in x86 Debug and Release. |
| x86 WinUI Debug/Release stages | Blocked in this host | Both full targets reach pinned package/resource generation, then fail at the same MIDL/FileTracker access-denied error. No x86 UI runtime or memory claim is made. |

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
missing runtime data into a pass.

## Commands used for this review

```text
ctest --test-dir build/windows-x64-winui-debug -C Debug -R "ClassMngrEngine(CampusRecordService|ResourcePackPolicy)Tests" --output-on-failure
ctest --test-dir build/windows-x86-winui-debug -C Debug -R ClassMngrEngineCampusRecordServiceTests --output-on-failure
ctest --test-dir build/windows-x86-winui-release -C Release -R ClassMngrEngineCampusRecordServiceTests --output-on-failure
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File scripts/porting/windows/measure_phase5_winui.ps1 -StageDirectory <stage> -Platform x64 -PlanOnly
```

The source scripts also pass PowerShell AST parsing and `git diff --check`.
Runtime capture and measurement commands intentionally were not run without a
fresh executable and a usable interactive desktop.

## Exit decision

Do not mark Phase 5 complete or begin Phase 6. The next owner action is to
resolve the host/build restriction or use a clean Windows desktop runner,
rebuild all required stages, collect the paired captures and measurement JSON,
and review keyboard/Korean IME/focus/DPI/accessibility behavior for the actual
Campus surface before accepting the phase gate.
