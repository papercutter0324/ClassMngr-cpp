# Current Slice Batches

This is the single live tracker for slice batches across the Qt rewrite. Keep
only the current batch closeout and the next discovered batch waiting for its
predecessor. Keep implementation evidence in the phase progress log.

## Current state

- Phase 3 is In Progress on the mandatory Heavy route; its exit gate remains
  open.

## Batch 15 — Slice work complete; closeout docs and push pending

Batch 14 is pushed to `origin/Qt-Rewrite` at verified local/remote tip
`071560c8774f5c16c2b2502a5f561507e1f5f86f`. F1437 is accepted and committed
at `91355e2c0483c550eca93bb9b0b20bc0a7c55547`; Batch 15 slice work is
complete, with closeout docs and push pending. Acceptance evidence is in the
[Phase 3 progress log](04-Phase-3-Progress-Log.md).

## Batch 16 — Discovered; waiting for Batch 15 closeout push

Discovery and bounded review completed under
`PHASE3-BATCH16-DISCOVERY-20261024`. F1438 is the sole accepted doc-only
candidate: correct the stale F1419 schema coverage statement in
`docs/qt-rewrite/phase-3-schema-baseline.md`. Its scope and retained coverage
limits are recorded in the [Phase 3 progress log](04-Phase-3-Progress-Log.md).
Start the edit after Batch 15 is pushed; begin Batch 17 discovery when F1438
starts.
