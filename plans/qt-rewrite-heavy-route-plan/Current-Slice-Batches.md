# Current Slice Batches

This is the single live tracker for slice batches across the Qt rewrite. Keep
only the current batch closeout and the next discovered batch waiting for its
predecessor. Keep implementation evidence in the phase progress log.

## Current state

- Phase 3 is In Progress on the mandatory Heavy route; its exit gate remains
  open.

## Batch 16 — Slice work complete; closeout docs and push pending

Batch 15 is pushed to `origin/Qt-Rewrite` at verified tip
`af1024d336241e0f2fe7db5e3af90785d604d50e`. F1438 is accepted and committed
at `6ad8c16e4cb0c1c473f047ab6e760e24f6895792`; Batch 16 slice work is
complete, with closeout docs and push pending. Acceptance evidence is in the
[Phase 3 progress log](04-Phase-3-Progress-Log.md).

## Batch 17 — Discovered; waiting for Batch 16 closeout push

Two independent scans and bounded review completed under
`PHASE3-BATCH17-DISCOVERY-20261024`. F1439 (file-backed/seekable shared
workbook input) and F1440 (safe discovery and user-directed recovery of normal
profile-replacement recovery siblings) are the ordered accepted candidates.
They wait for the Batch 16 closeout push. Begin Batch 18 discovery when F1439
starts; scope and constraints are in the [Phase 3 progress
log](04-Phase-3-Progress-Log.md).
