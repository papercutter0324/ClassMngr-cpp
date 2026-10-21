# Phase 2 Slice Batches

This is the canonical register for Phase 2 ordered slice batches, candidate
status, and discovery reminders. Keep live batch lists here, not in the phase
plan or progress log. Use the [Phase 2 progress log](03-Phase-2-Progress-Log.md)
for dated discovery evidence, full acceptance criteria, verification results,
and chronology.

## Batch533 — Closed

- F1301 — Complete at implementation commit `eeb68067`.
- No other slices were found.

## Batch534 — Active; implementation underway

1. F1302 — Implementation underway. Preserve assigned Testing Classes
   teachers when a Technical teacher-choice read failure leaves only the `None`
   fallback, so an unrelated field save cannot clear the assignment. Verify
   against the progress-log acceptance criteria.

No other slices were found.

## Batch535 — Discovered; one accepted candidate

1. F1303 — Accepted for Batch535 after Batch534 closes. Show the existing
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
