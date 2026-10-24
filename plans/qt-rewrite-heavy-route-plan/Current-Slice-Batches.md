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
- Batch 6 is complete with one accepted candidate, F1417. Its source/acceptance
  commit and batch push are pending. F1417 moves F1348's roster-availability
  read transaction into `WorkspaceStore` while preserving snapshot behavior.
- Batch 7 discovery is complete; it is waiting for Batch 6's source/acceptance
  commit and push. F1418 is the first/current candidate: move the Workspace
  Save/autosave commit from `ApplicationServices::saveDatabase()`'s
  `workspaceStore()->database().commit()` call behind `WorkspaceStore`.
  `DataService::save()` is unused in ordinary production composition; no other
  Batch 7 candidates were found.
