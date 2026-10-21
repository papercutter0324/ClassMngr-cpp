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

## Batch536 — Closed locally; push pending

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

## Batch537 — Next after Batch536 push; three accepted candidates

Batch537 discovery completed when F1305 implementation started. The candidates
below are accepted for activation after Batch536 is pushed; no other slices or
leads beyond the 20-candidate limit were found.

1. F1309 — Staff Directory autosave failure feedback. Show localized,
   nonmodal feedback for Technical autosave writes in both directory types;
   retain rows and dirty state, preserve manual warnings, and clear feedback
   on recovery.
2. F1310 — Teacher Info automatic update failure feedback. Disclose automatic
   update failures nonmodally while retaining editable dirty values; preserve
   manual warnings and clear feedback after successful save and reload.
3. F1311 — Testing Classes autosave failure feedback. Preserve manual warnings;
   show visible nonmodal feedback for Technical autosave failures and retain
   dirty values. Confirm the no-modal autosave policy applies; Batch530's
   safeguards do not distinguish autosave.

See the progress log for discovery evidence, source references, and overlap
screening. Batch538 discovery is due when F1309 implementation starts.
