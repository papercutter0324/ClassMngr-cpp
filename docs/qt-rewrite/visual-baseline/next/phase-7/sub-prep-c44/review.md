# C44 Sub Prep capture review

Task ID: `phase7_c44_subprep_implementation_01`.

Captures were taken at 800×600 with the offscreen Qt platform. The four
selected-state captures were reviewed against the retained English/Korean and
Light/Dark legacy references in this directory's manifest. They document the
route's structure and visible behavior; they do not claim pixel parity. The
mode controls show their localized light/dark combo arrows. The capture and
Next-shell route tests assert that both SVG resource paths exist.

| Variant | Next capture | Reviewed state |
| --- | --- | --- |
| English / Light | `selected-english-light.png` | English mode/day/action labels and the selected E4 class are visible. |
| English / Dark | `selected-english-dark.png` | The same selected state and controls remain legible on the dark palette. |
| Korean / Light | `selected-korean-light.png` | Mode, days, actions, detail labels, and preparation fields are translated. |
| Korean / Dark | `selected-korean-dark.png` | Korean labels remain translated and readable on the dark palette. |

The Legacy references include the main menu and feature navigation. The Next
route is hosted by the Next shell and contains one compact class list plus one
reusable selected-class detail panel. At the captured size, the summary text is
clipped to the list width; the complete navigation summary is available as a
tooltip, and the selected class's schedule and remaining fields are available
in the vertically scrollable detail panel. The selected state retained one
summary model, one detail panel, six text editors, and one model row per
projected visible class
(24 rows for the active grade, 96 across the four grade tabs). The capture
reported 105 page widgets. The shell lifecycle test separately verified
selection, scope refresh, output cancellation with no output directory and
retained selection/detail, the fixture's expected output conflict, leaving,
and re-entry.

The four `empty-*.png` captures show an empty selection and localized
no-scheduled-class status. `output-conflict-en-light.png` records the unchanged
96-class fixture's expected Monday 4:00 PM overlap rejection; no package folder
was created. A separate success route used
`tests/fixtures/workspaces/representative_startup.sql` (six visible summaries)
and produced nonempty `Sub Prep.pdf` and `Rosters - By Day.pdf` in
`generated-package-success/`.

The JSON manifest records model, active-grade row, detail, editor, widget,
query-count, and process-memory observations for each run. Memory samples are
process-wide snapshots before the page, while it is active, and after it is
suspended; they are not per-page allocation measurements. No Phase 9 memory
threshold was applied.

## Validation history

The focused verification commands were:

```text
ctest --test-dir build/macos-clang-debug --output-on-failure -R '^ClassMngrNext.*SubPrep|^NextApplicationSubPrepVacationSelectionPolicy$' --parallel 1
ctest --test-dir build/macos-clang-debug --output-on-failure -R '^(ClassMngrNextBootstrapTests|ClassMngrNextSubPrepGatewayTests|ClassMngrNextSubPrepVisualCapture|ClassMngrSubPrepClassInformationListModelTests)$' --parallel 1
ctest --test-dir build/macos-clang-debug --output-on-failure -R '^(ClassMngrNextScheduleImportWorkflowTests|ClassMngrNextPlatformApplicationServicesScheduleImportStateSnapshotPortTests|ClassMngrNextPlatformApplicationServicesScheduleImportApplyPortTests|ClassMngrNextNormalScheduleImportIntegrationTests)$' --parallel 1
ctest --test-dir build/macos-clang-debug --output-on-failure -R '^(ClassMngrSubPrepPrintPdfTests|ClassMngrSubPrepRosterOutputParityTests|ClassMngrSubPrepPackageServiceTests|SubPrepPackageServiceClassIdsForDaysProjection)$' --parallel 1
python3 scripts/phase4/generate_resource_inventory.py --check
git diff --check
```

