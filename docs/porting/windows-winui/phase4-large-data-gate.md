# Phase 4 large-data gate

## Representative workloads

| Surface | Source rows | Visible-region assertion |
| --- | ---: | --- |
| Class list | 10,000 | Realized containers remain below 3x the rows that fit in the viewport. |
| Roster | 5,000 | Visible rows plus an explicit bounded cache and one selected editor are retained. |
| Schedule slots | 2,000 | `ItemsRepeater` realizes the viewport and a bounded cache only. |
| Speaking scores | 10,000 x 8 logical cells | Only visible row/cell presentations are realized; editing one cell cannot materialize the grid. |

## Required capture

For each workload, record the following after initial render, a page-down
sequence, and a selection/edit operation:

- source item count and viewport row count;
- realized container count and live row/cell view-model count;
- frame sample count, 95th-percentile and maximum frame interval, and scoped native allocation sample;
- private working-set delta and reclaimed-memory observation;
- edit persistence after recycling and actual release of row/cell presentations.

Accessibility automation, touch, and high-contrast validation are outside the
current Phase 4 scope. Standard controls retain their platform defaults.

## Executable acceptance contract

The dedicated `--phase4-large-data-test` process uses an isolated, visible
1200-by-800 window and deterministic in-memory fixtures. It does not open the
normal shell or restore/save the owner's window settings. Each invocation
receives a unique run ID and output path; the runner checks both the run ID and
the child process ID before accepting its report. A successful exit without a
complete matching report is a failure.

Each workload records `initial`, `scrolled`, `edited`, and `released`
checkpoints. Scrolling must reach distant source rows, and an edited value must
survive scrolling away and returning. Source records are permitted to scale
with fixture size; visual objects and presentation view models are not.
Counts must describe active realized presentations, include the control's bounded
cache, and measurement must not create offscreen elements. The probe uses
`ListView.ContainerFromIndex` and `ItemsRepeater.TryGetElement` for this
active-realization count; it does not treat an internal recycle-pool object as
realized. Release is checked only after removing the source and visual tree and
observing zero active elements.

The following budgets are declared before collecting results:

| Metric | Budget |
| --- | --- |
| Retained row presentations/containers | Strictly less than 3 times viewport rows |
| Cell presentations | At most retained rows times workload columns |
| Active editors | At most one; zero after release |
| Release | Zero retained row/cell presentations and editors |
| Active frame interval | p95 <= 16.7 ms and maximum <= 100 ms |
| Private committed bytes | At most 64 MiB above the workload's pre-load baseline |
| Tracked native application allocations | At most 16 MiB per workload |

The frame budget carries forward the provisional table-scrolling budget in
[the performance baseline](../windows-direct2d/phase-0/performance-baseline.md).
The memory and allocation caps are initial engineering limits for this probe;
they must not be increased simply to turn a failed capture green.

Native allocation accounting must state its exact coverage. Application-owned
fixture/presentation allocations do not measure WinUI, COM, the process heap,
or GPU allocations. Private bytes and private working set are separate process
measurements, not allocation counters. This C++ application has no managed
allocation metric. A WPR/WPA or native-profiler trace is required for any claim
about total framework allocation traffic. Rendering callback intervals measure
UI frame cadence, not GPU presentation latency or input latency.

Record private working set before load and after release without forcing a
working-set trim. Windows may retain freed heap pages, so memory need not
return exactly to the starting value; bounded private growth and released
presentation objects are the acceptance checks.

The performance gate requires three successful Windows x64 Release runs with
the same executable and fixtures in a quiet interactive desktop session.
Debug and x86 runs provide functional/build evidence separately. Preserve
failed runs, raw per-checkpoint measurements, executable hash, source revision,
configuration, and host information. Missing, skipped, stale, malformed, or
over-budget evidence cannot produce a full Phase 4 pass.

## Running the gate

Build and capture from the repository root in a real desktop session:

```powershell
cmake --build --preset windows-x64-winui-release
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/porting/windows/run_phase4_large_data.ps1 `
  -Executable dist/ClassMngr-windows-winui-x64/Release/ClassMngrWinUI.exe `
  -OutputDirectory artifacts/phase4/large-data-release-run-001 `
  -ExpectedArchitecture x64 -ExpectedConfiguration Release -Repetitions 3
```

Use a new output directory for each capture. The runner preserves every raw
report and writes `phase4-large-data-summary.json`, including failures. Keep
the desktop unlocked and the diagnostic window visible throughout the run.

The validator's deterministic rejection tests run without opening a window:

```powershell
ctest --test-dir build/windows-x64-winui-debug -C Debug `
  -R ClassMngrWindowsWinUILargeDataArtifactTests --output-on-failure
```

`collect_phase4_winui_evidence.ps1` includes this large-data run as a required
check. The separate small-gallery semantic and idle-memory checks remain
useful, but neither substitutes for the representative workload capture.

The control fails the gate if realized visual or view-model counts track the
total data size. The implementation must use the first-party primitive selected
in `phase4-virtualization-decision.md`; a performance failure is not an
implicit approval for an external grid.
