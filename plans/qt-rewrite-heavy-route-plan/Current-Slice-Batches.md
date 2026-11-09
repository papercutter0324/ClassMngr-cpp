# Current Slice Batches

## Phase 8a - Active Batch 2 (OS-agnostic QWARN fixes)

Batch 2 was pushed to `origin/Qt-Rewrite` at `90a8357e38d2b8d59af82fed2daa8cad5f9f4bb3`.
The four commits after Batch 1 are `6e57a663`, `3f1b298c`, `f839b03a`, and
`90a8357e`.

The Batch 1 Refactoring Baseline run `42655216186` was superseded by that
push. Windows ARM64 passed; Linux and macOS were canceled; Windows x64 is
still finishing its baseline step. Replacement Refactoring Baseline run
`42664061605` is pending under the branch concurrency group. The Phase 4
resource-layout run `42664061594` and separate Windows C53 run
`42664061512` are also in progress.

The Next shared-widget resources and signed-64-bit schedule reader fixes
passed focused local verification. After replacement run `42664061605`
finishes, review failure/QWARN evidence and record OS-agnostic dispositions
before starting Windows-specific changes. The Phase 8a plan remains the
source for the full ordered scope and acceptance criteria.
