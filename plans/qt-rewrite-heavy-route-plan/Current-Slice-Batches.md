# Current Slice Batches

## Phase 8a - provenance follow-up and local macOS baseline

OS-agnostic batch d034bb56 is pushed to origin/Qt-Rewrite. Refactoring
Baseline run 42732863398 ended after the Mac job failed before runner
assignment. Its check annotation reported failed account payments or a spending
limit that must be increased. Linux x64 and Windows ARM64 baseline jobs were
canceled; Windows x64 was force-canceled.

The user requested cancellation of all active GitHub workflows. Windows
Release run 42732863262, macOS Release run 42732863243, C53 run 42732863482,
and Phase 4 resource-layout run 42732863211 are canceled. Linux x64 Release
run 42732863214 had already completed successfully. A follow-up Actions API
query found no queued or in-progress runs.

The initial Mac mini run at commit `3538e0fb7895eb973cc7fd6046dca396e1d33304`
had 20 CTest failures, including three modal-dialog stalls at 120 seconds.
After fixing the failures, the Debug tree rebuilt and the full 620-target
CTest run passed: 620 passed, 0 failed, 0 timed out, 0 skipped (798.46s).
Loopback tests ran with local test access, and the C53 selector received its
reference configuration. The separate C53 capture also passed (17 pages, zero
changed pixels). Final JUnit, full CTest log, and per-target results are in
the ignored `build/phase8a-macos-debug-3538e0fb/` directory; no tracked visual
baseline files changed during the final run.

This verifies the local macOS Debug build and full CTest suite for the current
working tree. It does not close Phase 8a: stable provenance and the required
Schedule/Roster/Speaking/C53 evidence remain to be completed across the
Windows, macOS, and Linux candidates, with the QWARN inventory dispositioned.
See the [Phase 8a progress log](09.5-Phase-8a-Progress-Log.md) for failure
classification, timeout causes, and per-target runtimes.

The checked-out `Qt-Rewrite` branch and its local `origin/Qt-Rewrite` tracking
ref were both at `3538e0fb` when this work began; the commit graph includes the
Windows fixes and focused evidence. This task did not update the remote.
