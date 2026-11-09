# Current Slice Batches

## Phase 8a - Batch 2 provenance follow-up and Windows x64 local work

Batch 2 was pushed to `origin/Qt-Rewrite` at
`90a8357e38d2b8d59af82fed2daa8cad5f9f4bb3`. Refactoring Baseline run
`42664061605` is active on Windows x64 and macOS; Linux has completed with a
failed baseline step, and the Windows ARM64 cross-build passed. Separate
Windows C53 run `42664061512` passed.

The Linux artifact reports configure/build success and 19/619 CTest failures.
Its provenance is invalid: `aqtinstall.log` was dirty at start, and the C44
visual capture changed tracked screenshots and added PDFs under the source
tree. The C53 SQL Sub Prep capture passed with 12 Sub Prep pages, 5 Roster
pages, identical pixels, and matching font provenance. Details are in the
Phase 8a progress log.

The local provenance follow-up is committed as `18affbb2`: Qt installation
steps run from runner temp; the C44 capture writes to the build tree, and its
generated package uses a temporary directory. The focused Windows x64 target
built and passed twice; capture files stayed under
`build/windows-x64-debug/artifacts/next/sub-prep-c44/`, leaving tracked docs
unchanged.

Windows x64 direct-target diagnostics are complete. The local Windows fixes
remain committed but unpushed; their focused evidence and QWARN dispositions
are recorded in the Phase 8a progress log. Hold pushes until run
`42664061605` completes. Then push the provenance follow-up as its own batch,
review its clean hosted run, and push the Windows batch afterward. The Phase
8a plan remains the source for full ordered scope and acceptance criteria.
