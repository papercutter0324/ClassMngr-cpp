# ClassMngr 0.18.0 Release Notes

ClassMngr 0.18.0 reduces memory use by loading pages and resources on demand,
makes roster editing more flexible, and improves validation for Korean student
names.

## Performance and Resource Loading

- Reduced memory use by creating larger pages and views only when they are
  needed.
- Updated resource loading so application resources are loaded dynamically as
  required.

## Rosters and Speaking Evaluations

- Roster tables now support selecting multiple cells.
- Korean student names with one or five or more syllables are clearly marked
  for verification.
- Saving a roster or speaking evaluation with a questionable Korean name now
  gives you the opportunity to verify it or save it anyway.

## App Updates

- Improved startup handling so an automatic update prompt stays visible above
  the main window while the app finishes opening.
