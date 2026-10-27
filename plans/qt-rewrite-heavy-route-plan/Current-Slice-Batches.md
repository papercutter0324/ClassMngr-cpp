# Current Slice Batches

## Batch 1 — complete; awaiting push

Candidates #1–15 are accepted and committed through `c9cdd421`. Batch 1 is
complete; push is pending. There is no active implementation candidate while
the push is pending. The [Phase 7 progress
log](08-Phase-7-Feature-Migration.md#progress-log) records acceptance evidence
and limits. Screenshot comparison and memory evidence, along with the 7A and
Phase 7 exit gates, remain open.

## Next batch — provisional Teacher/Staff proposals

Discovery is complete. These ordered proposals remain provisional; the first
implementation candidate (#16) starts only after Batch 1 is pushed. At that
point, advance the tracker to Batch 2:

1. Teacher profile page and navigation. Typed read/edit contracts exist; the
   Next shell still has placeholders.
2. Korean teacher list/navigation with New and Delete. Profile opening depends
   on #1; keep My Classes and class assignment out of this slice.
3. Shared Staff Directory page/model-view for Native English and GS Team data
   kinds. Reuse the page shell, but keep each kind's fields, IDs, validation,
   and read/write branches distinct and verify both.
4. Normal teacher workbook import in the Next shell. Initial Setup import is
   separate; preserve review, transaction, version prompts, and refresh of
   profile/list/directories.
5. Upcoming birthdays and daily dismissal in the Next shell. This depends on
   reads for all three staff groups.
