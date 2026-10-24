# Current Slice Batches

This is the single live tracker for slice batches across the Qt rewrite. Keep
only the active batch and the next batch after its discovery starts. Replace
closed batches; do not maintain phase-specific batch files or an archive of
completed batches. Keep slice requirements in the relevant phase plan, and
record useful implementation and verification evidence in its progress log.

## Current state

- Phase 3 is In Progress on the mandatory Heavy route.
- Batch 2 is active and unpushed; F1350 (Schedule slot-state read) is
  complete, and F1351 is current: Schedule testing-assignment board read.

## Phase 3 — Batch 2 (active; current: F1351)

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

When Batch 3 discovery becomes due, search for Testing Class
details/update/delete, Class Co-Teacher and Class Notes flows, selected-class
grade/subtitle/navigation reads, class CRUD/transfer, My Classes, roster,
speaking evaluation, preferences/directories, additional persistence
boundaries, and any other candidates.
