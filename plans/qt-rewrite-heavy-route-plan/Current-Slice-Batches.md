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
functionally verified and committed as `1f9b30bf`; its screenshot/reference
comparison remains an open 7A parity item because no matching Phase 0/7
screenshot exists. See the [Phase 7 progress
log](08-Phase-7-Feature-Migration.md#progress-log) for evidence and limits.
Candidate #3, teacher-entry setup, is functionally verified and committed as
`01aaf3d7`. Candidate #4, setup teacher import, is accepted and committed as
`8bee6dbb`. Its focused tests passed, including fixture-backed selection,
automatic NET/GS imports, rollback and retry, pending setup state, recovery
preservation, and cancel restoration. Candidate #5, setup class details, is
accepted and committed as `932fdfb2`; it captures a typed in-memory draft and
does not persist class, class-details, or schedule rows. Its focused
verification and limits are in the [Phase 7 progress
log](08-Phase-7-Feature-Migration.md#progress-log). Candidate #6, setup class
times, is accepted and committed as `97d45d3d`; its details and regular/
intensive schedule writes are transactional and retry preserves legacy
behavior. Candidate #7, setup schedule import, is accepted and committed as
`a83b7280`; it uses a Next-owned source/review flow and typed transactional
apply, and reports schedule clear counts. Focused verification and limits are
in the progress log. Candidate #8, normal `.tps` creation and replacement, is
accepted and committed as `95c1243a`; it creates empty schema-v6 workspaces
through a typed lifecycle and direct store adapter, preserves a recovery sibling
on replacement, and records recent history only after success. Focused
verification and limits are in the progress log. Candidate #9, existing
`.tps` open, is accepted and committed as `cdbf3ead`; focused verification and
limits are in the [Phase 7 progress
log](08-Phase-7-Feature-Migration.md#progress-log). Candidate #10, save, close,
and dirty state, is accepted and committed as `283871fa`; its focused evidence
and bounded Page dirty-hook limitation are in the progress log. Candidate #11,
Save As, is accepted and committed as `86b09d94`; its focused evidence and
limits are in the progress log. Current candidate: **#12 — Export**. The 7A
screenshot comparison and memory measurement remain open. Batch 1 remains
active and unpushed.
Next-batch discovery has not started; do not begin it before candidate #14.

Import, export, open, and save failure handling belongs to each owning
candidate, not a separate errors-only slice. No other slices were found.
