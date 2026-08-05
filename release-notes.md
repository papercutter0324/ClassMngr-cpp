# ClassMngr 0.13.0 Release Notes

ClassMngr 0.13.0 makes first-time setup clearer, introduces the new ClassMngr
database file format, and simplifies sharing batches of speaking-evaluation
reports.

## Getting Started

- Added a Getting Started panel when no database is open.
- The panel guides new users through creating or opening a database, adding or
  importing Korean teachers, creating classes, and then adding schedules and
  rosters.

## ClassMngr Database Files

- New databases, saved copies, and exports now use the `.tps` ClassMngr
  Database format.
- Existing legacy `.db` databases can still be opened.
- On Windows, the installer can associate `.tps` files with ClassMngr so they
  open directly from File Explorer.

## Speaking-Evaluation Reports

- Exporting reports for multiple students now creates a single ZIP archive by
  default, making the reports easier to share.
- Added an option to also keep the individual PDF files when creating a ZIP
  archive.
- The built-in report template is now the default and recommended report
  generator.
- PowerPoint remains available as a fallback when the built-in generator does
  not work.
