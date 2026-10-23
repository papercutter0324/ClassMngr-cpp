# Current Slice Batches

This is the single live tracker for slice batches across the Qt rewrite. Keep
only the active batch and the next batch after its discovery starts. Replace
closed batches; do not maintain phase-specific batch files or an archive of
completed batches. Keep slice requirements in the relevant phase plan, and
record useful implementation and verification evidence in its progress log.

## Current state

- Phase 3 is In Progress on the mandatory Heavy route.
- Batch 1 is active: F1318–F1336 are complete; F1337 Selected Sub Prep class
  details is current.
- Batch 2 discovery is complete: the 20 ordered candidates below remain
  inactive until Batch 1 closes and is pushed.

## Phase 3 — Batch 1 (active)

1. F1318 — Close schema-v6 behavior baseline (complete; commit `991c046e`)
2. F1319 — WorkspaceStore lifecycle seam (complete; commit `f78a0b50`)
3. F1320 — Calendar display preferences persistence (complete; commit `c6ea61d6`)
4. F1321 — Existing-teacher profile persistence-port migration (complete; commit `d0f888e4`)
5. F1322 — ClassesPage list persistence-port migration (complete; commit `582460e2`)
6. F1323 — My Classes student-count persistence-port migration (complete; commit `79004e09`)
7. F1324 — Class Details display persistence-port migration (complete; commit `d536916f`)
8. F1325 — Class Details save workflow persistence-port migration (complete; commit `8174df22`)
9. F1326 — Selected-class roster read (rows 0–24; preserve full-read contract; guard writes when rows extend beyond window; complete; commit `7f10f8f5`)
10. F1327 — Selected-class roster save (transactional tail preservation and column remapping; complete; commit `b705c158`)
11. F1328 — Speaking-evaluation read (complete; commit `fee2d6db`)
12. F1329 — Speaking-evaluation save (complete; commit `e696bbf0`)
13. F1330 — Schedule Builder source read (complete; preserve complete board projection through WorkspaceStore; commit `51449b56`)
14. F1331 — Intensive slot-state save (complete; commit `728d90ff`)
15. F1332 — Calendar page range read (complete; worker-local WorkspaceStore; commit `c5289ae2`)
16. F1333 — Single Calendar Event save (complete; commit `8ef71240`)
17. F1334 — Calendar Import batch save (complete; commit `abc8a278`)
18. F1335 — Repeat-series suffix delete (complete)
19. F1336 — Sub Prep visible schedule summaries (complete)
20. F1337 — Selected Sub Prep class details (current)

## Phase 3 — Batch 2 (discovered; inactive)

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
