# Current Slice Batches

## Batch 2 — active

Batch 1 is complete and pushed at `c5af6755`. The [Phase 7 progress
log](08-Phase-7-Feature-Migration.md#progress-log) records its acceptance
evidence and limits. Screenshot comparison and memory evidence, along with the
7A and Phase 7 exit gates, remain open.

The two independent discovery maps found no other eligible Teacher/Staff
slices. Work through these ordered candidates:

17. **Accepted — Korean teacher sidebar directory** (`d6574628`). The matched
    visual evidence and focused verification are in the [Phase 7 progress
    log](08-Phase-7-Feature-Migration.md#progress-log) and [capture
    package](../../docs/qt-rewrite/visual-baseline/next/phase-7/korean-teacher-directory/README.md).
18. **Accepted — Shared Staff Directory page/model-view for Native English and
    GS Team** (`aab11f67`). It preserves separate typed store branches and
    exposes `refreshFromStore()` for the following import slice. See the
    [Phase 7 progress log](08-Phase-7-Feature-Migration.md#progress-log) and
    [capture package](../../docs/qt-rewrite/visual-baseline/next/phase-7/shared-staff-directory/README.md).
19. **Active — Normal teacher workbook import in the Next shell, separate from
    Initial Setup.** Preserve review/transaction/version prompts and refresh of
    profile, list, and directories.
20. Upcoming birthdays and daily dismissal in the Next shell. This depends on
    reads for all three staff groups.

Begin discovering the next batch when candidate #19 starts, per the phase
batch rule.
