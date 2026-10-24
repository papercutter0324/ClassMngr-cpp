# Current Slice Batches

This is the single live tracker for slice batches across the Qt rewrite. Keep
the active batch and the next discovered batch while it waits for its
predecessor to close and push. Replace closed batches; do not maintain
phase-specific batch files or an archive of completed batches. Keep slice
requirements in the relevant phase plan, and record useful implementation and
verification evidence in its progress log.

## Current state

- Phase 3 is In Progress on the mandatory Heavy route; its exit gate remains
  open.
- Batch 10 is locally complete. F1428's code/test commit
  `3279627ddf4dbb0725a546ed4cd0ed6759c5a9f7` is committed; its closeout docs
  commit and push are pending.
- Batch 11 discovery is complete and inactive until Batch 10 is pushed.

## Batch 10 — Locally complete; closeout push pending

**F1428 — Expose all-class read diagnostics** is accepted and committed
locally. Its code/test commit is
`3279627ddf4dbb0725a546ed4cd0ed6759c5a9f7`; the closeout docs commit and push
are pending. See the [progress log](04-Phase-3-Progress-Log.md) for metrics,
verification, and limitations.

## Batch 11 — Discovery complete; waiting for Batch 10 push

Two independent scans under Exploration ID
`PHASE3-BATCH11-DISCOVERY-20261024` found no further data/API migration
candidate and identified five memory/query candidates. A bounded solution
review recommends these four ordered candidates:

1. **F1429 — Schedule Import row-count metrics** for teacher and
   class-info/time rows.
2. **F1430 — Speaking Evaluation result lifetime/streaming** without a row
   cap.
3. **F1431 — Bounded worksheet parsing** in the shared reader and consumers.
4. **F1432 — Class-transfer JSON DOM/package overlap.**

Calendar parsed-event consumer lifetime is intentionally deferred to Phase 7F,
its owning phase. Batch 11 remains inactive until Batch 10 is pushed. Begin
Batch 12 discovery when F1431 starts, the second-to-last candidate.
