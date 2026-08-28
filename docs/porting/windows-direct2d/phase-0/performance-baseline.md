# Phase 0 performance baseline and budgets

The port compares native Windows performance to the current Qt Windows product
on the same fixture and hardware class. The startup values below are
provisional Windows x64 budgets from three historical Qt **Release** GUI runs
recorded in
[baseline-results.md](baseline-results.md): the slowest observed value plus 20%
headroom, rounded up. ARM64 must establish an equivalent baseline before the
native target can advance beyond a smoke shell.

The current-branch Qt **Debug** probe was also rebuilt and sampled three times
at revision `b34a357`; its results are recorded in the current-probe section of
[baseline-results.md](baseline-results.md). It is diagnostic evidence only and
does not replace the Release budget.

| Metric | Scenario | Provisional x64/ARM64 budget | Current probe |
| --- | --- | --- | --- |
| Startup to window constructed | empty settings profile | <= 2,200 ms | `--startup-performance-test` |
| Startup to ready | empty settings profile | <= 4,500 ms | `--startup-performance-test` |
| Ready working set | empty settings profile | <= 900 MiB | startup report on Windows |
| Ready private bytes | empty settings profile | <= 800 MiB | startup report on Windows |
| First navigation | first open of lazy Calendar, Classes, Campus, PDF | budget = Qt p95 + 20%, then cap agreed after capture | page lifecycle/timing diagnostics |
| Resize latency | representative populated window, 60 resize samples | p95 <= 32 ms | Phase 3 native frame telemetry |
| Table scrolling | large roster and speaking-evaluation fixture | p95 frame <= 16.7 ms; no input stall > 100 ms | Phase 4 native frame telemetry |
| Device recovery | forced device loss and WARP fallback | visible recovery <= 2 s; no data loss | Phase 3 native diagnostics |
| PDF/report output | Korean/English representative fixture | Qt p95 + 20%, then cap agreed after capture | output timer + checksums |

## Repeatable Qt baseline

The existing `ClassMngrStartupPerformanceTests` invokes the app with an empty
settings root and writes `classmngr-scenario-report-v1`. It records process
start-to-window construction, start-to-ready, and Windows working-set/private
bytes at both checkpoints. Run it three times per architecture in a quiet
machine state, retaining the report and host metadata.

```powershell
ctest --test-dir build/windows-x64-debug -C Debug -R StartupPerformance --output-on-failure
```

To retain raw JSON outside CTest, use the Phase 0 collector. The Qt build needs
its Qt `bin` directory because it is not deployed beside the debug executable:

```powershell
.\scripts\porting\windows\collect_phase0_startup_baseline.ps1 `
  -AppPath .\build\windows-x64-debug\Debug\ClassMngr.exe `
  -BuildConfiguration Debug `
  -RuntimeDirectory C:\Qt\6.11.1\msvc2022_64\bin `
  -OutputDirectory .\artifacts\phase0\windows-x64-debug\run-001
```

`CLASSMNGR_STARTUP_WINDOW_MAX_MS`, `CLASSMNGR_STARTUP_READY_MAX_MS`, and
`CLASSMNGR_STARTUP_PROCESS_MAX_MS` already let CI enforce approved timing
limits. Do not set them from one run; set them after the approved three-run
baseline and record the chosen values here.

For memory and page/asset scenarios, follow
[`docs/memory-profiling-windows.md`](../../../memory-profiling-windows.md):
capture at least three runs, export the memory monitor JSON, and compare
steady-state/post-release private usage rather than a peak alone.

## Required baseline report fields

Every reported metric includes source revision, app build configuration, Qt
version, Windows edition/build, architecture, CPU/GPU/driver, display scale,
fixture SHA-256, scenario steps, sample count, median, p95, maximum, and raw
artifact paths. No student data or absolute user paths may appear in committed
reports.
