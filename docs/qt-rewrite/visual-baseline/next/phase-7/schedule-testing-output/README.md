# Phase 7 Schedule testing assignments and output captures

Task ID: `phase7-c32-testing-assignments-output-20261028`.

`schedule_testing_output_manifest.json` indexes eight assignment editor captures (Legacy and Next × English and Korean × light and dark), four Next output-options captures, eight paired #28 pending-slot manager captures, and one representative rendered Schedule PDF image. The editor is in Testing Class mode so the choices and Manage Classes handoff action are visible. The manager screenshots are reused from `../testing-classes/`; the Schedule page tests exercise the handoff with an unsaved editor draft and assert that it performs no schedule mutation.

The representative PDF shows a plain Oral Testing block with its Library room and a Writing Lab testing-class card with grade, level, and room. The source PDF is generated into an operation-local `QTemporaryDir`; the capture manifest retains only a PNG preview. The PDF is one landscape A4 page (842 × 595 points). `ClassMngrSchedulePrintPdfTests` separately checks both orientations, A4 page dimensions, half-inch content margins, styles, assignment text, and testing markers.

## Observed differences

- Legacy uses `DialogShell` and labels its action `Manage Classes`; Next uses a regular `QDialog` and labels the same handoff `Manage Testing Classes`.
- Both editors show the same Testing Class choice fields. Legacy retains the pending weekday and start internally; Next shows the pending slot in the #28 manager. The paired manager captures record this display difference.
- The Next output dialog lays style choices in two columns and orientation choices in one column. Current Appearance and Landscape are selected in the captures; page tests exercise Light, Dark, Excel, Portrait, Print, PDF chooser cancel, and save outcomes.
- Qt widget and glyph rasterization differ between Legacy and Next. These captures document visual states and do not claim pixel parity.

The dialog images use Qt Fusion with explicit light/dark palettes at native dialog bounds. The PDF preview is rasterized at 150 DPI for inspection; actual output uses the Schedule PDF writer's 300 DPI settings. The output request, printer/PDF objects, and source PDF buffer are operation-scoped. No memory budget is inferred from the captures.

Generate the artifact set with:

```sh
QT_QPA_PLATFORM=offscreen QT_SCALE_FACTOR=1 \
  build/macos-clang-debug/ClassMngrNextScheduleVisualCapture --c32 \
  docs/qt-rewrite/visual-baseline/next/phase-7/schedule-testing-output
```
