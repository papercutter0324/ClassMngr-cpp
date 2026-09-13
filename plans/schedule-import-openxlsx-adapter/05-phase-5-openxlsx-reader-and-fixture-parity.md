# Phase 5 — OpenXLSX Reader and Fixture Parity

**Previous:** [Phase 2](02-phase-2-winui-build-integration-spike.md), [Phase 4](04-phase-4-qt-free-schedule-format-interpreter.md)

**Next:** [Phase 6](06-phase-6-winui-dialog-integration.md)

## Goal

Implement the Windows-native `ScheduleWorkbookReader` with OpenXLSX and prove
that it produces the same canonical import model and diagnostic categories as
the Qt reference for real `.xlsx` fixtures.

## Scope

- Open an XLSX document read-only through the Phase 2 dependency bridge.
- Map worksheets, non-empty cells, merged ranges, notes where supported, and
  style facts into `ScheduleWorkbookLayout`.
- Feed the shared Phase 4 interpreter.
- Translate OpenXLSX/library exceptions to reader error types.
- Add true on-disk XLSX fixtures and compare native and Qt results.

## Non-Goals

- Wiring results into the WinUI dialogs.
- Saving, repairing, or rewriting an XLSX file.
- Treating partially decoded data as valid when document open or required
  traversal fails.

## Adapter Rules

- The public reader header contains no OpenXLSX types; include OpenXLSX only in
  implementation files.
- Keep an `XLDocument` alive only while mapping to independent value objects.
- Iterate existing cells without creating missing cells or modifying workbook
  state. Never call save on imported documents.
- Convert library strings to the project’s canonical UTF-8 representation at a
  single, tested boundary.
- Preserve worksheet name and visibility. The interpreter, rather than the
  adapter, decides which visible sheets are compatible schedules.
- Normalize merged ranges and styles according to the Phase 3 contract.

## Style Compatibility Gate

Schedule templates use formatting as data. Before calling the adapter complete,
verify what OpenXLSX reports for:

- direct RGB fills and font colors;
- indexed colors;
- theme colors and tints;
- absent style/fill/font values;
- bold state;
- merged-cell style/value placement.

If an API cannot expose a needed style fact, choose an explicit response:
extend the adapter using safely available underlying data, adjust the shared
format contract with a tested compatibility rule, or declare that template
variant unsupported with an actionable diagnostic. Do not substitute guessed
colors.

## Fixture Catalogue

Commit or otherwise securely provision small, copyright-safe `.xlsx` fixtures
whose expected canonical results are checked in. At minimum include:

| Fixture | Required evidence |
| --- | --- |
| regular multi-sheet workbook | visible schedule selection, Korean names, rooms, classes, colors, meetings |
| hidden-sheet workbook | hidden and very-hidden sheets do not become selectable schedules |
| merged/style workbook | merged headers and formatting-driven interpretation match Qt |
| intensive workbook | intensive slot/time conventions and import kind behavior match |
| Korean path/content workbook | Unicode path and cell-content handling is correct |
| malformed layout workbook | stable validation failure and no import model |
| corrupt/non-XLSX input | safe reader failure with no crash or fallback data |

Where a test needs a temporary workbook, retain that test; it does not replace
the committed cross-reader fixture catalogue.

## Work Breakdown

1. Establish the native reader implementation location and compile it through
   the proven WinUI integration route.
2. Implement document opening and top-level error mapping.
3. Map sheet metadata, cells, and merged ranges into raw layout values.
4. Map styles and validate the style compatibility gate.
5. Invoke the shared interpreter and map its diagnostic result to the reader
   contract.
6. Build a canonical comparator for Qt and native `ScheduleImportWorkbook`
   outputs and diagnostics.
7. Add the fixture catalogue and run every fixture through both paths.
8. Add targeted tests for cancellation/discard semantics at reader boundaries.

## Expected File Areas

- new WinUI/platform reader implementation and test support
- the shared format component from Phase 4
- native-reader tests and committed XLSX fixture location
- Windows build integration files only if Phase 2 revealed a missing final
  linkage requirement

## Validation

- Native reader succeeds on every supported valid fixture.
- Qt and native paths yield equal canonical models and diagnostic categories on
  supported fixtures.
- Malformed/corrupt fixtures produce safe errors, no crashes, and no output
  import data.
- Reader tests run through the actual configured WinUI/native build route.
- A debugger or instrumentation check confirms no document save/write is
  performed during reading.

## Exit Criteria

The native reader is a demonstrated substitute for the Qt reader for the
fixture catalogue. A concrete `ScheduleImportWorkbook`, rather than fallback
data, is available to Phase 6.

## Risks and Responses

| Risk | Response |
| --- | --- |
| Qt and OpenXLSX expose different workbook defaults | Normalize in adapters; keep the interpreter library-neutral. |
| OpenXLSX cannot decode an important template style | Block UI rollout until the compatibility rule is proven or a supported-template limitation is accepted. |
| Fixtures accidentally test only happy paths | Require at least one corrupted input and one structurally valid but incompatible layout. |
