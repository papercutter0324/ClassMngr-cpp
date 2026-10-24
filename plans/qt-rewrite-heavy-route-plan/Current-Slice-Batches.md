# Current Slice Batches

This is the single live tracker for slice batches across the Qt rewrite. Keep
the active batch and the next discovered batch while it waits for its
predecessor to close and push. Replace closed batches; do not maintain
phase-specific batch files or an archive of completed batches. Keep slice
requirements in the relevant phase plan, and record useful implementation and
verification evidence in its progress log.

## Current state

- Phase 3 is In Progress on the mandatory Heavy route.
- Batch 3 (F1358–F1377) implementation tip was
  `018b30b04ab713769f9e69b440ba7223310a2b87`; its push receipt documentation
  was later pushed as
  `6597875f6036224767fa8db7331eac7eac856869`, the remote tip before Batch 4
  began.
- Batch 4 (F1378–F1397) is complete and pushed to `origin/Qt-Rewrite`; its
  implementation tip is `544b94cf97ab9fce8bb1e174ba1d9e0c9774d1c3`.
- Batch 5 is complete locally (19/19 slices accepted) at implementation tip
  `61d1fc9f`; it remains unpushed.
- Batch 6 discovery found no prioritized candidates: `No other slices were
  found.` Broader `databaseSession()`/`DataService` paths remain unranked for
  future discovery; this does not exhaust the Phase 3 backlog.

## Phase 3 — Batch 5 (complete locally; unpushed)

Two complementary scans found 19 prioritized, distinct persistence slices.
The source scan found exactly 19 active Next Platform ports after excluding
the two final Batch 4 slices, F1396 and F1397. The feature-flow scan
cross-checked consumers and separately surfaced display-name preference,
latest-source-date read, and residual Calendar Event range projection. Every
scan-identified item is a candidate, not a test result.

1. F1398 — Middle-school analytics preferences (accepted)
2. F1399 — Calendar event-type color preferences (accepted)
3. F1400 — Class-day-filter reset policy (accepted)
4. F1401 — Class-selection reset policy (accepted)
5. F1402 — Evaluation default policy (accepted)
6. F1403 — Calendar event range projection (accepted; residual projection only;
   prior F1341 operations stay excluded)
7. F1404 — Personal display-name preferences (accepted)
8. F1405 — Personal details save (accepted)
9. F1406 — Personal signature preferences (accepted)
10. F1407 — Personal signature image (accepted)
11. F1408 — Sub Prep preferences (accepted)
12. F1409 — Sub Prep personal Zoom preferences (accepted)
13. F1410 — Custom color palette preferences (accepted)
14. F1411 — Initial Setup teacher choices read (accepted)
15. F1412 — Teacher create (accepted; Initial Setup parity test aborted)
16. F1413 — Teacher delete (accepted)
17. F1414 — Teacher Import latest-source-date read (accepted; F1349 apply
    already done)
18. F1415 — Schedule Import current-state snapshot (accepted at `e78f2056`)
19. F1416 — Schedule Import apply (accepted; implementation tip `61d1fc9f`)

Three direct session-check guards—Speaking Eval, Teacher Info, and Sidebar—
were audited as incidental behavior guards, not independent persistence
ports; leave them in overflow. `ClassService::rename` had no callers in the
prior search; recheck only if callers appear. FeatureService repository calls
were treated as the legacy facade, not a new v2 slice in this shortlist. The
shortlist is prioritized, not exhaustive.
