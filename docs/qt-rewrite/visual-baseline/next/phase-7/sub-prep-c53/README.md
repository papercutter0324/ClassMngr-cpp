# C53 Sub Prep PDF text-map finalizer

Task ID: `phase7_c53_subprep_finalizer_implementation_02`.

## Current status

C53 is accepted after independent local and native Windows verification. Local
evidence and the hosted Windows run are recorded below.

The accepted implementation adds a source-aware, fail-closed finalizer to the
shared Next Sub Prep output path. `SubPrepPackageService::generateForNext()`
captures the `QTextItem` strings passed to Qt while the existing roster painter
draws them, then associates those source characters with the actual emitted
PDF text codes. It rewrites the corresponding ToUnicode streams only; the page
content and embedded glyph streams remain byte-for-byte as Qt wrote them. The
Legacy `generate()` overloads still use the original writer path.

The finalizer is limited to the Qt 6.12.0 PDF structures exercised here. It
returns an error for unsupported or malformed input, including encryption,
unsupported xref/object/filter forms, missing or extra source text, conflicting
code-to-character associations, unsupported shaping/clusters, and mappings
that cannot be proven from the captured source. It does not use observed CIDs
as correction constants, rewrite font cmaps, or alter glyph painting.

## C44 acceptance evidence

[`c44-finalizer/C44-before.pdf`](c44-finalizer/C44-before.pdf) and
[`c44-finalizer/C44-after.pdf`](c44-finalizer/C44-after.pdf) are the same
synthetic 96-class, 25-student-per-class input. The first PDF uses the Legacy
renderer entry point without finalization. The second is produced through the
actual Next package entry point, `SubPrepPackageService::generateForNext()`.
The independent fixture/source oracle finds all 2,400 exact `Student n-n`
names, all 96 U+2022 bullets, and all 96 expected complete headers in the Next
PDF. It does not normalize extracted text or compare PDF text against the
Legacy PDF.

[`C44-raster-source-audit.json`](c44-finalizer/C44-raster-source-audit.json)
records exact source-text counts and every page's 150-DPI raster result. All 16
pages are 1240 × 1754 pixels and have zero changed pixels when rendered with
Qt `QPdfDocument` before and after finalization. The per-page raster hashes
match.

[`C44-used-code-audit.json`](c44-finalizer/C44-used-code-audit.json) audits
every used code for every page/font in the finalized PDF: all 64 page/font
records have no unmapped codes and no control-character mappings. It records
3,840 U+0020 space uses, 2,496 hyphen uses, and 96 bullet uses. The paired
[`C44-before-used-code-audit.json`](c44-finalizer/C44-before-used-code-audit.json)
shows the original omissions and TAB space mappings. These diagnostics are
kept as before-state evidence; the rejected font-cmap experiment and
no-shaping/outline trials under `font-variant/` and `writer-probe/` are not
production fixes.

## Reproduce locally

Use the standard macOS build folder and run from the repository root:

```sh
cmake --build build/macos-clang-debug \
  --target ClassMngrSubPrepPdfTextFinalizerTests ClassMngrSubPrepPackageServiceTests -j 6

QT_QPA_PLATFORM=offscreen build/macos-clang-debug/ClassMngrSubPrepPdfTextFinalizerTests

CLASSMNGR_SUB_PREP_C53_EVIDENCE_DIR=docs/qt-rewrite/visual-baseline/next/phase-7/sub-prep-c53/c44-finalizer \
QT_QPA_PLATFORM=offscreen build/macos-clang-debug/ClassMngrSubPrepPackageServiceTests \
  nextLargePackageFinalizesSourceTextAndPreservesAllPixels

python3 docs/qt-rewrite/visual-baseline/next/phase-7/sub-prep-c53/writer-probe/check_used_codes.py \
  --require-valid \
  --output docs/qt-rewrite/visual-baseline/next/phase-7/sub-prep-c53/c44-finalizer/C44-used-code-audit.json \
  docs/qt-rewrite/visual-baseline/next/phase-7/sub-prep-c53/c44-finalizer/C44-after.pdf

python3 docs/qt-rewrite/visual-baseline/next/phase-7/sub-prep-c53/writer-probe/check_used_codes.py \
  --require-valid \
  --output docs/qt-rewrite/visual-baseline/next/phase-7/sub-prep-c53/c44-finalizer/C44-before-used-code-audit.json \
  docs/qt-rewrite/visual-baseline/next/phase-7/sub-prep-c53/c44-finalizer/C44-before.pdf
```

The final command is diagnostic and exits nonzero for the known-invalid
before-state PDF. The strict auditor is a focused checker for the supported Qt
PDF structure, not a general PDF parser.

