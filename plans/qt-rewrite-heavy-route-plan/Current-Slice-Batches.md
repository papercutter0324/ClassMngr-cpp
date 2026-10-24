# Current Slice Batches

This is the single live tracker for slice batches across the Qt rewrite. Keep
the active batch and the discovered next batch while it waits for its
predecessor to close and push. Replace closed and pushed batches; do not
maintain phase-specific batch files or an archive of completed batches. Keep
slice requirements in the relevant phase plan, and record useful implementation
and verification evidence in its progress log.

## Current state

- Phase 3 is In Progress on the mandatory Heavy route.
- Batch 3 (F1358–F1377) implementation tip was
  `018b30b04ab713769f9e69b440ba7223310a2b87`; its push receipt documentation
  was later pushed as
  `6597875f6036224767fa8db7331eac7eac856869`, the remote tip before Batch 4
  began.
- Batch 4 discovery is complete; Batch 4 is complete and awaiting push.
- Batch 5 discovery is complete; its 19 prioritized slices are queued until
  the Batch 4 push receipt is recorded.

## Phase 3 — Batch 4 (complete; awaiting push)

Batch 4 discovery began when F1376 started (candidate 19 of 20 in Batch 3) and
is complete. Two complementary scans found more than 20 possible active
candidates. The port-centric scan found 41 active Next Platform persistence
ports, including F1376 and F1377, leaving 39 further port candidates. The
feature-flow scan cross-checked user-facing workflows. This is the
priority-ordered shortlist, not an exhaustive inventory.

1. F1378 — My Classes teacher-profile batch read (accepted)
2. F1379 — My Classes teacher-display-name batch read (accepted)
3. F1380 — Roster snapshot read (accepted)
4. F1381 — Roster column-name batch read (accepted)
5. F1382 — Roster template-print source read (accepted)
6. F1383 — Roster print class-info read (accepted)
7. F1384 — Speaking-evaluation roster-name read (accepted)
8. F1385 — GS Team directory read (accepted)
9. F1386 — GS Team directory save (accepted)
10. F1387 — Native English teacher directory read (accepted)
11. F1388 — Native English teacher directory save (accepted)
12. F1389 — Korean teacher birthday directory read (accepted)
13. F1390 — GS Team birthday directory read (accepted)
14. F1391 — Native English birthday directory read (accepted)
15. F1392 — Current-campus preferences (accepted)
16. F1393 — Class-visibility preferences (accepted)
17. F1394 — Schedule-display preferences (accepted)
18. F1395 — Schedule-display-mode preferences (accepted)
19. F1396 — Calendar first-day-of-week preferences (accepted)
20. F1397 — Academic-calendar schedule preferences (accepted)

## Phase 3 — Batch 5 (discovered; queued until Batch 4 push receipt)

Two complementary scans found 19 prioritized, distinct persistence slices.
The source scan found exactly 19 active Next Platform ports after excluding
the two final Batch 4 slices, F1396 and F1397. The feature-flow scan
cross-checked consumers and separately surfaced display-name preference,
latest-source-date read, and residual Calendar Event range projection. Every
scan-identified item is a candidate, not a test result.

1. F1398 — Middle-school analytics preferences
2. F1399 — Calendar event-type color preferences
3. F1400 — Class-day-filter reset policy
4. F1401 — Class-selection reset policy
5. F1402 — Evaluation default policy
6. F1403 — Calendar event range projection (residual projection only; prior
   F1341 operations stay excluded)
7. F1404 — Personal display-name preferences
8. F1405 — Personal details save
9. F1406 — Personal signature preferences
10. F1407 — Personal signature image
11. F1408 — Sub Prep preferences
12. F1409 — Sub Prep personal Zoom preferences
13. F1410 — Custom color palette preferences
14. F1411 — Initial Setup teacher choices read
15. F1412 — Teacher create
16. F1413 — Teacher delete
17. F1414 — Teacher Import latest-source-date read (F1349 apply already
    done)
18. F1415 — Schedule Import current-state snapshot
19. F1416 — Schedule Import apply

The F1396 discovery trigger has been fulfilled. Three direct session-check
guards—Speaking Eval, Teacher Info, and Sidebar—were audited as incidental
behavior guards, not independent persistence ports; leave them in overflow.
`ClassService::rename` had no callers in the prior search; recheck only if
callers appear. Other active `databaseSession()`/DataService paths remain
unranked for future discovery. The shortlist is prioritized, not exhaustive.
