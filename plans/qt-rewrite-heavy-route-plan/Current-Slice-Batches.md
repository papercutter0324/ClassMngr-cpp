# Current Slice Batches

This is the single live tracker for slice batches across the Qt rewrite. Keep
only the active batch and the next batch after its discovery starts. Replace
closed batches; do not maintain phase-specific batch files or an archive of
completed batches. Keep slice requirements in the relevant phase plan, and
record useful implementation and verification evidence in its progress log.

## Current state

- Phase 3 is In Progress on the mandatory Heavy route.
- Batch 2 is complete and unpushed, awaiting its authorized push; F1357
  (Testing class creation) is accepted. Batch 3 discovery is complete and Batch
  3 starts after the Batch 2 push.

## Phase 3 — Batch 2 (complete; pending push)

1. F1338 — Sub Prep information-sheet print-source read
2. F1339 — Sub Prep roster-output source read
3. F1340 — Sub Prep calendar-interval read
4. F1341 — Calendar availability and event-by-ID reads
5. F1342 — Single Calendar Event delete
6. F1343 — Delete all Calendar Events
7. F1344 — Repeat-series creation
8. F1345 — Repeat-series edit
9. F1346 — Calendar Import signature lookup
10. F1347 — Class Analytics dashboard persistence boundary (raw SQL behind repository)
11. F1348 — Roster availability batch read (raw SQL behind repository)
12. F1349 — Teacher Import apply (SQL transaction behind repository)
13. F1350 — Schedule slot-state read
14. F1351 — Schedule testing-assignment board read
15. F1352 — Schedule testing-class choices read
16. F1353 — Schedule testing-assignment save
17. F1354 — Clear Schedule testing assignments
18. F1355 — Schedule Editor selected-class information read
19. F1356 — Testing class teacher choices
20. F1357 — Testing class creation

## Phase 3 — Batch 3 (discovered; starts after Batch 2 push)

Discovery was completed when F1356 started (candidate 19 of 20 in Batch 2).
Two complementary scans found more than 20 possible items; this is the
priority-ordered shortlist for Batch 3, not an exhaustive inventory.

1. F1358 — Testing Class details read
2. F1359 — Testing Class details update
3. F1360 — Testing Class delete
4. F1361 — Class Co-Teacher page read
5. F1362 — Class Co-Teacher teacher choices read
6. F1363 — Class Co-Teacher assignment save
7. F1364 — Class Notes page read
8. F1365 — Class Notes save
9. F1366 — Selected-class grade read
10. F1367 — Selected-class subtitle read
11. F1368 — Selected-class subtitle batch read
12. F1369 — Classes navigation read
13. F1370 — Class teacher assignments read
14. F1371 — Regular Class creation and post-create by-ID lookup (one create flow; both ports)
15. F1372 — Class delete
16. F1373 — Class Transfer preview read
17. F1374 — Class Transfer export source read
18. F1375 — Class Transfer apply
19. F1376 — My Classes class-information batch read
20. F1377 — My Classes class-summary batch read

When Batch 4 discovery becomes due, continue searching for My Classes
teacher-profile batch and teacher-display-name reads; generic roster snapshot,
roster-column-name batch, roster template-print source, roster-print class-info;
speaking-evaluation roster names and additional speaking-evaluation
persistence; GS Team and Native English directory reads/saves; Korean/GS
Team/Native English birthday directories; current-campus, class-visibility,
schedule-display, schedule-display-mode preferences; calendar first-day,
type-color, academic-schedule preferences; custom-color palette, personal
display name/signature, middle-school analytics, Sub Prep preferences/Zoom,
class-day-filter reset; and any other active `databaseSession()`/DataService
persistence paths. `ClassService::rename` was searched but has no callers, so
it is not listed as an active candidate; future discovery can recheck if
callers appear. Do not treat the 20-item shortlist as all remaining candidates.