The first three selections passed 25/25, 4/4, and 4/4 tests respectively.
The package/PDF selection passed 3/4. `ClassMngrFontManagerTests` also passed
1/1, and the `ClassMngr`, `ClassMngrNext`, and
`ClassMngrNextResourcePackage` build targets completed.

The focused Next route, Gateway, visual capture, and Schedule Import workflow,
port, and integration checks passed. The package/PDF CTest selection passed
three of four tests. The remaining synthetic heavy-data legacy PDF reference
case generated the same 19-page document but placed the `E2 – Level 1` schedule
entry on page 2 instead of page 1; its strict per-page text comparison failed
at character 1180. The entry remains in the generated document. This rendering
comparison is separate from the retained `large_startup.sql` route's expected
overlap rejection. No assertion or legacy reference was changed.

`ClassMngrSubPrepPageTests` was interrupted after more than two minutes at
`SubPrepPageTests::unavailablePreferenceLoadsPreservePageValues`. An isolated
offscreen run of that same slot remained active for 38.4 seconds after emitting
the Qt `propagateSizeHints()` warning and was interrupted. Neither run is
counted as a pass. That legacy target was not rerun during the follow-up.

## Follow-up validation

CMake reconfiguration and the affected Debug targets completed in
`build/macos-clang-debug`, including the Next resource package. The resource
inventory passed `python3 scripts/phase4/generate_resource_inventory.py
--check`.

`ClassMngrNextSubPrepVisualCapture` passed and regenerated the English/Korean ×
Light/Dark captures. The focused Next-shell lifecycle function and Gateway
failure function passed, including Cancel with no output directory and a
retained selection/detail, lifecycle release/re-entry, and the closed detail
read's exact `Technical` error code. Schedule Import workflow and normal route
integration CTests passed (2/2); state snapshot and apply port suites passed
(8/8 and 9/9). `ClassMngrSubPrepPrintPdfTests` and
`ClassMngrSubPrepRosterOutputParityTests` passed (1/1 each).

The large-package reference test now matches all 19 pages of `Sub Prep.pdf`,
with `Teacher 05 405 / E2 – Level 1` together on page 1. The package test then
fails on the second file, `Rosters - Daily.pdf`, page 1 character 66: generated
PDF text extraction reads `Student 1#1`, while the retained PDF reads
`Student 1?1`. Both roster PDFs have 16 pages, and their rendered first pages
show the same visible `Student 1-1` text. `ClassMngrSubPrepPackageServiceTests`
passed 13 of 14 QtTest cases; the only failing case is
`largePackageGeneratesOutputReferenceWhenConfigured`. The strict comparator,
reference files, and `large_startup.sql` fixture remain unchanged. This PDF
text-extraction difference needs a main-agent decision before C44 package
acceptance. The CTest output is in
`build/macos-clang-debug/Testing/Temporary/LastTest.log`; generated PDFs are
retained in `/tmp/c44-final-parity/reference/` for inspection.

## Pretendard variable-axis repair

An isolated Qt 6.12 experiment using `FontManager::getKoreanFont(-1)` confirmed
that setting explicit `wght` axis values 400 and 700 makes `QPdfWriter` embed
separate Regular and Bold resources regardless of which face is drawn first.
C44 now sets the Regular axis on the operation font and changes the axis for
bold section labels. The daily roster PDF assigns Pretendard Regular to student
names and Pretendard Bold to section headings; the output regression test
asserts that both faces are embedded.

The fresh large-package CTest now passes its strict per-page text comparison
across all 16 roster pages. The candidate and reference have known unmapped
separator CIDs; no normalization was added, and this result does not establish
copy/search correctness. The pages match in dimensions at 1240×1754 rendered
at 150 DPI. Changed-pixel percentages range from 11.0517% to 12.2148%, so full
raster parity remains incomplete. See `font-axis-repair/README.md` and its raw
artifacts for embedded face names, page-level comparisons, experiment source,
generated PDFs, and test logs. The reference and comparator are unchanged.
