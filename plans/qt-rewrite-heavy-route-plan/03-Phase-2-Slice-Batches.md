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

## Batch536 — Active; five accepted candidates (F1304 first)

Batch536 discovery completed when F1303 implementation started. The five
ordered candidates below were accepted; no other slices or possible candidates
beyond the 20-candidate limit were found.

1. F1304 — Speaking Evaluation autosave failure feedback. Show localized,
   nonmodal feedback after an automatic write failure; retain dirty scores,
   clear feedback after recovery, and keep successful autosaves quiet.
2. F1305 — Class Notes autosave failure feedback. Show nonmodal automatic
   save feedback while preserving the draft and no-modal behavior; clear it on
   recovery.
3. F1306 — Sub Prep settings autosave failure feedback. Report failed
   autosaves nonmodally, preserve the draft, and clear feedback after a
   successful save.
4. F1307 — Class Details automatic Technical write feedback. Report automatic
   Technical write failures nonmodally while retaining dirty edits and
   preserving validation and schedule-conflict behavior.
5. F1308 — Co-Teacher autosave failure feedback. Report ordinary automatic
   assignment-write failures while preserving selection and dirty state; clear
   feedback on recovery.

See the progress log for discovery evidence and source references. F1304 is
the first candidate in Batch536. Batch537 discovery is due when F1304
implementation starts, following the one-slice trigger.
