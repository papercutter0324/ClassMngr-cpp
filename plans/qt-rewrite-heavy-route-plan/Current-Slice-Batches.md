# Current Slice Batches

## Phase 8a - OS-agnostic provenance follow-up

Refactoring Baseline run `42664061605` has finished. Linux and Windows x64
built and completed their baseline steps, but their reports fail clean
provenance because Qt installation modified tracked `aqtinstall.log` and C44
capture output changed tracked Phase 7 visual-baseline files. C53 parity passed
on both hosts. The macOS job timed out after 300 minutes during its build at
4,166 of 5,570 Ninja steps; it produced no baseline report, and C53 could not
run because its target had not been built.

The provenance/capture fix is committed as `0cefbcb6` on an isolated worktree
based at `origin/Qt-Rewrite`. Its focused Windows x64 C44 capture target passed
twice and left tracked docs unchanged. A macOS capacity change is committed
locally as `e11ea4b2`, selecting the 12-core Intel `macos-15-large` runner with
12-way parallel compilation. Its push is pending approval for billed runner
use and confirmation that the runner is available to this repository.

Windows-specific fixes and focused evidence remain committed locally on the
`Qt-Rewrite` branch and unpushed. Keep them out of the OS-agnostic push; publish
the Windows batch after the provenance follow-up is validated.
