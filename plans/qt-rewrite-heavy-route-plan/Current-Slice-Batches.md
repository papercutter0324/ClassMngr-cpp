# Current Slice Batches

This is the single live tracker for slice batches across the Qt rewrite. Keep
only the current batch closeout and the next discovered batch waiting for it.
Keep implementation evidence in the phase progress log.

## Current state

- Phase 3 is In Progress on the mandatory Heavy route; its exit gate remains
  open.

## Batch 12 — Slice work complete; closeout push pending

F1433 is committed at `cf3235a67a79e9820c85a6fb2320d64db8bbdaf9` and F1434 at
`6d10509b1b51cd687d975247209f1a2a86a1db7b`. Batch 12 is complete for slice
work; its closeout push is pending. Acceptance and verification evidence is in
the [Phase 3 progress log](04-Phase-3-Progress-Log.md).

## Batch 13 — Ready; waiting for Batch 12 closeout push

Discovery and bounded review completed under
`PHASE3-BATCH13-DISCOVERY-20261024`. The sole accepted candidate is F1435, a
bounded Class Details student-count roster read with existing validation and
profiler parity at the repository/existing-port boundary, without UI changes.
Preserve exact English/Korean first matching columns, trimmed nonblank count,
malformed/out-of-range errors, and profiler dimensions. This is source-derived;
no memory measurement is claimed. Defer the Class Transfer export projection
to Phase 8. Begin Batch 14 discovery when the one-candidate Batch 13 starts at
F1435, after Batch 12 closes and is pushed.
