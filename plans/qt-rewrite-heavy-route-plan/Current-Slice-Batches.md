# Current Slice Batches

## Phase 8a - Active Batch 2 (OS-agnostic QWARN fixes)

Batch 1 is committed and pushed at `13eb0f97091c2d02c509053477b92173e1abd4f2`.
Its Refactoring Baseline run `42655216186` remains in progress after about 52
minutes; the Windows ARM64 cross-build passed, while native Windows x64, macOS,
and Linux have not finished. The workflow uses `cancel-in-progress: true` for
this branch. The Batch 2 push will supersede that run; use the replacement run
as the provenance/C53 integration gate for the OS-agnostic changes.

Batch 2 currently contains two committed slices:

1. **Next shared widget resources:** commit `6e57a6634f01710543b8c252bfecf09119dbb820`; the Next production and bootstrap-test targets receive the keyboard and spin icons.
2. **Wide schedule teacher IDs:** commit `3f1b298cead9810e608588a8ca9e244d54728f4b`; ClassInfo schedule reads and the My Workspace adapter preserve signed 64-bit IDs.

Both focused slices passed local verification. After the replacement run
finishes, review its failure and warning evidence, record OS-agnostic
triage/dispositions, and then begin Windows-specific changes. The Phase 8a plan
remains the source for the full ordered scope and acceptance criteria.
