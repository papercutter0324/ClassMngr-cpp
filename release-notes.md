# ClassMngr 0.9.0 Release Notes

ClassMngr 0.9.0 makes it much easier to bring an existing schedule into the
app, while refining schedule printing, class setup, and release reliability.

## Import Schedules From Excel

- Import normal or intensive schedules directly from supported Excel (`.xlsx`)
  workbooks.
- Choose the worksheet and the schedule section that belongs to you, with a
  confirmation step when the spreadsheet name differs from My Information.
- Preview the imported schedule before making changes.
- Review every imported Korean teacher, room, and class before applying the
  import. You can reuse or update an existing record, create a new one, or
  skip it.
- The importer identifies invalid or overlapping cells so you can review and
  explicitly skip them when needed.

## Schedule And Class Improvements

- Schedule printing can now use Korean teachers' preferred English names.
- Fixed the **Hide empty rows** option so it consistently removes unused hours
  from schedule views and printed schedules.
- Improved the Schedule layout on macOS.
- New classes no longer start with default regular or intensive meeting times.

## Reliability And Polish

- Improved messages when importing teacher data whose version is older than or
  matches the data already in ClassMngr.
- Updated Korean and English translations.
- Improved Windows and macOS packaging workflows, including Windows ARM64
  release support.
