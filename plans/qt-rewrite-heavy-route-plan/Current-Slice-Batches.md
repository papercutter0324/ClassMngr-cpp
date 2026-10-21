# Current Slice Batches

This is the single live tracker for slice batches across the Qt rewrite. Keep
only the active batch and the next batch after its discovery starts. Replace
closed batches; do not maintain phase-specific batch files or an archive of
completed batches. Keep slice requirements in the relevant phase plan, and
record useful implementation and verification evidence in its progress log.

## Current state

- Phase 3 is In Progress on the mandatory Heavy route.
- Batch 1 is active: F1318, F1319, F1320, F1321, and F1322 are complete; F1323 is next.
- Begin Batch 2 discovery when F1336 starts; do not add Batch 2 candidates before then.

## Phase 3 — Batch 1 (active)

1. F1318 — Close schema-v6 behavior baseline (complete; commit `991c046e`)
2. F1319 — WorkspaceStore lifecycle seam (complete; commit `f78a0b50`)
3. F1320 — Calendar display preferences persistence (complete; commit `c6ea61d6`)
4. F1321 — Existing-teacher profile persistence-port migration (complete; commit `d0f888e4`)
5. F1322 — ClassesPage list persistence-port migration (complete)
6. F1323 — My Classes student-count batch read
7. F1324 — Class Details display snapshot
8. F1325 — Class Details save
9. F1326 — Selected-class roster read (window scope discovery required)
10. F1327 — Selected-class roster save
11. F1328 — Speaking-evaluation read
12. F1329 — Speaking-evaluation save
13. F1330 — Schedule Builder source read (scope discovery required)
14. F1331 — Intensive slot-state save
15. F1332 — Calendar page range read
16. F1333 — Single Calendar Event save
17. F1334 — Calendar Import batch save
18. F1335 — Repeat-series suffix delete
19. F1336 — Sub Prep visible schedule summaries (scope discovery required)
20. F1337 — Selected Sub Prep class details
