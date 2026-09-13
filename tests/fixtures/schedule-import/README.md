# Schedule-import fixture catalogue

The native-reader test creates a small, copyright-safe `.xlsx` workbook on
disk for each run. This keeps the fixture source reviewable while still
exercising OpenXLSX against a real ZIP/XML workbook rather than a mocked
object graph.

`tests/openxlsx_schedule_workbook_reader_tests.cpp` covers the current Phase 5
catalogue:

- a visible schedule sheet plus hidden and very-hidden sheets;
- Korean cell content, a Unicode filename, rooms, merged cells, and direct RGB
  fill/font styles;
- normal and intensive schedule interpretation;
- unchanged file size and timestamp after reading;
- cancellation, a structurally valid but incompatible workbook, a non-XLSX
  file, and a corrupt `.xlsx` input.

The fixture deliberately uses OpenXLSX to write the temporary workbook and the
native reader to read it. No imported workbook is saved or rewritten by the
reader.
