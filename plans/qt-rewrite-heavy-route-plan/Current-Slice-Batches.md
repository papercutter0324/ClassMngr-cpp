# Current Slice Batches

This is the single live tracker for slice batches across the Qt rewrite. Keep
the active batch and the next discovered batch while it waits for its
predecessor to close and push. Replace closed batches; do not maintain
phase-specific batch files or an archive of completed batches. Keep slice
requirements in the relevant phase plan, and record useful implementation and
verification evidence in its progress log.

## Current state

- Phase 3 is In Progress on the mandatory Heavy route.
- Batch 9 is active and discovery is complete with nine ordered candidates.

1. **F1419 — Runtime v6 DDL/index/constraint contract tests (accepted).** The
   focused `ClassMngrDatabaseSchemaManagerTests` CTest passed 1/1; its verbose
   QtTest rerun passed 31 cases. No production behavior change was part of this
   contract-test slice.
2. **F1420 — API-by-API `DataService` migration map (current).**
3. **F1421 — Populated v6 `.tps` SaveAs/export round-trip and backup comparison.**
4. **F1422 — Valid-v6 locked and truncated-file preservation.**
5. **F1423 — Deterministic post-write `QSaveFile` interruption and atomic
   destination preservation.**
6. **F1424 — Calendar Import transaction-start, commit, and error coverage.**
7. **F1425 — Compact Schedule Import persistence snapshot plus large 96-class
   query/result/lifetime evidence.** Phase 3 owns the persistence/query side;
   Schedule Import review and presentation remain Phase 7E work.
8. **F1426 — Staged/file-backed workbook representation lifetime at the
   reader boundary.** This preserves the shared Phase 3/7F boundary.
9. **F1427 — Class-transfer codec representation lifetime before dialog/UI.**
   Phase 3.4 retains the codec boundary; Phase 8 retains transfer/export
   operation ownership.

Begin Batch 10 discovery when F1426 (candidate 8 of 9, the second-to-last)
starts. The ordered memory boundaries match the shared memory-hotspot plan;
Phases 7 and 8 retain their existing consumer ownership.
