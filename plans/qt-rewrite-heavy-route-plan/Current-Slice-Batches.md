# Current Slice Batches

This is the single live tracker for slice batches across the Qt rewrite. Keep
the active batch and the next discovered batch while it waits for its
predecessor to close and push. Replace closed batches; do not maintain
phase-specific batch files or an archive of completed batches. Keep slice
requirements in the relevant phase plan, and record useful implementation and
verification evidence in its progress log.

## Current state

- Phase 3 is In Progress on the mandatory Heavy route.
- Batch 7 is complete and pushed with F1418 at
  `7e9760f354a6be5c02936ff728fa0bc93ea8e6bc`. The push reached
  `origin/Qt-Rewrite`; local and remote tips matched and the worktree was clean.
- Batch 8 discovery is complete. Two complementary read-only scans found no
  other slices. The schema baseline's missing full DDL-shape/all-index tests
  were excluded as coverage-only because no production behavior slice was
  identified. `DataService::save()` has no ordinary production caller.
- No batch or candidate is active. Phase 3 remains In Progress; this discovery
  does not exhaust its backlog.
