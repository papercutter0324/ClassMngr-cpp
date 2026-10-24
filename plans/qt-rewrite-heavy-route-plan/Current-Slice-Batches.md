# Current Slice Batches

This is the single live tracker for slice batches across the Qt rewrite. Keep
only the active batch and the next batch after its discovery starts. Replace
closed batches; do not maintain phase-specific batch files or an archive of
completed batches. Keep slice requirements in the relevant phase plan, and
record useful implementation and verification evidence in its progress log.

## Current state

- Phase 3 is In Progress on the mandatory Heavy route.
- Batch 3 discovery is complete. F1358–F1364 are accepted; F1365 — Class
  Notes save is current.

## Phase 3 — Batch 3 (active; F1365 current)

Discovery was completed when F1356 started (candidate 19 of 20 in Batch 2).
Two complementary scans found more than 20 possible items; this is the
priority-ordered shortlist for Batch 3, not an exhaustive inventory.

1. F1358 — Testing Class details read (accepted)
2. F1359 — Testing Class details update (accepted)
3. F1360 — Testing Class delete (accepted)
4. F1361 — Class Co-Teacher page read (accepted)
5. F1362 — Class Co-Teacher teacher choices read (accepted)
6. F1363 — Class Co-Teacher assignment save (accepted)
7. F1364 — Class Notes page read (accepted)
8. F1365 — Class Notes save (current)
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

Batch 4 discovery has not started. Begin it when work starts on F1376
(candidate 19 of this original 20-item shortlist).
