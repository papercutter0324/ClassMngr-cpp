# Current Slice Batches

This is the single live tracker for slice batches across the Qt rewrite. Keep
only the current batch closeout and the next discovered batch waiting for its
predecessor. Keep implementation evidence in the phase progress log.

## Current state

- Phase 3 is In Progress on the mandatory Heavy route; its exit gate remains
  open.

## Batch 18 — Active at F1441

Batch 17 is pushed to `origin/Qt-Rewrite` at verified local/remote tip
`11db3892c3533f7dd7d270be30905bd3d730fdd2`. F1441 is the active candidate.
Its Online Backup API mechanism review is ongoing; no implementation or
review outcome is recorded.

## Batch 19 — Discovery complete; no new candidate accepted

Discovery and bounded review completed under
`PHASE3-BATCH19-DISCOVERY-20261024`. No candidate was accepted beyond active
F1441. The F1418 `void saveDatabase()` disposition is retained; see the
[Phase 3 progress log](04-Phase-3-Progress-Log.md) for rationale.
