# Current Slice Batches

## Phase 8a - OS-agnostic provenance follow-up

Refactoring Baseline run `42664061605` is complete. Linux and Windows x64 built
and completed baseline steps, but their source provenance is invalid: Qt
installation changed tracked `aqtinstall.log`, and C44 capture changed tracked
Phase 7 visual-baseline files. C53 parity passed on both hosts. The macOS job
was cancelled at its 300-minute timeout after 4,166 of 5,570 build steps. It
produced no baseline report, and C53 could not run because its target had not
been built.

The C44 provenance/capture fix is committed on the isolated OS batch branch as
`0cefbcb6`; its focused Windows x64 target passed twice locally. A Mac capacity
change to `macos-15-large` with 12-way compilation is committed there as
`e11ea4b2`. The OS batch is ready, but its push is waiting for approval to use
the billed runner and confirmation that the repository has access to it.

Windows-specific fixes remain committed locally on `Qt-Rewrite` and unpushed.
The hosted Windows baseline review, local target evidence, and QWARN dispositions
are recorded in the Phase 8a progress log. Keep those implementation commits
out of the OS-agnostic push; publish the Windows batch after the provenance
follow-up is validated.
