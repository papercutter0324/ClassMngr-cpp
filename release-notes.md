# ClassMngr 0.9.0 Release Notes

ClassMngr 0.9.0 introduces Excel schedule import and makes the surrounding
workflows faster, clearer, and more dependable. This release also refines
schedule and roster printing, class setup, Sub Prep, and navigation throughout
the app.

## Import Schedules From Excel

- Import normal or intensive schedules from supported Excel (`.xlsx`)
  workbooks.
- Choose the worksheet and schedule section that belong to you, with a
  confirmation step when the spreadsheet name differs from My Information.
- Preview an imported schedule before applying any changes.
- Review imported Korean teachers, rooms, and classes in a redesigned,
  easier-to-scan reconciliation view. Reuse or update existing records, create
  new ones, or skip entries as needed.
- Improved matching helps ClassMngr reconcile imported schedule information
  with the records already in your data.
- Invalid or overlapping cells are identified so they can be reviewed and
  skipped explicitly when necessary.
- Spreadsheet imports now run in the background, keeping ClassMngr responsive
  while the workbook is being read.

## Schedule, Roster, And Class Improvements

- Schedule tables have clearer styling, and schedule printing can use Korean
  teachers' preferred English names.
- Fixed the **Hide empty rows** option so it consistently removes unused hours
  from schedule views and printed schedules.
- Improved the Schedule layout on macOS.
- The Roster print dialog is easier to work with on smaller screens, with a
  refined layout and vertical scrolling for its options.
- New classes no longer start with default regular or intensive meeting times.
- Sub Prep now provides a simpler grade and class selector.

## Interface Refinements

- Standardized tab bars and improved page layouts across the app.
- Improved button font styling on the Calendar, Roster, Classes, and Speaking
  Evaluation pages.

## Reliability And Polish

- Improved messages when importing teacher data whose version is older than or
  matches the data already in ClassMngr.
- Updated Korean and English translations.
- Improved Windows and macOS packaging workflows, including Windows ARM64
  release support.
