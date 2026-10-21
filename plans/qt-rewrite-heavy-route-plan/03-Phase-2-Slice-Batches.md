# Phase 2 Slice Batches

This is the canonical register for Phase 2 ordered slice batches, candidate
status, and discovery reminders. Keep live batch lists here, not in the phase
plan or progress log. Use the [Phase 2 progress log](03-Phase-2-Progress-Log.md)
for dated discovery evidence, full acceptance criteria, verification results,
and chronology.

## Batch533 — Closed

- F1301 — Complete at implementation commit `eeb68067`.
- No other slices were found.

## Batch534 — Closed locally; push pending

1. F1302 — Complete at implementation commit `09b2c1a5`. Technical
   teacher-choice read failure now preserves the saved/current teacher ID
   through unrelated-field saves; explicit `None` remains unassignment.

No other slices were found.

## Batch535 — Pushed through closeout

- F1303 — Complete at implementation commit `1f21e364`. CalendarPage now
  includes Technical first-day preference read failures in its existing
  localized, nonmodal notice; locale fallback and Calendar Preferences
  behavior remain unchanged.
- Closeout `6239d350` was pushed; `origin/Qt-Rewrite` advanced from
  `d2c2eabc`.

No other slices were found.

## Batch536 — Pushed through closeout

Batch536 discovery completed when F1303 implementation started. The five
ordered candidates below were accepted; no other slices or possible candidates
beyond the 20-candidate limit were found.

1. F1304 — Complete at implementation commit `31faba36`. Speaking Evaluation
   now reports automatic save failures nonmodally, retains dirty scores, clears
   feedback after recovery, and keeps successful autosaves quiet.
2. F1305 — Complete at implementation commit `4082c9d2`. Class Notes now
   reports autosave failures nonmodally while preserving the dirty draft and
   no-modal behavior; feedback clears on recovery.
3. F1306 — Complete at implementation commit `a9224b63`. Sub Prep now reports
   autosave failure nonmodally with status and tooltip, preserves the dirty
   draft, and clears status after a successful retry.
4. F1307 — Complete at implementation commit `8f7ddb9e`. Class Details now
   shows nonmodal feedback for automatic Technical write failures while
   retaining dirty edits and preserving validation and schedule-conflict
   behavior.
5. F1308 — Complete at implementation commit `7fa77d38`. Co-Teacher now
   reports automatic assignment-write failures while preserving selection and
   dirty state; feedback clears on recovery.

No other slices were found. See the progress log for verification and
discovery evidence.

Closeout `4312e1fe` was pushed; `origin/Qt-Rewrite` advanced from `9bf62a69`.

## Batch537 — Pushed through closeout

Batch537 discovery completed when F1305 implementation started. All three
candidates are complete. No other slices or leads beyond the 20-candidate
limit were found.

1. F1309 — Complete at implementation commit `122e2a12`. Staff Directory now
   shows localized nonmodal feedback for Technical autosave writes in both
   directory types; rows and dirty state remain, manual warnings are preserved,
   and feedback clears on recovery.
2. F1310 — Complete at implementation commit `c11525f1`. Teacher Info now
   gives inline feedback for automatic Technical update failure while
   retaining dirty values; retry and successful persistence clear the status.
3. F1311 — Complete at implementation commit `536e5dd3`. Testing Classes
   reports Technical autosave failures nonmodally, retains dirty values, and
   preserves manual warnings.

No other slices were found. See the progress log for verification and
discovery evidence.

Closeout `8c06b019` was pushed; `origin/Qt-Rewrite` advanced from
`4312e1fe`.

## Batch538 — Discovery complete; no candidates

Discovery at F1309 implementation start found no eligible candidate in either
lane. No other slices or leads beyond the 20-candidate limit were found. No
candidate is queued. Current Phase 2 work is to review and execute remaining
exit-gate evidence.
