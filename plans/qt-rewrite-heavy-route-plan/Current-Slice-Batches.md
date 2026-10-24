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
- Batch 10 is closed and pushed to `origin/Qt-Rewrite` at verified tip
  `01670f53d7924a5e68ce9288effe3a4bf12b3403` (`Phase3 - Close Batch 10 and
  record Batch 11`).

## Batch 11 — Closed; closeout push pending

Discovery under `PHASE3-BATCH11-DISCOVERY-20261024` produced four ordered
candidates, and all four are accepted; Batch 11 is closed for slice work. The
final slice, F1432, is committed at
`2ce5bbe59ae67872fd025133b4e1fd5eca359a5e`; its mapped-file parity and bounded
full-target exception are recorded in the [Phase 3 progress log](04-Phase-3-Progress-Log.md).
The separate Batch 11 closeout commit/push remains pending.

## Batch 12 — Ready; waiting for Batch 11 push

Discovery and bounded review are complete under Exploration ID
`PHASE3-BATCH12-DISCOVERY-20261024`. Two independent scans and review accepted
these candidates in order; no other Batch 12 candidate was identified:

1. **F1433 — Populated schema-v6 `.db` SaveAs/export round-trip** through the
   v2 `ApplicationServicesWorkspacePort`, required compatibility gate
   evidence. No pre-v6 migration scope.
2. **F1434 — Schedule Builder row streaming and diagnostics:** stream rows from
   the repository to Application and report successful class/regular/intensive
   completed rows, preserving F1330's full uncapped board.

Possible Classes/Speaking Evaluation navigation detail-query/lifetime work is
deferred to Phase 7D/7H because eliminating consumer projections requires those
page/model changes. Begin Batch 13 discovery when F1433 starts (candidate 1 of
2), under the second-to-last-candidate rule. Batch 12 work remains inactive
until the Batch 11 closeout is pushed.
