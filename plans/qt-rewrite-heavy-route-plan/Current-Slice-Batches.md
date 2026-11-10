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

The simpler Mac mini path uses the existing macOS universal Debug preset and
baseline recorder from a clean local checkout. The recorder builds all targets
and runs the full CTest suite; the matched SQL Sub Prep capture can then run as
a separate step. Commands and evidence paths are in
[the local macOS build guide](../../docs/qt-rewrite/phase8a/macos-local-debug-build.md).
The Mac build and C53 evidence are still pending.

Windows-specific fixes and focused evidence remain committed locally on
Qt-Rewrite and unpushed. Publish the implementation batch after the
OS-agnostic baseline has been reviewed.