The focused finalizer suite covers exact text and raster preservation, source
conflicts, unsupported clusters, malformed xrefs/objects, encryption,
unsupported filters, missing maps/resources/source, unexpected text, and
control-character input. The package-service suite also exercises the
Next-specific entry point and existing Legacy paths.

## SQL-backed Legacy/Next comparison

`ClassMngrStartupPerformanceTests::capturesMatchedSqlSubPrepForC53` creates
the Legacy package from `tests/fixtures/workspaces/large_startup.sql`, applies
the existing route's schedule and roster-column preparation, retains the
prepared `.tps` before Legacy generation, and records its SHA-256. The capture
uses Teacher 01, 14–18 September 2026, English, and By Day output.
The manifest also records the working database hash after Legacy generation;
Next consumes the retained pre-generation snapshot and verifies its hash
before and after its own generation. The SQL oracle compares the retained
snapshot with the post-Legacy workspace and fails if any roster-source table
or schema (`classes`, `class_info`, `teachers`, `class_times`,
`roster_columns`, or `roster_data`) changed during the Legacy run.

[`scripts/phase7/c53_sub_prep_sql_oracle.py`](../../../../../../scripts/phase7/c53_sub_prep_sql_oracle.py)
opens that retained database read-only and derives the exact 750 weekday
roster names and 30 By Day class/teacher-room header records from SQL using
the renderer's `classLabel` and `teacherRoomLabel` source rules. It records
the SQL fixture and source hashes. It does not read PDFs, normalize extracted
PDF text, or compare PDF text to the Legacy output.

`ClassMngrNextSubPrepSqlParityCapture` verifies the database hash before Next
generation, selects the same class IDs, dates, locale, and template, and calls
`SubPrepPackageService::generateForNext()`. It loads the information-sheet
preferences, personal Zoom values, and current campus from the same prepared
workspace and the same campus resource directory used by the Legacy page. It
applies the Legacy page's documented default grading text when the workspace
does not store one. It checks every SQL-derived roster name and header
occurrence in the Next roster PDF and rejects control characters. The
independent used-code auditor checks every used code on every page/font of the
Next roster PDF; Legacy roster audit output is retained as before-state
diagnostics. The harness also binds the Legacy PDF hash, capture selection,
and pre-run database hash to the oracle, then requires matching SHA-256 values
for both embedded font resources.

The raster gate compares **both** package documents: all pages of `Sub Prep.pdf`
and `Rosters - By Day.pdf` from Legacy and Next. The same Qt `QPdfDocument`
renderer produces 150-DPI PNGs, per-page geometry and hashes, difference
images, and changed-pixel totals. Both PDF pairs must have matching page counts
and dimensions and exactly zero changed pixels on every page.

The focused workflow
[`sub-prep-c53-windows.yml`](../../../../../../.github/workflows/sub-prep-c53-windows.yml)
runs on relevant pushes to `main` or `Qt-Rewrite`, relevant pull requests,
and manual dispatch. It builds the Legacy capture test and Next parity
harness on native Windows x64 with Qt 6.12, runs the SQL oracle and strict used
code audit, and uploads the PDFs, manifests, SQL oracle, all-page raster PNGs,
diff images, and comparison reports as `c53-matched-sql-windows-x64`.

## Accepted local evidence

The same flow was reproduced locally with macOS Qt 6.12. The Legacy capture
passed; the SQL oracle found 750/750 exact names and 30/30 expected header
records; the Next used-code audit passed; and all 17 matched page pairs
(12 information-sheet pages plus 5 roster pages) had zero changed pixels.

## Hosted Windows evidence

The [native Windows run](https://github.com/papercutter0324/ClassMngr-cpp/actions/runs/41268987161)
used commit `b6d8a0a18b42e23da8d455dc8989f9b5cbd3658f` on Windows Server 2022
x64 with Qt 6.12.0 at 150 DPI and strict zero tolerance. The read-only SQL
oracle verified 750 exact names and 30 header records. The Next roster
used-code audit covered 5 pages and 15 page-font records, with zero unmapped
codes and zero control mappings. Both matched PDF pairs passed all 17 pages
(12-page Sub Prep and 5-page By Day roster): dimensions match and total changed
pixels are zero. The run also verified the same SQL snapshot and embedded
fonts.

The run uploaded `c53-matched-sql-windows-x64` (73 files, 10,811,991 bytes;
SHA-256 `1bc44fe9124182a9b97b6e1d74f2f47c878bb8114a30298290d4b00519e6b961`).
The workflow retains the artifact for 90 days.
