# Current Slice Batches

## Phase 8a - Batch 2 verification and Batch 3 Windows x64 local work

Batch 2 was pushed to `origin/Qt-Rewrite` at `90a8357e38d2b8d59af82fed2daa8cad5f9f4bb3`.
The four commits after Batch 1 are `6e57a663`, `3f1b298c`, `f839b03a`, and
`90a8357e`.

The superseded baseline run `42655216186` completed canceled after the push;
Windows ARM64 had passed. Replacement Refactoring Baseline run
`42664061605` is active on Windows x64, Windows ARM64, macOS, and Linux. The
Windows ARM64 cross-build passed; native baseline steps are in progress, with
that run's C53 steps pending. Separate Windows C53 run `42664061512` passed.

The Next shared-widget resources and signed-64-bit schedule reader fixes
passed focused local verification. Replacement run `42664061605` remains
active. While it runs, Windows x64 direct-target diagnostics are complete and
the SchedulePage/My Workspace header-initialization fix is committed locally
as `42bcb0c9` and independently passed its focused CTest targets. The
FileDialog shortcut-fixture correction is committed locally as `11354fbc`;
its target passed independent verification. The Speaking mixed-template test
correction is committed locally as `7a0dd151` and passed its focused target.
The Schedule PDF text assertion now passes in the standard Windows x64 Debug
tree after the test target was wired to the embedded application fonts; that
fix is committed locally as `1e4bf221` and its evidence is recorded in the
Phase 8a progress log. Restoring the original SubPrep Schedule spacing also
passes strict text and Windows raster parity in the package and print/PDF
targets; its evidence and the Windows QWARN dispositions are recorded in the
Phase 8a progress log. The unprovisioned Documents resource-pack warning in
the ClassTransfer test target is handed to Phase 11 test-harness work. Local
Windows x64 direct-target work is complete. Hold the push until active run
`42664061605` completes, then push the Windows batch and run the clean hosted
Windows candidate. Separate Windows C53 matched-output run `42664061512`
passed. The Phase 8a plan remains the source for the full ordered scope and
acceptance criteria.
