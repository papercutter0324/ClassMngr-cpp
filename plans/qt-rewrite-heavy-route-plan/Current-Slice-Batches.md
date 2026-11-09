# Current Slice Batches

## Phase 8a - Batch 2 provenance follow-up and Windows x64 local work

Batch 2 was pushed to `origin/Qt-Rewrite` at
`90a8357e38d2b8d59af82fed2daa8cad5f9f4bb3`. In Refactoring Baseline run
`42664061605`, the Windows ARM64 cross-build passed; Linux and Windows x64
completed with failed baseline steps; macOS remains active.

The Linux artifact reports 19/619 CTest failures and the Windows artifact
reports 43/618. Both fail provenance because Qt setup modified tracked
`aqtinstall.log` and the C44 capture wrote screenshots and generated PDFs into
the tracked source visual-baseline directory. C53 SQL Sub Prep parity passed
on both hosts. Their reports and artifact checksums are recorded in the Phase
8a progress log.

The local provenance follow-up is committed as `18affbb2`: Qt installation
steps run from runner temp; the C44 capture writes to the build tree, and its
generated package uses a temporary directory. The focused Windows x64 target
built and passed twice; capture files stayed under
`build/windows-x64-debug/artifacts/next/sub-prep-c44/`, leaving tracked docs
unchanged.

The Windows Schedule visual-capture mismatch is fixed locally in `21ed5b9b`:
Next's toolbar now matches Legacy's 34 px height. The full 32-capture target
passed twice in the standard Windows x64 Debug tree. Windows x64 direct-target
fixes remain committed locally but unpushed; their focused evidence and QWARN
dispositions are in the Phase 8a progress log. The hosted run's Schedule
visual-capture timeout came from the old SHA and still needs candidate review.

Hold pushes until run `42664061605` completes. Then push the provenance
follow-up as its own batch, review its clean hosted run, and push the Windows
batch afterward. The Phase 8a plan remains the source for full ordered scope
and acceptance criteria.
