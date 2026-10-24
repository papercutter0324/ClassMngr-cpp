# Current Slice Batches

This is the single live tracker for slice batches across the Qt rewrite. Keep
the active batch and the next discovered batch while it waits for its
predecessor to close and push. Replace closed batches; do not maintain
phase-specific batch files or an archive of completed batches. Keep slice
requirements in the relevant phase plan, and record useful implementation and
verification evidence in its progress log.

## Current state

- Phase 3 is In Progress on the mandatory Heavy route.
- Batch 5 is complete (19/19 accepted) and pushed: source/acceptance tip
  `5725e723`, follow-up receipts `8d74cf7a` and `ea91cb64`, and handoff tip
  `b29a0c9b`.
- Batch 6 is complete and pushed with one accepted candidate, F1417, at
  `1a42b6ad1d94c1af28702ec3342d623c5d4d368b`. The push reached
  `origin/Qt-Rewrite`; local and remote matched and the worktree was clean.
- Batch 7 is active with one discovered candidate, F1418: move the Workspace
  Save/autosave commit from `ApplicationServices::saveDatabase()`'s
  `workspaceStore()->database().commit()` call behind `WorkspaceStore`.
  `DataService::save()` has no ordinary production caller; no other Batch 7
  candidates were found.
- Batch 8 discovery is complete and waiting for Batch 7 to close. Two
  complementary read-only scans found no other slices; the schema baseline's
  missing full DDL-shape/all-index tests were excluded as coverage-only, with
  no production behavior slice identified.
