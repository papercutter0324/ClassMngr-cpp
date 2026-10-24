# Current Slice Batches

This is the single live tracker for slice batches across the Qt rewrite. Keep
only the active batch and the next batch after its discovery starts. Replace
closed batches; do not maintain phase-specific batch files or an archive of
completed batches. Keep slice requirements in the relevant phase plan, and
record useful implementation and verification evidence in its progress log.

## Current state

- Phase 3 is In Progress on the mandatory Heavy route.
- Batch 3 is active: F1358–F1376 are accepted; F1377 — My Classes
  class-summary batch read is current.
- Batch 4 discovery is complete and queued as the next batch.

## Phase 3 — Batch 3 (active; F1377 current)

Batch 3 discovery began when F1356 started (candidate 19 of 20 in Batch 2) and
is complete.
Two complementary scans found more than 20 possible items; this is the
priority-ordered shortlist for Batch 3, not an exhaustive inventory.

1. F1358 — Testing Class details read (accepted)
2. F1359 — Testing Class details update (accepted)
3. F1360 — Testing Class delete (accepted)
4. F1361 — Class Co-Teacher page read (accepted)
5. F1362 — Class Co-Teacher teacher choices read (accepted)
6. F1363 — Class Co-Teacher assignment save (accepted)
7. F1364 — Class Notes page read (accepted)
8. F1365 — Class Notes save (accepted; commit `5833f388eaefdf38d1be9812a72267b8f2169d20`)
9. F1366 — Selected-class grade read (accepted; candidate 9 of 20)
10. F1367 — Selected-class subtitle read (accepted; commit `b1da9e3545ce5d7f0816b3d3880e801f264d0702`)
11. F1368 — Selected-class subtitle batch read (accepted; candidate 11 of 20)
12. F1369 — Classes navigation read (accepted; candidate 12 of 20)
13. F1370 — Class teacher assignments read (accepted; candidate 13 of 20)
14. F1371 — Regular Class creation and post-create by-ID lookup (accepted;
    one create flow; both ports)
15. F1372 — Class delete (accepted)
16. F1373 — Class Transfer preview read (accepted)
17. F1374 — Class Transfer export source read (accepted)
18. F1375 — Class Transfer apply (accepted)
19. F1376 — My Classes class-information batch read (accepted)
20. F1377 — My Classes class-summary batch read (current)

## Phase 3 — Batch 4 (discovered; next)

Batch 4 discovery began when F1376 started (candidate 19 of 20 in Batch 3) and
is complete. Two complementary scans found more than 20 possible active
candidates. The port-centric scan found 41 active Next Platform persistence
ports, including F1376 and F1377, leaving 39 further port candidates. The
feature-flow scan cross-checked user-facing workflows. This is the
priority-ordered shortlist, not an exhaustive inventory.

1. F1378 — My Classes teacher-profile batch read
2. F1379 — My Classes student-count batch read
3. F1380 — My Classes teacher-display-name batch read
4. F1381 — Roster snapshot read
5. F1382 — Roster column-name batch read
6. F1383 — Roster template-print source read
7. F1384 — Roster print class-info read
8. F1385 — Speaking-evaluation roster-name read
9. F1386 — Speaking-evaluation read
10. F1387 — Speaking-evaluation save
11. F1388 — GS Team directory read
12. F1389 — GS Team directory save
13. F1390 — Native English teacher directory read
14. F1391 — Native English teacher directory save
15. F1392 — Korean teacher birthday directory read
16. F1393 — GS Team birthday directory read
17. F1394 — Native English birthday directory read
18. F1395 — Current-campus preferences
19. F1396 — Class-visibility preferences
20. F1397 — Schedule-display preferences

For Batch 5 discovery, retain schedule-display-mode, academic-calendar and
academic-schedule, first-day and type-color preferences; custom palette;
personal details and signature; middle-school analytics; Sub Prep
preferences/Zoom; reset policies including class-day-filter reset; calendar
events; schedule import; teacher create/delete/import; Initial Setup choices;
and other active `databaseSession()`/DataService paths. `ClassService::rename`
was searched but has no callers; future discovery can recheck if callers
appear. Do not treat this shortlist as exhaustive.

Direct active session-check follow-ups to verify against actual consumers
before ranking: speaking-evaluation availability, the teacher-info save gate,
and sidebar prompt/display guards.

Begin Batch 5 discovery when work starts on F1396 (candidate 19 of 20 in Batch
4).
