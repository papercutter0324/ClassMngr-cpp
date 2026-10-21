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

## Batch535 — Active; one accepted candidate

1. F1303 — Accepted before implementation. Show the existing
   localized, nonmodal CalendarPage notice when a Technical first-day
   preference read fails, while preserving the locale fallback and current
   Calendar Preferences behavior. Keep successful missing/unavailable fallback
   quiet and clear the notice on provider recovery. Preserve F1301 behavior.
   See the progress log for full acceptance criteria and discovery evidence.

No other slices were found.

## Next discovery

Batch536 discovery is due when F1303 implementation starts, following the
one-candidate trigger. Search for up to 20 ordered candidates. No possible
candidates beyond the 20-candidate limit were found in Batch535; include any
newly found candidates and revisit any such reminders when discovery starts.
