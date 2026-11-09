# Current Slice Batches

## Phase 8a - Active Batch 2 (OS-agnostic QWARN fixes)

Batch 2 was pushed to `origin/Qt-Rewrite` at `90a8357e38d2b8d59af82fed2daa8cad5f9f4bb3`.
The four commits after Batch 1 are `6e57a663`, `3f1b298c`, `f839b03a`, and
`90a8357e`.

The superseded baseline run `42655216186` completed canceled after the push;
Windows ARM64 had passed. Replacement Refactoring Baseline run
`42664061605` is now active on Windows x64, Windows ARM64, macOS, and Linux.
Linux has started its baseline step; the other native/cross-build jobs are
setting up. C53 outcomes are pending.

The Next shared-widget resources and signed-64-bit schedule reader fixes
passed focused local verification. While replacement run `42664061605` is
active, the authorized Windows x64 local diagnostic slice is underway against
the direct Phase 8a targets. This is local diagnostic work only; do not push
until the active run completes. Separate Windows C53 matched-output run
`42664061512` passed. The Phase 8a plan remains the source for the full ordered
scope and acceptance criteria.
