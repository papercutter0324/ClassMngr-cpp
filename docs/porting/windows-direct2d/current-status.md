# Windows Direct2D/DirectComposition port — current status

Last recorded: 2026-08-28 (Asia/Seoul)

This is the cross-device handoff for the Windows port. Phase 0 is still in
progress; its exit gate has not been claimed.

## Repository snapshot

- Branch: `NativeWindowsPort`, ahead of `origin/NativeWindowsPort` by five
  commits when this record was created.
- HEAD: `e483053` (`Add artifacts folder for cross device development`).
- The current handoff and artifact commits contain evidence/documentation
  changes relative to the last product source revision, `b34a357`; no native
  Direct2D/DirectComposition target has been implemented yet.
- The working tree was clean before this handoff file and its links were
  added. The handoff documentation is the pending change to commit before
  switching devices.

## Phase 0 completed evidence

- Architecture decision record, feature inventory, parity matrix, fixture
  contract, capture protocol, and performance protocol are present.
- The fixture corpus contains nine SHA-pinned fixtures and executable semantic
  and migration checks.
- The opt-in Windows Qt visual target covers 16 scenarios and performs the
  production show/settle/capture/close/cleanup lifecycle.
- Windows x64 Debug non-visual validation passed 67/67 tests, including the
  database-port fixture tests.
- The visual target passed 16/16 scenarios with
  `CLASSMNGR_PHASE0_DISPLAY_SCALE_PERCENT=100`. Every sidecar in the run
  reports 100% display scale and `sourceRevision: b34a357`.
- Phase 0 contract validation passes: 80 capture rows, 21 parity rows, and
  nine fixtures.
- The current x64 Debug startup probe has three runs at approximately 3.4 s
  startup and a maximum peak of 703.9 MiB working set / 599.6 MiB private
  bytes. These are diagnostic values, not the approved Release budget.
- Windows ARM64 Qt compilation/linking was established, but ARM64 runtime
  validation has not been performed.

## Linux Qt validation

- `cmake --preset linux-gcc-debug` configured successfully with Qt 6.11.1;
  the retained `ClassMngr` executable and all 67 registered test targets
  rebuilt successfully.
- `ClassMngrDatabasePortFixtureTests` passed, verifying all nine checked-in
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

The latest validated capture run is:

```text
artifacts/phase0/windows-qt-visual/20260828T025557225Z-39876/
```

It contains 16 PNG/JSON pairs. The artifact directory is ignored and is not
carried by Git; another device must either copy it separately or recapture it.
The capture was produced from the `b34a357` product build. Since `b4a4efd` is
documentation-only, the captured product source is unchanged, but a strict
current-HEAD provenance run should rebuild the target first.

## Remaining Phase 0 work

1. Capture and review the remaining ledger states, including populated,
   validation, error, modal, print/PDF, and output flows.
2. Perform manual keyboard-only, Korean IME, UI Automation/Narrator,
   high-contrast, focus-restoration, and unsaved-change checks.
3. Complete light-theme and 150%/200% DPI evidence; the 100% automated matrix
   has been recaptured.
4. Resolve and rerun the two Linux diagnostics failures above, then validate
   the retained Qt product on macOS and run Windows ARM64 tests on ARM64
   hardware.
5. Collect a current Windows x64 Release baseline and equivalent ARM64,
   resize, scrolling, first-paint, output, and device-recovery measurements.

## Reproduce on another Windows device

From the repository root, with the required Qt 6.11.1 kit, Visual Studio,
Windows SDK, and an interactive monitor configured at 100% scaling:

```powershell
cmake --preset windows-x64-phase0-visual
cmake --build build/windows-x64-phase0-visual --config Debug --target ClassMngrWindowsQtVisualCaptureTests --parallel 2
$env:CLASSMNGR_PHASE0_DISPLAY_SCALE_PERCENT = "100"
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
