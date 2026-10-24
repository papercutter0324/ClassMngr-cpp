# Current Slice Batches

This is the single live tracker for slice batches across the Qt rewrite. Keep
only the active batch and the next batch after its discovery starts. Replace
closed batches; do not maintain phase-specific batch files or an archive of
completed batches. Keep slice requirements in the relevant phase plan, and
record useful implementation and verification evidence in its progress log.

## Current state

- Phase 3 is In Progress on the mandatory Heavy route.
- Batch 3 (F1358–F1377) implementation tip was
  `018b30b04ab713769f9e69b440ba7223310a2b87`; its push receipt documentation
  was later pushed as
  `6597875f6036224767fa8db7331eac7eac856869`, the remote tip before Batch 4
  began.
- Batch 4 discovery is complete; Batch 4 is active with F1392 — Current-campus
  preferences current.

## Phase 3 — Batch 4 (active; F1392 current)

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
15. F1392 — Current-campus preferences (current)
16. F1393 — Class-visibility preferences
17. F1394 — Schedule-display preferences
18. F1395 — Schedule-display-mode preferences
19. F1396 — Calendar first-day-of-week preferences
20. F1397 — Academic-calendar schedule preferences

For Batch 5 discovery, retain type-color preferences; custom palette;
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
