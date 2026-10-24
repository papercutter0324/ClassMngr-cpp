# Current Slice Batches

This is the single live tracker for slice batches across the Qt rewrite. Keep
only the current batch closeout and the next discovered batch waiting for its
predecessor. Keep implementation evidence in the phase progress log.

## Current state

- Phase 3 is In Progress on the mandatory Heavy route; its exit gate remains
  open.

## Batch 17 — Implementation and docs complete; push pending

Batch 16 is pushed to `origin/Qt-Rewrite` at verified tip
`49ec21e783441fef616fae67cc633e9c7c6de578`. F1439 is committed at
`070593edffd3293e6f3bbd2fc3cc241c2e468564`; F1440 is committed at
`2d06dc1954ba5dcd5fae69532bc569696662bc76`. Both slices are accepted and
documented. Push remains pending; evidence and limitations are in the [Phase
3 progress log](04-Phase-3-Progress-Log.md).

## Batch 18 — Discovered; waiting for Batch 17 push

Discovery and bounded review completed under
`PHASE3-BATCH18-DISCOVERY-20261024`. F1441, consistent SQLite snapshots for
SaveAs/export from WAL-mode profiles, is the sole accepted candidate. It waits
for the Batch 17 push. Begin Batch 19 discovery when F1441 starts; scope and
constraints are in the [Phase 3 progress
log](04-Phase-3-Progress-Log.md).
