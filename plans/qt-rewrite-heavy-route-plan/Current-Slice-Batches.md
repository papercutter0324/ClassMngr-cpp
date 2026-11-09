# Current Slice Batches

## Phase 8a - Active Batch 2 (OS-agnostic QWARN fixes)

Batch 1 is committed and pushed at `13eb0f97091c2d02c509053477b92173e1abd4f2`.
Its Refactoring Baseline run `42655216186` is still in progress. The Windows
ARM64 cross-build passed; native Windows x64, macOS, and Linux baseline jobs
have not finished. The workflow cancels an in-progress same-branch run on a
new push, so hold the Batch 2 push until this run completes.

Batch 2 currently contains two committed, unpushed slices:

1. **Next shared widget resources:** commit `6e57a6634f01710543b8c252bfecf09119dbb820`; the Next production and bootstrap-test targets receive the keyboard and spin icons.
2. **Wide schedule teacher IDs:** commit `3f1b298cead9810e608588a8ca9e244d54728f4b`; ClassInfo schedule reads and the My Workspace adapter preserve signed 64-bit IDs.

Both focused slices passed local verification. The remaining Batch 2 work is to
review the completed Batch 1 failure and warning evidence, record dispositions,
and then push Batch 2. Windows-specific changes follow that OS-agnostic review.
The Phase 8a plan remains the source for the full ordered scope and acceptance
criteria.
