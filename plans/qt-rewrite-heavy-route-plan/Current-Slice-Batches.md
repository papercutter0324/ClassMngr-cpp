# Current Slice Batches

## Phase 8a - provenance follow-up and local macOS baseline

OS-agnostic batch d034bb56 and the follow-up C53 recorder fix at
3dc37340 are pushed to origin/Qt-Rewrite. Refactoring Baseline run 42732863398
ended after the Mac job failed before runner assignment. Its check annotation
reported failed account payments or a spending limit that must be increased.
Linux x64 and Windows ARM64 baseline jobs were canceled; Windows x64 was
force-canceled.

The user requested cancellation of all active GitHub workflows. Windows
Release run 42732863262, macOS Release run 42732863243, C53 run 42732863482,
and Phase 4 resource-layout run 42732863211 are canceled. Linux x64 Release
run 42732863214 had already completed successfully. A follow-up Actions API
query found no queued or in-progress runs.

An earlier Mac mini run at commit `3538e0fb7895eb973cc7fd6046dca396e1d33304`
had 20 CTest failures, including three modal-dialog stalls at 120 seconds.
Those app, fixture, harness, and sandbox issues were triaged and the local
suite passed afterward. Its recorder provenance was not source-stable, so the
run is diagnostic rather than baseline evidence.

The final local baseline at commit
`3dc373406e0f31409f572ccf35815541737f0d1f` used the `macos-clang-debug`
preset on macOS 27.2 arm64. The clean build completed all 5,572 Ninja steps.
Unrestricted CTest ran all 620 targets: 620 passed, 0 failed, 0 timed out,
0 skipped in 871.72 seconds. C53 passed with 17 pages compared, zero changed
pixels, and matching font-resource provenance. The recorder reports valid,
clean start/end provenance with an unchanged source fingerprint.

The initial sandboxed CTest attempt had six failures: local HTTP tests could
not bind loopback sockets, and three QWizard tests aborted when AppKit received
a nil NSBundle URL. The sandbox itself denied a plain localhost bind with
`EPERM`. All six targets passed when rerun with normal macOS framework and
loopback access, and the full unrestricted CTest run also passed. These were
execution restrictions, not app or test defects. No tests timed out. The
successful baseline JSON, log, JUnit, full CTest transcript, and C53 capture
are under the ignored
`build/phase8a-macos-debug-3dc37340/artifacts/` and
`build/phase8a-macos-debug-3dc37340/Testing/` directories.

This closes the source-stable local macOS Debug, full-CTest, and C53 slice for
the current candidate. It does not close Phase 8a: Windows and Linux still
need their clean baseline/capture evidence, and the cross-host QWARN inventory
and required Schedule/Roster/Speaking artifacts remain open. See the
[Phase 8a progress log](09.5-Phase-8a-Progress-Log.md) for exact provenance,
failure classification, timeout findings, and runtime evidence.

The checked-out `Qt-Rewrite` branch and `origin/Qt-Rewrite` tracking ref now
both point to `3dc37340`.
