# Current Slice Batches

## Batch 2 — active

Batch 1 is complete and pushed at `c5af6755`. The [Phase 7 progress
log](08-Phase-7-Feature-Migration.md#progress-log) records its acceptance
evidence and limits. Screenshot comparison and memory evidence, along with the
7A and Phase 7 exit gates, remain open.

Work through these ordered Batch 2 candidates:

17. **Accepted — Korean teacher sidebar directory** (`d6574628`). The matched
    visual evidence and focused verification are in the [Phase 7 progress
    log](08-Phase-7-Feature-Migration.md#progress-log) and [capture
    package](../../docs/qt-rewrite/visual-baseline/next/phase-7/korean-teacher-directory/README.md).
18. **Accepted — Shared Staff Directory page/model-view for Native English and
    GS Team** (`aab11f67`). It preserves separate typed store branches and
    exposes `refreshFromStore()` for the following import slice. See the
    [Phase 7 progress log](08-Phase-7-Feature-Migration.md#progress-log) and
    [capture package](../../docs/qt-rewrite/visual-baseline/next/phase-7/shared-staff-directory/README.md).
19. **Accepted — Normal teacher workbook import in the Next shell, separate
    from Initial Setup** (`6dedd431`). See the [Phase 7 progress
    log](08-Phase-7-Feature-Migration.md#progress-log) and [capture
    package](../../docs/qt-rewrite/visual-baseline/next/phase-7/normal-teacher-import/README.md).
20. **Active — Upcoming birthdays and daily dismissal in the Next shell.** This
    depends on reads for all three staff groups.

## Batch 3 — discovered (next)

Two independent discovery maps agree that post-#20 work covers the remaining
7B–7J vertical features; existing Next contracts and tests do not count as
migrated UI. Main selected the granular 20-slice sequence below to bound page
ownership, dependencies, lifecycle, and verification.

21. My Details and Signature.
22. My Classes with compact summaries and one selected detail; use model/view
    and compact resident state.
23. My Schedule and workspace event summary; exclude #20 birthdays and daily
    dismissal.
24. Classes list/navigation.
25. Class details, create/edit, and co-teachers.
26. Class notes.
27. Class analytics and speaking-evaluation defaults.
28. Testing classes and blocks.
29. Class transfer.
30. Schedule display/editing with model/view and compact state.
31. Schedule workbook import/review/apply; separate from #19 teacher import.
32. Testing assignments and schedule output/printing.
33. Calendar events, preferences, and month grid.
34. Calendar import.
35. Roster editor/student model with model/view and compact state.
36. Roster imports/transfers.
37. Roster templates/printing.
38. Speaking-evaluation core using model/view and compact state.
39. Evaluation analytics, AI-assisted comments, and batch workflow.
40. Reports, PDF, PowerPoint, and export jobs.

Keep My Classes, schedules, rosters, and evaluations model/view-backed with
compact resident state. Bound import, review, and output data; release it when
each stage completes.
The 7J.1 Sub Prep path remains a combined large-workspace/output acceptance
including 96 classes and 8 slots. Begin next-batch discovery when candidate
#39 starts. Search then for possible work beyond #40: Campus Directory;
Document catalog/viewer; 7J.1 Sub Prep; legacy `.db` compatibility import
evidence; global shell, preferences, permissions, and updater parity; and any
other remaining slices. No separate batch-history document is needed.
