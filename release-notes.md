# ClassMngr 0.10.0 Release Notes

ClassMngr 0.10.0 expands schedule importing, adds preferred teacher names, and
introduces a more flexible document system. It also improves theme consistency
and release support across platforms.

## Documents

- Reworked the Documents area around a modular, JSON-driven catalog.
- Made document navigation easier to extend and update through resource packs.

## Schedule Import

- Expanded spreadsheet import support to handle more schedule layouts and
  class-matching cases.
- Added stronger conflict detection and clearer warnings before imported
  changes are applied.
- Improved the review experience for ambiguous classes, unknown cells, and
  overlapping schedule data.

## Teachers

- Added preferred names for teachers and carried them through the relevant
  class, schedule, import, and transfer workflows.

## Appearance and Platform Support

- Improved light-mode styling in the schedule import review dialog.
- Updated file dialog icons to follow the selected theme and render
  consistently on Windows.
- Added and refined release workflows for Windows x64, Windows ARM64, Linux,
  and universal macOS builds.
