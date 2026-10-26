# Current Slice Batches

## Active Batch 1 — Phase 7A setup and workspace/file workflows

Discovery is complete from two independent Explorer maps. Ordered candidates:

1. User-visible v2 profile-bootstrap stage and recovery.
2. Setup personal details and signature.
3. Setup teacher entry.
4. Setup teacher import.
5. Setup class details.
6. Setup class times.
7. Setup schedule import.
8. Normal `.tps` creation and replacement.
9. Existing `.tps` open.
10. Save, close, and dirty state.
11. Save As.
12. Export.
13. Recent-history menu, restore, and clear.
14. Normal replacement and interrupted-create recovery.
15. Schema-v6 `.db` input and native `.tps` normalization.

Candidate #1, the user-visible v2 profile-bootstrap stage, is accepted and
committed as `d49f34fe`. Candidate #2, setup personal details and signature, is
functionally verified; its screenshot/reference comparison remains an open 7A
parity item because no matching Phase 0/7 screenshot exists. See the [Phase 7
progress log](08-Phase-7-Feature-Migration.md#progress-log) for evidence and
limits. Current candidate: **#3 — setup teacher entry**. Batch 1 remains active
and unpushed. Next-batch discovery has not started.

Import, export, open, and save failure handling belongs to each owning
candidate, not a separate errors-only slice. No other slices were found.
