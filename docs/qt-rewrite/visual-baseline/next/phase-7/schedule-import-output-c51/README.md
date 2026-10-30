# Phase 7 Candidate 51 Schedule Import and Output Evidence

Evidence task: `phase7_c51_schedule_import_output_implementation_01`.

## Import evidence

`tests/fixtures/imports/schedule_import_c51_conflict_free_5_blocks.xlsx` is the
reproducible repository fixture. Its SHA-256 is
`2975f63c860113a7eecc7f1ba3dba965d9f68b88e0938c7715a4dc2009c3d742`. The
production workbook parser reads 96 E6 candidates across five instructor
blocks of 20, 20, 20, 20, and 16 candidates.

`block-apply-evidence.json` records a separate apply and temporary workspace
for each block. Each apply created the recorded classes, class-info rows, and
schedule rows, then reloaded those entries on the Schedule page. This is five
block-level applies totaling 96 candidates; no single 96-candidate transaction
is claimed. Each apply replaces the class-times snapshot. The integration
tests also verify that the overlap workbook remains blocked during review and
check localized success, conflict, and failed-apply messages, including the
technical detail in the failure summary.

## Schedule output evidence

The capture applies block 2 and reloads its data from a temporary workspace;
the workspace file is not retained. The Next PDF is saved through the
production Next Schedule-page output action. For the Legacy PDF, the test builds
a document with `ScheduleBuilder`, adapts it with the Legacy print-document
adapter, and saves it through the Legacy Schedule PDF service. This does not
exercise a Legacy Schedule widget. Both documents use the same reloaded data and
selected output settings. No physical print was performed.

`output-evidence.json` records one landscape A4 page per route (842 × 595
points) and checks the profile name and four E6 course labels in both PDFs.
The representative first pages rendered with QtPdf at 150 DPI are 1754 × 1240
pixels; that measured pair has zero changed pixels and zero mean absolute
channel difference. This is a comparison of these rendered pages and settings,
not a general pixel-parity claim. The PDF byte hashes differ.

The requested English and Korean fonts were `Sans Serif` at 9 and 10 points.
That family was unavailable on the capture host, so Qt used its platform font
fallback. The fallback face is not pinned by this evidence.

## Retained artifacts

- `block-apply-evidence.json`: per-block candidate, decision, persistence, and
  reload counts.
- `output-evidence.json`: PDF hashes and metadata, temporary workspace hash
  and size, settings, expected text, and render comparison measurements. The
  workspace is identified as not retained.
- `legacy-schedule.pdf` and `next-schedule.pdf`: matched output documents.
- `legacy-page-1.png`, `next-page-1.png`, and `page-comparison.png`:
  representative first-page renders and their side-by-side comparison.

Raw test logs and the generated `.tps` workspace are omitted. The fixture and
integration tests are the reproducible source.

## Reproduction and recorded verification

From the repository root on macOS:

```sh
cmake --build build/macos-clang-debug \
  --target ClassMngrNextSchedulePageTests \
           ClassMngrNextNormalScheduleImportIntegrationTests -j4

QT_QPA_PLATFORM=offscreen QT_SCALE_FACTOR=1 \
  build/macos-clang-debug/ClassMngrNextSchedulePageTests

C51_OUTPUT_DIR="$(mktemp -d)"
QT_QPA_PLATFORM=offscreen QT_SCALE_FACTOR=1 \
CLASSMNGR_C51_SCHEDULE_OUTPUT_DIR="$C51_OUTPUT_DIR" \
  build/macos-clang-debug/ClassMngrNextNormalScheduleImportIntegrationTests
```

The recorded run used macOS 27.0.1, Qt 6.12.0, arm64, and Apple Clang 17.0.0.
Schedule page tests passed 12/12; Schedule Import integration tests passed
17/17. The full suite and Windows were not run. The generated workspace and
capture outputs go to the temporary directory above; no logs are part of this
package.
