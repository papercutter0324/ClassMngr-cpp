# Current Slice Batches

This is the single live tracker for slice batches across the Qt rewrite. Keep
the active batch and the next discovered batch while it waits for its
predecessor to close and push. Replace closed batches; do not maintain
phase-specific batch files or an archive of completed batches. Keep slice
requirements in the relevant phase plan, and record useful implementation and
verification evidence in its progress log.

## Current state

- Phase 3 is In Progress on the mandatory Heavy route.
- Batch 9 is complete: all nine ordered slices are accepted and committed
  locally; the push is pending.

1. **F1419 — Runtime v6 DDL/index/constraint contract tests (accepted).** The
   focused `ClassMngrDatabaseSchemaManagerTests` CTest passed 1/1; its verbose
   QtTest rerun passed 31 cases. No production behavior change was part of this
   contract-test slice.
2. **F1420 — API-by-API `DataService` migration map (accepted, docs-only).**
   Independent review confirmed 77/77 declarations, both overloads,
   constructor/destructor ownership, and 101 source links. No tests or builds
   were run. See the [DataService migration
   map](../../docs/qt-rewrite/data-service-migration-map.md).
3. **F1421 — Populated v6 `.tps` SaveAs/export round-trip and backup
   comparison (accepted).** Focused macOS arm64 CTest passed 1/1; verbose rerun
   passed 40 QtTest cases. Both destinations reopened existing-only with
   expected fixture data, while source path/open/data were preserved. No
   Windows or interrupted-copy claim; see accepted F1423 below.
4. **F1422 — Valid-v6 locked and truncated-file preservation (accepted).**
   Focused macOS arm64/Qt 6.12 CTest passed 1/1; verbose rerun passed 42 QtTest
   cases. Same-process `BEGIN EXCLUSIVE` prevented existing-only open of a
   valid v6 profile; a 100-byte truncated SQLite profile was rejected. In both
   cases, the active store, session, services, connection, path, sentinel, and
   target bytes were preserved; the locked target reopened as v6 after release.
   No Windows claim.
5. **F1423 — Deterministic post-write `QSaveFile` interruption and atomic
   destination preservation (accepted).** Focused macOS CTest passed 1/1. The
   injected source-read failure on the second read, after one 64 KiB chunk had
   been written to the `QSaveFile` temporary file, was returned; source and
   existing destination were byte-identical, with no temporary artifact after
   `QSaveFile` destruction. The assertion was corrected to check afterward;
   no production defect was found. The internal helper is source-manifest
   registered; application API is unchanged. No full-suite or Windows claim.
6. **F1424 — Calendar Import transaction-start, commit, and error coverage
   (accepted).** Focused macOS arm64/Qt 6.12.0 CTest passed 1/1; verbose run
   passed 81 Qt cases with zero failed or skipped. Nested-transaction start
   failure left zero calendar rows and the expected sequence before and after
   outer rollback. Deferred-FK commit failure returned `Technical` and rolled
   back calendar/child rows and sequence; existing mid-write small and
   4097-event cases passed. No production changes; no full-suite or Windows
   claim.
7. **F1425 — Compact Schedule Import persistence snapshot plus large 96-class
   query/result/lifetime evidence (accepted).** Focused macOS arm64/Qt 6.12.0
   verification passed 8/8 executable cases, 3/3 for the large case, and CTest
   1/1. The full ordered output was checked; query counts cover teacher,
   metadata, regular, and intensive queries. The class-name query is not
   instrumented. No heap-byte or full-suite claim. Phase 7E retains review and
   presentation ownership.
8. **F1426 — Reduce workbook entry copies (accepted).** Commit
   `86eef5c9131e602ca915db94b4f9800cf6b7dd41`. Stored-entry views and direct
   deflate input remove one entry-sized scratch copy; the full archive and inflated
   worksheet XML remain allocated. No heap-byte, full-suite, or Windows claim.
9. **F1427 — Map class transfer input during parsing (accepted).** Commit
   `3bdcb245f7465bfad0c03a6e2137fea09d0fc793`. Mapped input is parsed before
   schema conversion, with `readAll()` fallback for zero, unrepresentable, or
   map-failure cases. Focused codec selectors passed 5/5; the full focused
   CTest selection had 64 passed and one unrelated baseline dialog failure.
   No heap/RSS or Windows claim. Phase 3.4 retains the codec boundary; Phase 8
   retains transfer/export operation ownership.

## Batch 10 — Discovery complete; waiting for Batch 9 push

Batch 10 discovery is complete. It waits for the Batch 9 push; it is not
active. **F1428 — Expose `ClassRepository::getClasses()` query/result
metrics for large snapshot diagnostics** is the sole ordered candidate.
Implementation has not started. It qualifies under Phase 3 §3.7, which
requires query/result-size diagnostics; F1425's 96-class parity test has no
metrics for the all-classes query. F1428 depends on accepted F1425. See the
[Batch 10 discovery record](04-Phase-3-Progress-Log.md)
for source evidence, proposed acceptance scope, and scan exclusions. The
ordered memory boundaries match the shared memory-hotspot plan; Phases 7 and
8 retain their existing consumer ownership. Because Batch 10 has one candidate,
begin Batch 11 discovery when F1428 starts.
