# Windows Direct2D/DirectComposition port — current status

Last recorded: 2026-08-28 (Asia/Seoul)

This is the cross-device handoff for the Windows port. Phase 0 is still in
progress; its exit gate has not been claimed.

## Repository snapshot

- Branch: `NativeWindowsPort`, at `origin/NativeWindowsPort`.
- HEAD: `fb4f268` (`Phase 0 - Prepare automation to capture additional information`).
- The current product source still has no native Direct2D/DirectComposition
  target; this status file and the capture artifacts are Phase 0 evidence.
- The current Phase 0 release gate is x64 only. ARM64 build compatibility is
  unofficial and informational; ARM64 runtime validation, baselines, and
  release support are deferred. UI Automation, Narrator, and high-contrast
  support are also deferred from the current roadmap.

## Phase 0 completed evidence

- Architecture decision record, feature inventory, parity matrix, fixture
  contract, capture protocol, and performance protocol are present.
- The fixture corpus contains eleven SHA-pinned fixtures and executable semantic
  and migration checks.
- The opt-in Windows Qt visual target covers 28 scenarios and performs the
  production show/settle/capture/close/cleanup lifecycle.
- Windows x64 Debug non-visual validation passed 67/67 tests, including the
  database-port fixture tests.
- The visual target passed 16/16 scenarios with
  `CLASSMNGR_PHASE0_DISPLAY_SCALE_PERCENT=100` on the replacement computer
  with one active 2560x1600 monitor. The final run is
  `20260828T083829986Z-18036`; every promoted sidecar reports 100% display
  scale, a 1270x1040 capture window, and `sourceRevision: d9732c8`.
  - The same visual target passed 16/16 scenarios with
  `CLASSMNGR_PHASE0_DISPLAY_SCALE_PERCENT=150` on the current 150%-scaled
  displays. The validated run is
  `20260828T080550115Z-12884`; every sidecar reports 150% display scale and
  `sourceRevision: d9732c8`. The stable 100% baseline remains retained for
  cross-scale comparison.
- The same visual target passed 16/16 scenarios with
  `CLASSMNGR_PHASE0_DISPLAY_SCALE_PERCENT=200` on one active monitor. The
  validated run is `20260828T091020942Z-21232`; every sidecar reports 200%
  display scale, a 1270x780 capture window, and `sourceRevision: d9732c8`.
- The expanded visual target passed 28/28 scenarios in final run
  `20260828T123139612Z-10236` at 150% display scale. It covers populated,
  empty, large, dirty, validation, and error states for the Classes editors;
  all 28 PNG/JSON pairs pass the artifact validator and report
  `sourceRevision: fb4f268`. Manual review and keyboard/IME evidence remain
  pending. The run takes 172.16 seconds against the current 180-second CTest
  timeout, so the matrix should be split or given more timeout margin before
  further growth.
- Phase 0 contract validation passes: 80 capture rows, 21 parity rows, and
  eleven fixtures.
- The current x64 Debug startup probe has three runs at approximately 3.4 s
  startup and a maximum peak of 703.9 MiB working set / 599.6 MiB private
  bytes. These are diagnostic values, not the approved Release budget.
- Windows ARM64 Qt compilation/linking was established as unofficial build
  compatibility evidence. No ARM64 runtime validation is planned in the
  current roadmap.

## Linux Qt validation

- `cmake --preset linux-gcc-debug` configured successfully with Qt 6.11.1;
  the retained `ClassMngr` executable and all 67 registered test targets
  rebuilt successfully.
- `ClassMngrDatabasePortFixtureTests` passed, verifying all eleven checked-in
  database fixtures and their migration/rollback expectations.
- The complete Linux Qt Debug suite passed 65/67 tests when run with
  `QT_QPA_PLATFORM=offscreen` and normal loopback access. All updater tests
  passed once the local HTTP test server was allowed to bind.
- The two remaining failures are the Linux memory snapshot assertion at
  `tests/memory_usage_tests.cpp:315` and the dependent startup-memory
  assertion at `tests/startup_performance_tests.cpp:658`. The Linux provider
  reads `/proc` pseudo-files with `QFile::atEnd()`, which sees their reported
  zero size and returns an unavailable sample. No Linux source was changed by
  this validation run.

## Current visual evidence

The latest validated expanded capture run is:

```text
artifacts/phase0/windows-qt-visual/20260828T123139612Z-10236/
```

It contains 28 PNG/JSON pairs, validated against the Phase 0 artifact
contract, at 150% display scale. The validated 100%/150%/200% baseline runs
remain available for cross-scale comparison. The stable 100% baseline at
`artifacts/phase0/windows-qt-visual/20260828T025557225Z-39876/` was replaced
with the single-monitor run `20260828T083829986Z-18036` so the canonical
evidence no longer includes the previous second-monitor capture. The validated
150% companion remains at
`artifacts/phase0/windows-qt-visual/20260828T080550115Z-12884/`; the 200% run
was generated on the same single-monitor setup.

The repository HEAD is `fb4f268`; the promoted 100%/150%/200% capture sidecars
correctly retain their captured-product `sourceRevision: d9732c8`. The
expanded editor run records `sourceRevision: fb4f268` and is automated evidence
pending visual/manual review; it is not yet a reviewed/stable golden matrix.

## Remaining Phase 0 work

1. Capture and review the remaining ledger states, including populated,
   validation, error, modal, print/PDF, and output flows.
2. Perform manual keyboard-only, Korean IME, focus-restoration, and
   unsaved-change checks. UI Automation, Narrator, and high-contrast work are
   deferred from the current roadmap.
3. Complete manual review of the 100%/150%/200% DPI evidence; all three
   automated matrices have been captured and validated on the replacement
   computer.
4. Resolve and rerun the two Linux diagnostics failures above, then validate
   the retained Qt product on macOS.
5. Collect a current Windows x64 Release baseline plus resize, scrolling,
   first-paint, output, and device-recovery measurements.

## Reproduce on another Windows device

From the repository root, with the required Qt 6.11.1 kit, Visual Studio,
Windows SDK, and an interactive monitor configured at 100%, 150%, or 200%
scaling:

```powershell
cmake --preset windows-x64-phase0-visual
cmake --build build/windows-x64-phase0-visual --config Debug --target ClassMngrWindowsQtVisualCaptureTests --parallel 2
$env:CLASSMNGR_PHASE0_DISPLAY_SCALE_PERCENT = "100" # or "150" or "200"
ctest --test-dir build/windows-x64-phase0-visual -C Debug -R ClassMngrWindowsQtVisualCaptureTests --output-on-failure
```

The test creates a timestamped directory below
`artifacts/phase0/windows-qt-visual/`. Validate the newly created directory
and the checked-in contracts with:

```powershell
.\scripts\porting\windows\validate_phase0_capture_artifacts.ps1 `
  -ArtifactRoot artifacts\phase0\windows-qt-visual\<new-run> `
  -ProjectRoot .
.\scripts\porting\windows\validate_phase0_contracts.ps1 -ProjectRoot .
```

## Documents of record

- [Phase 0 README](README.md)
- [Baseline results](phase-0/baseline-results.md)
- [Capture ledger](phase-0/capture-ledger.csv)
- [Performance baseline](phase-0/performance-baseline.md)
- [Port plan](../../../plans/windows-direct2d-directcomposition-port-plan.md)
