# Phase 4 — Qt-Free Schedule-Format Interpreter

**Previous:** [Phase 3](03-phase-3-reader-contract-and-format-boundary.md)

**Next:** [Phase 5](05-phase-5-openxlsx-reader-and-fixture-parity.md)

## Goal

Move schedule spreadsheet semantics out of the Qt-specific reader path into a
Qt-free interpreter fed by the raw workbook layout contract, while preserving
current Qt behavior through regression tests.

## Why It Matters

`src/features/schedule/import/schedule_workbook_parser.cpp` currently combines
two responsibilities: it understands schedule conventions and it consumes the
Qt workbook representation produced by
`src/features/calendar/calendar_workbook_reader.cpp`. Porting the whole file
to WinUI would create two parsers that inevitably drift. This phase creates one
semantic implementation that both readers can use.

## Scope

- Extract the workbook-layout rules into a Qt-free component.
- Adapt the current Qt decoded workbook into `ScheduleWorkbookLayout`.
- Preserve existing output models, warnings, errors, and selected-workbook
  behavior unless a documented bug fix is accepted separately.
- Add focused raw-layout and Qt adapter regression tests.

## Non-Goals

- OpenXLSX file access or WinUI dialog wiring.
- Changing the schedule workbook template.
- Simplifying behavior by silently ignoring layouts that Qt currently supports.

## Behavior Inventory

The extraction plan must retain and test the currently supported rules,
including:

- visible versus hidden/very-hidden worksheets;
- worksheet selection and schedule/user identification;
- merged headers and their effective value ownership;
- weekday headings in supported Korean/English forms;
- regular and intensive schedule type detection;
- time parsing, including noon/afternoon and intensive-slot conventions;
- Korean teacher, room, grade, level, and class-name parsing;
- aggregation of class occurrences into meeting patterns;
- fill/font/bold style interpretation, including colors used by the template;
- ignored cells, malformed rows, recoverable warnings, and fatal diagnostics;
- user/group blocks and any ordering assumptions used by the UI.

## Implementation Shape

1. Keep codec adapters responsible only for mapping their workbook API into
   `ScheduleWorkbookLayout`.
2. Give the interpreter only value types and standard/project dependencies.
3. Return the existing engine import model plus stable diagnostics.
4. Make all text encoding conversions explicit and UTF-8-safe at adapter
   boundaries.
5. Do not introduce Qt containers, `QString`, or `QColor` into the new shared
   interpreter public headers.

## Work Breakdown

1. Characterize the Qt parser with tests before moving logic. Capture both
   successes and currently intentional failures.
2. Introduce the raw layout fixtures/builders needed to express cells, styles,
   merges, and sheet visibility without creating an XLSX file.
3. Extract pure helpers first: cell text normalization, time parsing, weekday
   recognition, name/room/class parsing, color comparison, and diagnostics.
4. Extract worksheet traversal and class aggregation next.
5. Replace the Qt parser’s direct semantic work with the Qt-layout adapter plus
   the shared interpreter.
6. Compare old and new Qt parser results on the fixture catalogue. Resolve
   intentional deviations as named compatibility decisions, never as silent
   test weakening.

## Test Strategy

Use two complementary test levels:

- **Raw layout tests:** precise unit tests for merges, styles, time values,
  hidden sheets, malformed cells, and diagnostics.
- **Qt compatibility tests:** existing and new Qt-created/read workbooks must
  produce the same normalized `ScheduleImportWorkbook` and diagnostic classes
  before and after extraction.

Canonical comparison should ignore nondeterministic presentation details but
must compare selected sheet/user choices, classes, meetings, colors where they
are domain-significant, and error/warning classification.

## Expected File Areas

- `src/features/schedule/import/schedule_workbook_parser.cpp`
- `src/features/calendar/calendar_workbook_reader.cpp`
- a new shared schedule-format component and its tests
- `tests/schedule_import_tests.cpp` and related fixture/build helpers

Final names and placement should follow existing feature-module conventions;
this plan deliberately does not mandate a premature directory refactor.

## Validation

- Existing schedule import tests remain green.
- New raw-layout tests cover the behavior inventory above.
- Qt adapter outputs match the pre-extraction baseline for every canonical
  fixture.
- The shared interpreter compiles without Qt linkage or Qt types in public
  headers.

## Exit Criteria

An OpenXLSX adapter can be implemented by mapping workbook facts to the raw
layout model, without reimplementing any schedule-layout rules. The Qt importer
has become an adapter plus the same shared interpreter, preserving its behavior.

## Main Risks

| Risk | Mitigation |
| --- | --- |
| Style semantics are implicit in Qt APIs | Create fixtures covering direct, indexed, theme, and tint-derived colors before declaring parity. |
| Extraction changes diagnostic wording/order | Compare stable diagnostic code/category and relevant location; retain wording where it is user-visible. |
| Broad refactor obscures regressions | Move pure helpers and traversal incrementally with tests after each boundary. |
