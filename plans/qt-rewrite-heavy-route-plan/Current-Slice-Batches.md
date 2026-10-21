# Current Slice Batches

This is the single live tracker for slice batches across the Qt rewrite. Keep
only the active batch and the next batch after its discovery starts. Replace
closed batches; do not maintain phase-specific batch files or an archive of
completed batches. Keep slice requirements in the relevant phase plan, and
record useful implementation and verification evidence in its progress log.

## Current state

- Phase 3 is In Progress on the mandatory Heavy route.
- Batch 1 is active: F1318–F1334 are complete; F1335 implementation is current.
- Begin Batch 2 discovery when F1336 starts; do not add Batch 2 candidates before then.

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
17. F1334 — Calendar Import batch save (complete)
18. F1335 — Repeat-series suffix delete
19. F1336 — Sub Prep visible schedule summaries (scope discovery required)
20. F1337 — Selected Sub Prep class details
