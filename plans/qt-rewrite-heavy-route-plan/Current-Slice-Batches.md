# Current Slice Batches

This is the single live tracker for slice batches across the Qt rewrite. Keep
only the current batch closeout and the next discovered batch waiting for its
predecessor. Keep implementation evidence in the phase progress log.

## Current state

- Phase 3 is In Progress on the mandatory Heavy route; its exit gate remains
  open.

## Batch 14 — Slice work complete; closeout docs and push pending

F1436 is accepted and committed at
`cb6f90126dc2321b304cb87c5d6bcad642b18b27`. Batch 14 slice work is complete;
its closeout docs and push are pending. Acceptance evidence is in the [Phase 3
progress log](04-Phase-3-Progress-Log.md).

## Batch 15 — Discovered; waiting for Batch 14 closeout push

Discovery and bounded review completed under
`PHASE3-BATCH15-DISCOVERY-20261024`. F1437 is the sole accepted candidate:
avoid the full JSON DOM during mapped class-transfer import validation, with
a conservative Qt fallback. Its mapped-file validator must validate the
whole document before schema conversion and preserve the existing Qt path for
rejects, uncertain cases, and non-object roots. Keep public `fromJson()` and
`readAll()` fallbacks unchanged. Begin Batch 16 discovery when F1437 starts,
after Batch 14 closes and is pushed.
