# Current Slice Batches

## Batch 3 — active

Two independent discovery maps agree that post-#20 work covers the remaining
7B–7J vertical features; existing Next contracts and tests do not count as
migrated UI. Main selected the granular 20-slice sequence below to bound page
ownership, dependencies, lifecycle, and verification.

21. **Accepted — My Details and Signature** (`060aca5c`). See the [Phase 7
    progress log](08-Phase-7-Feature-Migration.md#progress-log) and [capture
    package](../../docs/qt-rewrite/visual-baseline/next/phase-7/my-details-signature/README.md).
22. **Accepted — My Classes** (`f7a3d81c`). See its [Phase 7 progress
    log](08-Phase-7-Feature-Migration.md#progress-log) and [capture
    package](../../docs/qt-rewrite/visual-baseline/next/phase-7/my-classes/README.md).
23. **Accepted — My Schedule and workspace event summary.** See its [Phase 7
    progress log](08-Phase-7-Feature-Migration.md#progress-log) and [capture
    package](../../docs/qt-rewrite/visual-baseline/next/phase-7/my-schedule-workspace-events/README.md).
24. **Accepted — Classes list/navigation** (`368fcee8`). See its [Phase 7
    progress log](08-Phase-7-Feature-Migration.md#progress-log) and [capture
    package](../../docs/qt-rewrite/visual-baseline/next/phase-7/classes-navigation/README.md).
25. **Accepted — Class details, create/edit, and co-teachers** (`c0cb9c93`).
    See its [Phase 7 progress log](08-Phase-7-Feature-Migration.md#progress-log)
    and [capture package](../../docs/qt-rewrite/visual-baseline/next/phase-7/class-details-coteacher/README.md).
26. **Accepted — Class notes and time filler** (`833e9074`). See the [Phase 7
    progress log](08-Phase-7-Feature-Migration.md#progress-log) and [capture
    package](../../docs/qt-rewrite/visual-baseline/next/phase-7/class-notes/README.md).
27. **Accepted — Class analytics and speaking-evaluation defaults** (`8f38ae44`).
    See the [Phase 7 progress log](08-Phase-7-Feature-Migration.md#progress-log)
    and [capture package](../../docs/qt-rewrite/visual-baseline/next/phase-7/class-analytics/README.md).
28. **Accepted — Testing classes and blocks** (`9bdbe3aa`). See the [Phase 7
    progress log](08-Phase-7-Feature-Migration.md#progress-log) and [capture
    package](../../docs/qt-rewrite/visual-baseline/next/phase-7/testing-classes/README.md).
29. **Accepted — Class Transfer** (`56455a1b`). See the [Phase 7 progress
    log](08-Phase-7-Feature-Migration.md#progress-log) and [capture
    package](../../docs/qt-rewrite/visual-baseline/next/phase-7/class-transfer/README.md).
30. **Accepted — Schedule display/editing** (`894a11ec`). See the [Phase 7
    progress log](08-Phase-7-Progress-Log.md) and [capture package](../../docs/qt-rewrite/visual-baseline/next/phase-7/schedule-display-editing/README.md).
31. **Accepted — Normal Schedule workbook import/review/apply** (`c1166d79`).
    See the [Phase 7 progress log](08-Phase-7-Progress-Log.md) and [capture
    package](../../docs/qt-rewrite/visual-baseline/release/normal-schedule-import-next/README.md).
32. **Accepted — Testing assignments and Schedule output/printing** (`51faa658`).
    See the [Phase 7 progress log](08-Phase-7-Progress-Log.md) and [capture
    package](../../docs/qt-rewrite/visual-baseline/next/phase-7/schedule-testing-output/README.md).
33. **Accepted — Calendar events, preferences, and month grid** (`21f2d98c`).
    See the [Phase 7 progress log](08-Phase-7-Progress-Log.md) and [capture
    package](../../docs/qt-rewrite/visual-baseline/next/phase-7/calendar/README.md).
34. **Accepted — Calendar import** (`8645d9db`). See the [Phase 7 progress
    log](08-Phase-7-Progress-Log.md) and [import-state captures](../../docs/qt-rewrite/visual-baseline/next/phase-7/calendar/import/README.md).
35. **Accepted — Roster editor/student model with model/view and compact state**
    (`9da01020`). See the [Phase 7 progress log](08-Phase-7-Progress-Log.md)
    and [roster-editor presentation
    captures](../../docs/qt-rewrite/visual-baseline/next/phase-7/roster-editor/README.md).
36. **Accepted — Roster score import and student transfer** (`b0df9112`). See
    the [Phase 7 progress log](08-Phase-7-Progress-Log.md) and [import/transfer
    captures](../../docs/qt-rewrite/visual-baseline/next/phase-7/roster-import-transfer/README.md).
37. **Accepted — Roster templates/printing** (source `098f1de7`, follow-up
    `1214b569`). See the [Phase 7 progress log](08-Phase-7-Progress-Log.md)
    for acceptance evidence and limits.
38. **Accepted — Speaking-evaluation core using model/view and compact state**
    (`5b0274d9`). See the [Phase 7 progress log](08-Phase-7-Progress-Log.md)
    and [capture package](../../docs/qt-rewrite/visual-baseline/next/phase-7/speaking-evaluation-core/README.md).
39. **Accepted — AI-assisted Speaking Evaluation comments and batch workflow**
    (`02fbeca6`). Class analytics remain in #27; see the [C39 capture package](../../docs/qt-rewrite/visual-baseline/next/phase-7/speaking-evaluation-ai-comments-c39/README.md).
40. **Accepted — Reports, PDF, PowerPoint, and export jobs** (`f2b24bd7`). See
    the [Phase 7 progress log](08-Phase-7-Progress-Log.md) and [C40 capture
    package](../../docs/qt-rewrite/visual-baseline/next/phase-7/speaking-evaluation-reports-c40/README.md).

Keep My Classes, schedules, rosters, and evaluations model/view-backed with
compact resident state. Bound import, review, and output data; release it when
each stage completes. Candidate #31 schedule workbook import is distinct from
#19 teacher workbook import. The 7J.1 Sub Prep path remains a combined
large-workspace/output acceptance including 96 classes and 8 slots. Batch 3 is
complete locally and awaits its push; do not archive it in this tracker before
the batch is pushed. No separate batch-history document is needed. The
generated schema-v6 `.db` relationship/output check from
`representative_startup.sql` passed as synthetic Phase 7 exit evidence; it is
not a historical user profile or a test-only candidate. The Initial Setup personal-details
screenshot subgate is closed; broader 7A visual evidence, memory evidence, the
Windows packaged-Next memory gate, and the 7A/Phase 7 exit gates remain open.

## Batch 4 — discovered; queued

Two independent maps agree on this ordered list:

41. **Discovered — Campus Directory:** campus content, directions, address,
    housing, maps, and permitted editing.
42. **Discovered — Document catalog.**
43. **Discovered — Document viewer lifecycle/actions:** load, save/copy,
    zoom/navigation, release, and reopen.
44. **Discovered — 7J.1 Sub Prep:** large-workspace class information and
    output.
45. **Discovered — General app-wide Preferences UI/menu.**
46. **Discovered — Admin and actual permission-gate parity.**
47. **Discovered — User-initiated update workflow:** startup updater work is
    already owned by Phase 5.
48. **Discovered — Useful Links external navigation.**

No other slices were found.
