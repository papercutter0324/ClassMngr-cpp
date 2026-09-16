# Qt Rewrite Phase 0 - Baseline and Evidence Log

Status: In progress
Snapshot: `892d51c` (application snapshot used for the current packaged run)
Date started: `2026-09-15`

## Evidence policy

Only clean, freshly configured builds from the current source snapshot are
authoritative. Existing build directories are useful for discovery but are not
accepted as baseline evidence because they contain stale cache variables from
an older tree. The obsolete historical docs were removed from `docs/`; this log
does not recreate or rely on them.

## Initial static evidence

- Historical source snapshot: `75755460`.
- Raw assets: 226 files, 65,729,685 bytes.
- Current source inventory: 58 core, 42 data, 43 domain, 115 shared UI, 283
  feature, and 34 app files; 73 test files.
- A stale Windows x64 Debug build enumerated 66 CTest tests. It must be
  reconfigured before test results are attached to this baseline.
- Current startup code still includes a splash screen and resource-pack
  initialization; these are explicit v2 removal/replacement acceptance items,
  not changes made in Phase 0.
- Document-catalog startup parsing is metadata-only in the current source.
  The document route resolves the selected asset and calls
  `PdfViewerPage::loadPdf()` only after navigation to a document; leaving the
  viewer calls `releaseDocument()`. This is the source baseline for the v2
  contract, while startup-negative and repeated open/close memory evidence
  remain open.

## Windows x64 toolchain run log

- Earlier run snapshot: `75755460`.
- Environment: Visual Studio 2026 Developer Command Prompt, MSVC
  `19.51.36256.0`, Qt `6.12.0`, CMake `4.4.2`.
- Clean application-only Ninja configure succeeded: configure `38.5 s`,
  generation `1.6 s`.
- Ninja with two workers stopped at feature compilation with MSVC exit code
  `0xFFFFFFFF` and no diagnostic output.
- Ninja single-worker retry progressed to `50/215` and stopped compiling
  `src/features/my_info/ui/my_classes_page_content.cpp` with the same silent
  exit code.
- No Release executable, install tree, or memory measurements are accepted
  from this run. The next diagnostic is an isolated single-file MSVC compile or
  a toolchain/resource check; this is a baseline blocker, not a rewrite result.

### Current clean fixture run

- Source state: `892d51c` plus the test-only fixture-retention hook in this
  evidence slice.
- Build directory: `build/qt-rewrite-phase0-startup-ninja`.
- Configuration: clean Ninja Debug, Windows x64, Qt `6.12.0`, MSVC
  `19.51.36256.0`, `BUILD_TESTING=ON`, and startup update checks disabled.
- Configure and generation succeeded; configure took `398.8 s` because the
  current Qt setup scans the repository's many test targets.
- The focused target build completed all `352` steps, including
  `src/features/my_info/ui/my_classes_page_content.cpp`, the application, and
  `ClassMngrStartupPerformanceTests.exe`.
- `ClassMngrStartupPerformanceTests.exe -v1` passed all eight test slots with
  exit code `0`. The representative profile was materialized from
  `tests/fixtures/workspaces/representative_startup.sql` through the current
  schema manager.
- Representative checkpoints reported `202,162,176` bytes peak working set at
  `startup-complete` and `221,011,968` bytes at the five-second settled
  checkpoint, with `153,092,096` bytes private usage at the latter checkpoint.
  These are Debug/offscreen measurements and are preliminary, not the packaged
  Release gate.
- The existing Visual Studio build directory still has a Qt QML metadata
  regeneration loop and remains excluded from acceptance evidence.

### Current packaged Windows x64 Release run

- Application snapshot: `892d51c`. Build directory:
  `build/qt-rewrite-phase0-windows-x64-release`. Install prefix:
  `dist/qt-rewrite-phase0-windows-x64`.
- A clean Ninja Release configure succeeded with Qt `6.12.0`, MSVC
  `19.51.36256.0`, and CMake `4.4.2`. The application build completed all
  `347` steps, and `cmake --install` produced the packaged executable,
  resource packs, QML modules, Qt plugins, and SQLite driver.
- The installed `ClassMngr.exe` launched successfully with
  `QT_QPA_PLATFORM=offscreen`, startup update checks disabled at configure
  time, and exit code `0` for both the empty and representative scenarios.
  The Debug focused suite remains the fixture/compatibility test authority;
  this Release configuration intentionally omits test binaries.
- Empty Release startup (`minimal-startup`, five-second settle):
  `startup-complete` at `2,974 ms`, `settled-5s` at `8,028 ms`, peak working
  set `138,178,560` bytes, peak private usage `111,345,664` bytes, 20
  checkpoints, 14 progress updates, and final progress `100`.
- Representative Release startup (`representative-startup`, the retained
  `representative-startup.tps` fixture, five-second settle):
  `database-opened` at `718 ms`, `startup-complete` at `3,037 ms`, and
  `settled-5s` at `8,103 ms`. Peak working set was `169,287,680` bytes and
  peak private usage was `150,745,088` bytes; the trace contains 20
  checkpoints, 14 progress updates, and final progress `100`. At
  `startup-complete` the trace reported 224 widgets, one instantiated page,
  11 registered pages, one live schedule widget, and one schedule render.
- Large-workspace Release startup (`representative-startup`, the retained
  `large-startup.tps` fixture, five-second settle): `database-opened` at
  `748 ms`, `startup-complete` at `2,942 ms`, and `settled-5s` at `8,000 ms`.
  Peak working set was `175,370,240` bytes and peak private usage was
  `156,618,752` bytes. The startup-complete trace reported 231 widgets, one
   instantiated page, 11 registered pages, 49 schedule cell widgets, and a
   118 ms full schedule render.
- Legacy-workspace Release startup (`representative-startup`, the migrated
  `legacy-startup.db` fixture, five-second settle): `database-opened` at
  `616 ms`, `startup-complete` at `2,942 ms`, and `settled-5s` at `7,998 ms`.
  Peak working set was `161,640,448` bytes and peak private usage was
  `141,246,464` bytes. The startup-complete trace reported 212 widgets, one
  instantiated page, 11 registered pages, 30 schedule cell widgets, and a
  5 ms full schedule render. The fixture migration also preserved the
  pre-constraint backup sidecar required by the compatibility test.
- Packaged Release lifecycle workflow (representative workspace): the new
  `--startup-performance-workflow` harness visited all 11 registered top-level
  routes, opened the deferred Calendar tab, exercised the PDF viewer, returned
  to My Workspace, and retained the final route state through the one- and
  five-second samples. `startup-complete` was `3,006 ms`;
  `workflow-complete` was `8,255 ms`; the one- and five-second samples were
  `9,302` and `13,298 ms`. The workflow reached 11/11 instantiated pages,
  2,530 widgets, three live ScheduleWidgets, and three full schedule renders.
  The PDF trace opened and rendered the 38-page Lesson Planning Guide twice,
  released it between opens, and ended with zero live PDF documents. Working
  set was `249,663,488` bytes with a `259,137,536`-byte peak; peak private
  usage was `297,848,832` bytes. Both working-set samples remain below the
  final 250 MiB target; this is observational trend evidence, not a Phase 0
  acceptance gate, and it is a tighter regression boundary than the route-only
  run.
- Five-minute idle Release workflow: the same heavy route and PDF cycle
  completed `workflow-complete` at `8,846 ms` and `settled-5m` at
  `308,890 ms`. The sample retained 2,530 widgets and 11/11 pages, with zero
  live PDF documents and 2 loads/2 renders/2 releases. Working set at the
  five-minute sample was `250,036,224` bytes; the run peak was
  `259,510,272` bytes and peak private usage was `298,201,088` bytes. The
  per-route `workflow-page-left` checkpoints cover all 11 transitions from
  My Workspace through the PDF viewer.
- The opt-in packaged Release workflow against `large_startup.sql` now reaches
  and completes the Sub Prep route, but the current path is not within the
  representative memory boundary: `workflow-complete` was `9,553 ms`, peak
  working set was `410,468,352` bytes, and peak private usage was
  `452,853,760` bytes. The Sub Prep portion retained 2,948 descendant widgets,
  192 text editors, 96 logical navigation rows, 24 teacher groups, and made
  96 class-information, teacher, and roster lookups in one rebuild. The
  one-second settled checkpoint retained `401,285,120` bytes working set and
  `425,164,800` bytes private usage with the same feature counts. The
  retained trace and JSON report are under
  `docs/qt-rewrite/visual-baseline/release/large-sub-prep-boundary/`; this is
  the current-product heavy-route boundary for the v2
  virtualization/resource-ownership slice, not an accepted v2 result.
- The follow-up heavy lifecycle run drives the same packaged Release fixture
  through selection, two refreshes, two leaves, and two re-entries. It
  completed normally (`workflow-complete` `12,161 ms`, `settled-1s`
  `13,197 ms`) with peak working set `447,959,040` bytes and peak private
  usage `477,900,800` bytes. Class-information descendants/text editors grew
  from `2,948`/`192` after the first load to `5,896`/`384` after refresh one
  and `8,844`/`576` after refresh two; neither leave nor re-entry released the
  graph. The trace records `288` roster queries, `7,200` result rows, and
  `21,600` cells across the three loads. These are the legacy before-state
  inputs for the v2 release/reuse boundary, not an accepted v2 budget result.
- The packaged Release Classes lifecycle run uses the same 96-class fixture and
  completes selection, two refreshes, two leaves, and two re-entries. It
  records 96 visible classes in four grade groups, 192 grouped/All class-tab
  placeholders, and 473 navigation descendants. Across three loads it makes
  288 class rows, 288 class-information rows with 2,376 schedule rows, and
  288 teacher rows of query results. Navigation remains bounded at 473
  descendants, while process working set rises from `231,948,288` bytes at
  entry to `273,227,776` bytes at lifecycle completion. The retained artifact
  is under `docs/qt-rewrite/visual-baseline/release/large-classes-boundary/`;
  this is a legacy before-state measurement, not an accepted v2 budget result.
- The follow-up packaged Release Classes visual-state run retains the real
  entry, selected-class `96`, and selected-class `1` frames for all four
  English/Korean and light/dark combinations. Each route reports 96 visible
  classes, 473 navigation descendants, and `captured=true` at every visual
  checkpoint; the four routes complete between `11,367` and `11,415 ms`, with
  peak working sets from `409,907,200` to `412,454,912` bytes. The retained
  fixture, screenshots, metrics/traces, and manifest are under
  `docs/qt-rewrite/visual-baseline/release/large-classes-visual-states/`.
- The packaged Release Schedule lifecycle run uses the same fixture and
  completes two refreshes, two leaves, and two re-entries. The standalone
  schedule page remains at 7 model/table rows, 49 model cells, 768 schedule
  entries, 8 columns, 7 time-column items, 49 cell widgets, and 96 visible
  classes at every checkpoint. Its memory rises from `201,711,616` working-set
  bytes / `187,531,264` private-usage bytes at entry to `203,198,464` /
  `189,116,416` at lifecycle completion. The process-level renderer counters
  show 98 created cell widgets and zero removals/deferred deletions across the
  repeated refreshes; the retained artifact is under
  `docs/qt-rewrite/visual-baseline/release/large-schedule-boundary/`. This is
  the legacy Schedule before-state for the Phase 7E model/delegate migration.
- The packaged Release heavy-route Schedule Import run uses the same 96-class
  workspace and a generated two-sheet workbook with five user blocks and 96
  parsed class candidates. It reaches workbook parse, staged review, conflict
  acknowledgement, cancellation, review release, and workbook release before
  returning to the normal workflow. At review-ready it materializes 20 preview
  entries with five teacher controls and 20 class controls; the raw workbook
  bytes are no longer retained after parsing, the parsed workbook remains held
  through review, and it is released after the source dialog closes. The child
  exits normally; the import checkpoints are `3,684`/`4,489`/`4,563`/`4,597` ms
  for parse/review/post-review-release/post-release, while the full heavy route
  reaches `workflow-complete` at `10,760 ms` and `settled-1s` at `11,795 ms`.
  Route-wide peak working set/private usage are `418,152,448`/
  `461,545,472` bytes. The retained source workbook, source/review captures,
  trace, manifest, and profiler report are under
  `docs/qt-rewrite/visual-baseline/release/large-schedule-import-boundary/`.
  This is the current-product import before-state, not a v2 operation budget.
- The packaged Release heavy-route Schedule Import apply run uses a separate
  deterministic conflict-free variant of the same 96-class workbook. It parsed
  96 class candidates, prepared 20 preview entries with five teacher controls
  and 20 class controls, loaded 24 existing teachers, 96 existing classes, and
  96 class-information records, then committed five teachers, 20 classes, and
  20 schedule rows while clearing 96 prior schedules. The workbook and review
  representations were released after the committed transaction; the refreshed
  Schedule page settled at 20 visible classes. The child exited normally; the
  apply/release/refresh checkpoints were `4,568`/`4,635`/`4,681 ms`. The full
  route reached `workflow-complete` at `9,218 ms` and `settled-1s` at
  `10,253 ms`, with route-wide peak working set/private usage of
  `361,115,648`/`403,337,216` bytes. The retained source/review captures,
  generated workbook, trace, manifest, and profiler report are under
  `docs/qt-rewrite/visual-baseline/release/large-schedule-import-apply-boundary/`.
  This is the current-product apply/commit before-state, not a v2 operation
  budget.
- Both packaged Schedule Import heavy routes now retain the real asynchronous
  loading state as `schedule-import-loading.png`. The capture records
  `status=Loading workbook...`, a visible indeterminate progress bar, all
  source/load controls disabled, and `captured=true` before the 96-class
  workbook reaches parsing. The cancel route completed with a
  `418,488,320`-byte peak working set and the apply route with
  `361,316,352` bytes; the loading capture is retained in each corresponding
  Schedule Import boundary directory.
- The cancel boundary now also retains the real Schedule Import conflict-warning
  modal from the large route. Its deterministic workbook variant intentionally
  overlaps projected meetings so the existing warning path is exercised; the
  checkpoint records `visible=true`, warning text beginning
  `Review these schedule conflicts before importing:`, `importEnabled=false`,
  and `captured=true`. The captured `schedule-import-conflict-warning.png` is
  retained under
  `docs/qt-rewrite/visual-baseline/release/large-schedule-import-boundary/`,
  with the full warning detail preserved in the trace and profiler report.
  The separate apply variant remains conflict-free, commits normally, and
  records `conflictWarningVisualReference=false` in its manifest. In the
  latest focused runs, cancel/apply peak working sets were `418,058,240` /
  `360,960,000` bytes and peak private usage was `461,225,984` /
  `402,731,008` bytes. This is current-product before-state evidence, not a
  v2 operation budget.
- The packaged Release heavy-route Calendar Import run uses the same 96-class
  workspace and a deterministic two-sheet, 16,284-byte workbook served through
  a local HTTP response. The import parsed 382 cells and 12 merged ranges,
  produced 261 calendar events, skipped 104 weekend entries, queried 180
  existing events, and saved 261 new events. The response bytes were released
  before the parsed workbook checkpoint, and the workbook/event/operation
  representations were all released by the operation-release checkpoint.
  Calendar refresh then settled at 22 cached events, 22 date buckets, one
  loaded range, and one retained range. The child exited normally; the
  response/parse/events/save/apply/release/page-refresh checkpoints were
  `3,354`/`3,386`/`3,479`/`3,543`/`3,582`/`3,638`/`3,813` ms. The full route
  reached `workflow-complete` at `11,227 ms` and `settled-1s` at `12,263 ms`,
  with route-wide peak working set/private usage of `416,530,432`/
  `457,003,008` bytes. The retained workbook, Preferences and Calendar
  captures, trace, manifest, and profiler report are under
  `docs/qt-rewrite/visual-baseline/release/large-calendar-import-boundary/`.
  This is the current-product Calendar import before-state, not a v2 operation
  budget.
- The same packaged Calendar Import Heavy route now retains the real transient
  Preferences loading state as `calendar-import-loading.png`. The checkpoint
  records `status=Importing events...`, `controlsDisabled=true`, and
  `captured=true`; the capture scrolls the existing Calendar tab to the Import
  section for a useful frame and restores the prior scroll position afterward.
  Loading occurred at `3,207 ms`, workbook parsing at `3,262 ms`, page refresh
  at `3,656 ms`, and the route completed at `10,112 ms` with `11,138 ms`
  settled. Route-wide peak working set/private usage was `416,694,272`/
  `457,310,208` bytes. The retained loading frame, updated Preferences and
  Calendar captures, trace, manifest, and profiler report remain under the
  Calendar Import boundary directory.
- The packaged Release heavy-route Class Transfer run uses the same 96-class
  workspace and a generated 1,716,291-byte JSON package containing 12 teachers,
  48 classes, 8,640 roster cells, 96 speaking evaluations with 26,400 cells,
  and 48 schedule rows. It reaches package load, preview, a 12-teacher/
  48-class review dialog, transaction commit, dialog release, operation release,
  and Classes refresh. The apply creates 12 teachers and 48 classes, taking the
  destination from 24/96 teachers/classes to 36/144; the refreshed Classes page
  reports 144 visible classes. Raw JSON bytes and the decoded document are not
  retained after load, and package/preview/dialog/operation ownership is false
  at post-release. Transfer operation memory rises from `231,911,424` working/
  `220,925,952` private bytes at start to a `250,601,472`/
  `237,604,864` review peak, then is `242,741,248`/
  `234,180,608` at post-release; the page refresh is `262,430,720`/
  `251,817,984`. The full heavy route reached `workflow-complete` at
  `15,423 ms` and `settled-1s` at `16,458 ms`, with route-wide peak working
  set/private usage of `476,622,848`/`519,741,440` bytes. The retained package,
  review capture, trace, manifest, process logs, and profiler report are under
  `docs/qt-rewrite/visual-baseline/release/large-class-transfer-boundary/`.
  This is the current-product transfer before-state, not a v2 operation budget.
- The packaged Release heavy-route Speaking Evaluation run augments the same
  96-class workspace with 96 canonical `Winter` evaluations and 2,400
  evaluation rows, then exercises the real page, report dialog, export dialog,
  AI prompt/review dialog, comment apply, cleanup, and refresh paths for one
  selected 25-row evaluation. The page reports 96 visible class tabs across
  four navigation widgets, four evaluation tabs, and an 11-column by 25-row
  model (275 cells). The report batch contains 25 records (`4,215` text bytes);
  the AI selection/review tables contain 75/125 items, with 25 accepted
  comments and an `8,109`-byte prompt plus `5,025`-byte response. Export
  produces 25 individual PDFs (`51,631,608` bytes) and a
  `51,634,180`-byte archive. The child exits normally; operation start/page/
  report/export/AI-response/release/refresh checkpoints are
  `4,558`/`5,597`/`5,637`/`9,646`/`9,736`/`9,941`/`9,993 ms`.
  The speaking operation reaches a `384,593,920`-byte peak working set and
  `292,061,184`-byte private usage before release; after release it measures
  `307,388,416`/`289,484,800` bytes. The full route reaches
  `workflow-complete` at `14,893 ms` and `settled-1s` at `15,923 ms`, with
  route-wide peak working set/private usage of
  `471,343,104`/`525,725,696` bytes. All speaking-operation retention flags
  are false after release and refresh. The retained fixture, four UI captures,
  25 PDFs, archive, trace, manifest, process logs, and profiler report are
  under
  `docs/qt-rewrite/visual-baseline/release/large-speaking-evaluation-boundary/`.
  The retained UI set now includes the PowerPoint renderer-selection reference;
  the external Office automation itself was not executed in this Windows
  offscreen run and remains platform-specific evidence for a later slice.
  This is the current-product speaking batch before-state, not a v2 operation
  budget.
- The packaged Release heavy-route Staff Directory run adds 96 deterministic
  Native English Teacher rows and 96 GS Team rows to a generated copy of the
  large workspace, then drives both real directory pages through load, two
  refreshes, leave, re-entry, and operation release. Native English Teachers
  expose a 96-by-6 table with 576 cell items and 7,314 bytes of cell text;
  GS Team exposes a 96-by-5 table with 480 items and 4,940 bytes of text.
  Both pages expose 85 page-widget descendants. Native lifecycle checkpoints
  are `5,314`/`5,344`/`5,425`/`5,459`/`5,490`/`5,519 ms`; GS checkpoints are
  `5,784`/`5,815`/`5,889`/`5,923`/`5,955`/`5,984 ms`. The staff operation
  peaks at `242,855,936` working-set bytes and `235,036,672` private-usage
  bytes; the full route peaks at `410,624,000`/`452,751,360` bytes and
  settles at `10,814 ms`. The child exits normally and the retained evidence
  is under
  `docs/qt-rewrite/visual-baseline/release/large-staff-directory-boundary/`.
  Operation ownership is false after release, but both table-retention flags
  remain true, documenting the current page-manager retention boundary. This
  is the Staff Directory current-product before-state, not a v2 operation
  budget.
- The packaged Release heavy-route Sub Prep output run composes the 96-class
  selection/lifecycle boundary with the actual generation dialog and package
  service. A valid By Day output fixture selects 30 classes across the five
  weekdays and six supported time slots, while retaining the 96-class workspace
  load and 7,200-cell roster payload. The in-process Qt automation captures
  the configured dialog, drives the real validation state with both output
  options cleared, verifies that the OK action is disabled, then restores valid
  controls and accepts the real modal flow. It generates two PDFs (the Sub
  Prep document and By Day roster) totaling 17 pages and `139,650` bytes, then
  reopens both documents and renders their first pages at 150 DPI. Output
  start/validation/generated/release checkpoints were
  `8,088`/`8,747`/`9,296`/`9,337 ms`; decoded first-page bytes totaled
  `17,399,680`. The output operation peaked at `498,176,000` working-set bytes
  and `480,948,224` private-usage bytes, and the full route reached
  `workflow-complete` at `11,896 ms` and `settled-1s` at `12,928 ms`.
  Output failure/operation-retention flags were clear after release and
  `livePdfDocumentCount` was zero. The retained configured dialog,
  validation-error dialog, PDFs, first-page captures, generated fixture, trace,
  manifest, and profiler report are under
  `docs/qt-rewrite/visual-baseline/release/large-sub-prep-output-boundary/`.
  This is current-product generated-output before-state evidence, not a v2
  memory budget.
- Reviewable Release artifacts are retained at
  `docs/qt-rewrite/visual-baseline/release/empty/` and
  `docs/qt-rewrite/visual-baseline/release/representative/`, with the large
  and legacy runs in their matching subdirectories. The representative
  `release/workflow/` directory contains the packaged lifecycle trace, PDF
  open/reopen frames, and startup/settled frames. Each directory contains
  `startup-complete.png`, `settled-final.png`, and `startup-metrics.json`; the
  workflow directory additionally contains `pdf-opened.png`,
  `pdf-reopened.png`, and `workflow-trace.txt`. The populated frames show the
  expected workspace schedule grid. The
  `release/workflow-five-minute/` directory contains the five-minute idle
  report and its matching route/PDF captures. The four-language/theme matrices
  remain available in the Debug/offscreen
  reference directories.
- The packaged-configuration Sub Prep output reference is retained under
  `docs/qt-rewrite/visual-baseline/release/sub-prep-output/reference/`. It
  contains the direct service-level 96-class-scale `Sub Prep.pdf` and Daily
  roster PDF, their first-page PNGs, and a manifest with document sizes, page
  counts, and render dimensions. The actual packaged UI/output boundary is
  retained separately under the large Sub Prep output directory above.
- The packaged Release heavy-route Sub Prep visual-state run drives the real
  96-class workspace through populated selection and changed-selection states
  in English/Korean and light/dark variants, then drives a separate 96-class
  database with all meeting times cleared to capture the empty class-information
  state. The populated captures report 96 visible classes and change the
  selected class from 25 to 1; the empty capture reports zero visible classes
  and selected class `-1`. Each populated route also captures the top-of-page
  mixed editing/read-only state and verifies four editable text fields plus
  six read-only line fields. The four populated routes completed between
  `7,079` and `7,364 ms`, with peak working sets between `408,637,440` and
  `410,324,992` bytes. The empty route completed at `5,085 ms` with a
  `292,331,520`-byte peak working set. The retained screenshots, two generated
  fixtures, per-variant metrics/traces, and manifest are under
  `docs/qt-rewrite/visual-baseline/release/large-sub-prep-visual-states/`.

## Progress update - 2026-09-17 (Calendar Import parser-failure boundary)

- The Calendar Import parser-failure boundary is accepted for Phase 0. A
  fresh Ninja Release configure/build completed at
  `build/qt-rewrite-calendar-verify-ninja` with Qt `6.12.0`/x64 MSVC, linking
  `ClassMngr` and `ClassMngrStartupPerformanceTests`.
- The expected-failure route passed with
  `CLASSMNGR_STARTUP_CALENDAR_IMPORT_EXPECTED_FAILURE=1` and a deterministic
  69-byte malformed local HTTP response. The manifest references
  `large-calendar-import-workflow.json`; trace/metrics record
  `Import failed: The downloaded spreadsheet is missing xl/workbook.xml.`,
  Import Events re-enabled, unchanged events `0 -> 0`, operation release
  before failure observation, absent forbidden success checkpoints, workflow
  complete, and normal exit. It completed at `11,191 ms`/`12,228 ms`
  (`workflow`/`settled-1s`) with peak working set/private usage of
  `410,251,264`/`452,120,576` bytes (legacy before-state only).
- The error screenshot was manually inspected; an independent Tester
  confirmed both PNGs valid at `1020x735` and all checks. The unchanged
  success route passed, and `ClassMngrStartupPerformanceTests.exe -v1` with
  Calendar Import opt-ins cleared exited `0`. Failure evidence is under
  `docs/qt-rewrite/visual-baseline/release/large-calendar-import-error-boundary/`;
  normal success evidence is under the corresponding
  `large-calendar-import-boundary/` directory. Phase 0 remains In progress;
  no Phase 1 or v2 memory acceptance follows from this boundary.

## Fixture added in this pass

`tests/fixtures/workspaces/representative_startup.sql` is the permanent,
reviewable source for the representative startup profile. The startup test
creates a temporary `.tps` database, applies the latest schema, executes the
fixture statements, and then validates integrity plus the expected teachers,
classes, schedules, roster rows, and saved schedule settings. This removes the
dependency on the deleted startup-optimization plan and makes the fixture
reproducible on a clean checkout.

`tests/fixtures/workspaces/large_startup.sql` adds a generated heavy-route
workspace without checking in a binary database. Its deterministic contract is
24 teachers, 96 classes, 768 regular schedule rows, 24 intensive rows, five
intensive slot states, 7,200 roster cells, 20 speaking evaluations with 600
evaluation rows, three campuses, and 180 calendar events. The focused startup
test validates its integrity and row counts after materialization.

The direct heavy Sub Prep output slot mirrors the large fixture's 24 teachers,
96 classes, and 7,200 roster cells in an operation-scoped package request. The
direct output oracle generates a 19-page Sub Prep PDF and a 16-page Daily
roster PDF, then reopens and renders the first page of each. The separate
packaged UI/output boundary drives the real dialog and package path against a
96-class workspace, retains the valid configured dialog and the disabled-OK
validation-error state, uses 30 valid By Day selections, and retains its generated
PDFs and decoded first-page captures under
`docs/qt-rewrite/visual-baseline/release/large-sub-prep-output-boundary/`.

The packaged Sub Prep visual-state probe generates a populated copy of the
large workspace plus a matching copy with regular and intensive meeting times
cleared. It retains the selected and changed-selection captures for all four
English/Korean and light/dark combinations, the empty class-information
capture, and their metrics/traces under
`docs/qt-rewrite/visual-baseline/release/large-sub-prep-visual-states/`.

The packaged Release resource trace runs after the full large-workspace route,
including PDF open/render/release/reopen, and returns to My Workspace before
enumerating every required RCC pack and embedded asset. Its retained report is
under `docs/qt-rewrite/visual-baseline/release/large-resource-trace-boundary/`:
189 payload entries, `62,781,401` logical installed bytes, 34 image decodes
with `147,851,916` bytes of potential decoded residency, 83 on-demand entries,
and zero decoded bytes for all catalog PDF/PPTX entries. Required pack leases
return to their pre-trace mount state. The declared `roster-designs` pack is
recorded as optional and unavailable because this checkout has no source
directory or packaged `.rcc`; that is an explicit Phase 4 packaging decision,
not silently omitted evidence.

The large packaged PDF viewer probe now retains the empty catalog-ready viewer,
first open/render, post-close release, expected missing-file error, and
reopened document states under
`docs/qt-rewrite/visual-baseline/release/large-pdf-viewer-visual-states/`.
The route completed normally with `workflow-complete` at `8,990 ms`, settled at
`10,448 ms`, and reached a `410,120,192`-byte working-set peak. The error
capture visibly reports the expected missing-file message while the profiler
keeps the active PDF document count at zero; this is in-process Qt automation
on the real packaged Release route.

`tests/fixtures/workspaces/legacy_startup.sql` is a partial schema-version-zero
`.db` source. The startup test materializes it, runs the current schema manager,
and verifies migration to the latest schema, foreign-key integrity, preservation
of the legacy class and schedule, repair of its non-positive teacher reference,
and creation of the pre-constraint migration backup.

The same focused suite generates two non-persistent failure scenarios: a
malformed `.db` whose bytes must remain unchanged after rejection, and an
exclusive lock held during legacy migration. The lock case must fail cleanly,
then migrate successfully after the lock is released.

`tests/fixtures/transfers/conflict_source.json` is the permanent class-transfer
package used for conflict review. The focused class-transfer test loads it,
constructs a destination with matching class/teacher identity and a colliding
schedule, verifies the review choices, and confirms the import rolls back
without changing the destination. Its readable offscreen dialog reference is
retained at `docs/qt-rewrite/visual-baseline/conflict/class-import-review.png`.

`tests/fixtures/imports/schedule_review.xlsx` is the permanent schedule
workbook used for staged import review. The dialog test loads the workbook
through the normal source-selection path, verifies that the review stage builds
its teacher/class controls and imported color choices, and can retain a
readable offscreen reference at
`docs/qt-rewrite/visual-baseline/schedule/schedule-import-review.png`.

`tests/fixtures/imports/schedule_large_conflict.xlsx` exercises the same
normal file-selection flow with a larger set of paired schedule entries and
two imported classes that both resolve to one existing class. The focused
dialog test confirms the duplicate-target warning is presented and Import
remains disabled; its conflict-state reference is retained at
`docs/qt-rewrite/visual-baseline/schedule/schedule-conflict-review.png`.

The heavy-route import boundary uses a deterministic workbook generated by the
startup performance test. It is retained as
`docs/qt-rewrite/visual-baseline/release/large-schedule-import-boundary/generated-large-schedule-import.xlsx`
alongside the process trace and lifecycle report. The generated input is kept
separate from the compact and conflict fixtures because its purpose is memory
measurement across parse, review, cancellation, and cleanup rather than a new
product-format compatibility contract.

The heavy-route Calendar import boundary uses a deterministic two-sheet
workbook generated by the startup performance test and served through an
in-process local HTTP server. It is retained as
`docs/qt-rewrite/visual-baseline/release/large-calendar-import-boundary/generated-large-calendar-import.xlsx`
alongside the Preferences/Calendar captures, process trace, manifest, and
lifecycle report. This keeps the response, parsed workbook, database apply,
page refresh, and release checkpoints reproducible without depending on the
remote academic-calendar source.

The heavy-route Schedule Import apply boundary uses a conflict-free variant of
the large workbook generated by the startup performance test. It is retained
as
`docs/qt-rewrite/visual-baseline/release/large-schedule-import-apply-boundary/generated-large-schedule-import.xlsx`
alongside the source/review captures, process trace, manifest, and lifecycle
report. The variant preserves the 96-class stress scale while using valid,
non-overlapping meeting patterns so the actual transaction commit and
post-commit Schedule refresh can be measured.

The heavy-route Class Transfer boundary uses a generated JSON package retained
at
`docs/qt-rewrite/visual-baseline/release/large-class-transfer-boundary/generated-large-class-transfer.json`
alongside the review capture, process trace, manifest, and lifecycle report.
The package is intentionally larger than the compact compatibility fixture so
JSON decoding, matching, review controls, transaction apply, and release can
be measured at a multi-class scale without changing the product file format.

The heavy-route Speaking Evaluation boundary augments a generated copy of the
large workspace with 96 canonical `Winter` evaluations and 2,400 evaluation
rows. It is retained at
`docs/qt-rewrite/visual-baseline/release/large-speaking-evaluation-boundary/generated-large-speaking-evaluation.tps`
alongside the page/report/export/AI captures, 25 individual PDFs, archive,
process trace, manifest, and lifecycle report. The augmentation keeps the
existing workspace and file-format contract while making the large batch
review and output path reproducible.

The heavy-route Staff Directory boundary adds 96 deterministic rows to each of
the Native English Teacher and GS Team tables in a generated copy of the large
workspace. It is retained at
`docs/qt-rewrite/visual-baseline/release/large-staff-directory-boundary/generated-large-staff-directory.tps`
alongside the two directory captures, process trace, manifest, and lifecycle
report. The augmentation preserves the existing database format while making
the table cardinality and leave/re-entry retention path reproducible.

## Reproduction commands

Windows x64 clean Phase 0 build (Qt 6.12.0 installed at the path below).
Run these commands from the Visual Studio 2026 x64 Developer Command Prompt so
the bundled Ninja and MSVC toolchain are on `PATH`:

```powershell
cmake -S . -B build/qt-rewrite-phase0-windows-x64-debug `
  -G Ninja `
  -DQt6_DIR=C:/Qt/6.12.0/msvc2022_64/lib/cmake/Qt6 `
  -DBUILD_TESTING=ON `
  -DCMAKE_BUILD_TYPE=Debug `
  -DCLASSMNGR_UPDATE_CHECK_ON_STARTUP=OFF `
  -DCLASSMNGR_RESOURCE_PACK_CHECK_ON_STARTUP=OFF
cmake --build build/qt-rewrite-phase0-windows-x64-debug --config Debug --parallel 2
ctest --test-dir build/qt-rewrite-phase0-windows-x64-debug -C Debug --output-on-failure
```

Release packaging evidence will use a separate clean build and install tree:

```powershell
cmake -S . -B build/qt-rewrite-phase0-windows-x64-release `
  -G Ninja `
  -DQt6_DIR=C:/Qt/6.12.0/msvc2022_64/lib/cmake/Qt6 `
  -DBUILD_TESTING=OFF -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_INSTALL_PREFIX=dist/qt-rewrite-phase0-windows-x64 `
  -DCLASSMNGR_UPDATE_CHECK_ON_STARTUP=OFF `
  -DCLASSMNGR_RESOURCE_PACK_CHECK_ON_STARTUP=OFF
cmake --build build/qt-rewrite-phase0-windows-x64-release --config Release --parallel 2
cmake --install build/qt-rewrite-phase0-windows-x64-release --config Release
```

The required Phase 0 packaged Release platforms are Windows x64 and macOS
universal. Windows ARM64 and Linux are unofficial ports deferred to later
work. Runtime memory reports must record commit, private bytes, working
set/Resident Set Size, peak working set, handles, threads, and the exact
scenario/fixture.

The Phase 0 memory contract is recorded in
`docs/qt-rewrite/phase-0-memory-thresholds.md`: packaged Release working set
uses a strictly-below `262,144,000`-byte (`250 MiB`) comparison for the
end-of-rewrite normal resident target, while bounded heavy operations have a
temporary strictly-below `536,870,912`-byte (`512 MiB`) diagnostic ceiling.
Current legacy before-state runs retain both comparisons in each heavy
manifest as trend evidence; they are not Phase 0 acceptance gates or v2 pass
results. Each later phase must avoid regressions and should lower the affected
heavy-route working set until the final target can be enforced.

### Visual capture harness

The application now supports an in-process visual capture mode for Phase 0
when native desktop automation is unavailable. Pass an output directory with
`--startup-visual-capture-output`. The harness captures the visible main window
to `startup-complete.png`; when a positive settle interval is requested it also
writes `settled-final.png`. It uses the same startup path as the performance
instrumentation and exits nonzero if the image cannot be rendered or written.

Use `--startup-visual-capture-language english|korean` and
`--startup-visual-capture-theme light|dark` to make a language/theme variant
explicit. The focused startup test exercises all four combinations in an
offscreen Qt process. The generated empty-workspace reference set is under
`docs/qt-rewrite/visual-baseline/empty/` in the four variant directories.

The representative capture test uses the same four variants with the
reproducible startup workspace. Set `CLASSMNGR_VISUAL_BASELINE_OUTPUT_DIR` when
running that test to retain the generated populated PNGs; otherwise it uses a
temporary output directory. This keeps ordinary CTest runs non-mutating while
providing a repeatable capture command for permanent evidence.

The current representative startup matrix is retained under
`docs/qt-rewrite/visual-baseline/representative/`, alongside the empty-workspace
matrix. These are Debug/offscreen reference frames from the current source
snapshot; packaged Release captures are retained separately under
`docs/qt-rewrite/visual-baseline/release/`.

For an in-process lifecycle trace, add
`--startup-performance-workflow` to a representative startup run. It drives
every registered top-level route, opens the deferred Calendar child once,
returns to My Workspace, and then starts the requested settled checkpoints.
The trace records `workflow-page-*` checkpoints plus `page-enter` and
`page-leave` events. Set `CLASSMNGR_STARTUP_WORKFLOW_TRACE_PATH` when a
step-by-step diagnostic trace is needed while investigating a child-process
failure.

To retain a canonical fixture for a packaged run, set
`CLASSMNGR_STARTUP_FIXTURE_OUTPUT_PATH` for the representative fixture,
`CLASSMNGR_LARGE_STARTUP_FIXTURE_OUTPUT_PATH` for the large fixture, or
`CLASSMNGR_LEGACY_STARTUP_FIXTURE_OUTPUT_PATH` for the migrated legacy fixture
while running the corresponding test. With the variables unset, the tests
continue to use temporary fixtures and ordinary CTest remains non-mutating.

Set `CLASSMNGR_CLASS_TRANSFER_FIXTURE_OUTPUT_PATH` while running
`jsonRoundTripPreservesCompletePackage` to regenerate the transfer package.
Set `CLASSMNGR_CONFLICT_REVIEW_OUTPUT_PATH` while running
`permanentConflictFixturePresentsReviewAndRejectsScheduleCollision` to retain
the offscreen dialog PNG.

Set `CLASSMNGR_SCHEDULE_IMPORT_FIXTURE_OUTPUT_PATH` while running
`compactFlowAndReviewPresentation` to regenerate the permanent schedule
workbook. Set `CLASSMNGR_SCHEDULE_REVIEW_OUTPUT_PATH` while running
`suppliedWorkbookBuildsStagedReview` to retain the staged-review dialog PNG.
Set `CLASSMNGR_SCHEDULE_CONFLICT_FIXTURE_OUTPUT_PATH` while running
`permanentConflictWorkbookPresentsReviewWarning` to regenerate the larger
schedule conflict workbook. Set
`CLASSMNGR_SCHEDULE_CONFLICT_REVIEW_OUTPUT_PATH` during the same test to retain
its conflict-state dialog PNG.

Set `CLASSMNGR_SUB_PREP_OUTPUT_REFERENCE_DIR` while running
`largePackageGeneratesOutputReferenceWhenConfigured` to retain the heavy
96-class Sub Prep documents, first-page captures, and `manifest.json` under a
normal reference directory. The package itself is always generated in a
temporary target, so the retained files do not include empty per-class
directories. With the variable unset, the test still exercises the same
renderer against a temporary target and remains non-mutating.

Set `CLASSMNGR_LARGE_SUB_PREP_BOUNDARY_REFERENCE_DIR` while running
`capturesLargeSubPrepBoundaryWhenConfigured` to run the actual
`large_startup.sql` workspace through the packaged Release workflow and retain
`manifest.json`, the profiler JSON, process logs, and the flushed workflow
trace. The slot is opt-in because it intentionally drives the high-memory
large route; the manifest records whether the route completed or terminated.

Set `CLASSMNGR_LARGE_SCHEDULE_IMPORT_BOUNDARY_REFERENCE_DIR` while running
`capturesLargeScheduleImportBoundaryWhenConfigured` to run the actual
`large_startup.sql` workspace through the packaged Release workflow with the
generated large workbook. The test retains the workbook, source/review PNGs,
`manifest.json`, profiler JSON, process logs, and the flushed workflow trace
under the configured directory. The test is opt-in because it intentionally
drives the large import/review memory boundary.

Set `CLASSMNGR_LARGE_SCHEDULE_IMPORT_APPLY_BOUNDARY_REFERENCE_DIR` while
running `capturesLargeScheduleImportApplyBoundaryWhenConfigured` to run the
same packaged Release workflow through the actual transaction apply and
post-commit Schedule refresh. The apply-mode test uses a conflict-free variant
of the generated workbook, retains the source/review PNGs, `manifest.json`,
profiler JSON, process logs, and flushed workflow trace, and remains opt-in
because it intentionally drives the large import/apply memory boundary.

Set `CLASSMNGR_LARGE_CLASS_TRANSFER_BOUNDARY_REFERENCE_DIR` while running
`capturesLargeClassTransferBoundaryWhenConfigured` to run the generated
multi-class transfer package through the packaged Release workflow. The test
retains the package, review PNG, `manifest.json`, profiler JSON, process logs,
and flushed workflow trace under the configured directory. The test is opt-in
because it intentionally drives the large transfer review/apply memory
boundary.

Set `CLASSMNGR_LARGE_SPEAKING_EVALUATION_BOUNDARY_REFERENCE_DIR` while running
`capturesLargeSpeakingEvaluationBoundaryWhenConfigured` to augment the
96-class workspace and run the actual packaged Release Speaking Evaluation
page, report review, AI batch review, PDF export, cleanup, and refresh path.
The test retains the augmented fixture, five UI captures (including the
PowerPoint renderer-selection reference), individual PDFs and archive,
`manifest.json`, profiler JSON, process logs, and flushed workflow trace. The
reference proves the renderer option and fallback warning are present; it does
not execute external Office automation. The test is opt-in because it
intentionally drives the large speaking-report/output memory boundary.

Set `CLASSMNGR_LARGE_STAFF_DIRECTORY_BOUNDARY_REFERENCE_DIR` while running
`capturesLargeStaffDirectoryBoundaryWhenConfigured` to add the deterministic
96-row Native English Teacher and GS Team directories and run both actual
packaged Release pages through load, refresh, leave, re-entry, and release.
The test retains the generated fixture, two directory PNGs, `manifest.json`,
profiler JSON, process logs, and flushed workflow trace. The test is opt-in
because it intentionally drives the large directory memory boundary.

Set `CLASSMNGR_LARGE_SUB_PREP_OUTPUT_BOUNDARY_REFERENCE_DIR` while running
`capturesLargeSubPrepOutputBoundaryWhenConfigured` to run the actual packaged
Release Sub Prep generation dialog and package output path. The test retains
the valid large-workspace output fixture, configured and validation-error dialog
captures, generated Sub Prep and By Day PDFs, decoded first-page PNGs,
`manifest.json`, profiler JSON, process logs, and flushed workflow trace. The
test is opt-in because it intentionally drives the large Sub Prep
generation/output memory boundary.

For an empty workspace:

```powershell
ClassMngr.exe `
  --startup-visual-capture-output docs/qt-rewrite/visual-baseline/empty
```

For a populated workspace, pass a `.tps` or legacy `.db` path. A supplied
database selects the representative scenario automatically unless an explicit
`--startup-performance-scenario minimal` is provided. Use
`--startup-performance-settle-ms 5000` when a settled reference is needed.
The capture option can be combined with `--startup-performance-output` to
store the JSON startup trace beside the PNG files.

## Required runtime scenarios

1. Empty workspace startup.
2. Representative workspace startup.
3. First page render.
4. Five minutes idle.
5. Navigation through all normal pages.
6. Large schedule, roster, campus, document, speaking-evaluation, staff
   directory, and class transfer flows.
7. Open a representative PDF in QtPdf, exercise the required viewer actions,
   close or navigate away, and reopen it.
8. Leave each large feature and observe retained memory, including the viewer.
9. Repeated document open/close plus language/theme changes.
10. Heavy PDF/report/sub-prep/PowerPoint output.

## Evidence currently missing

- Feature-specific retained-memory measurements for the remaining large
  workflows beyond the route-level `workflow-page-left` checkpoints; the
  representative five-minute all-route workflow is retained under
  `visual-baseline/release/workflow-five-minute/`, and the Sub Prep, Classes,
  and Speaking Evaluation lifecycle boundaries are retained under their
  large-fixture directories.
- Windows ARM64 and Linux Release baselines; these unofficial ports are
  deferred and are not Phase 0 blockers. The macOS universal Release baseline
  is retained below.
- Packaged Release language/theme variants and visual references for editing,
  read-only, dialogs, loading, errors, and import conflict resolution; the
  populated Classes entry/selection/re-entry frames, Schedule Import loading
  dialog, and Calendar Import parser-failure/error boundary are now retained,
  while the other feature states remain open.
- Remaining report/output operation measurements beyond the retained Speaking
  Evaluation, Sub Prep output, and PDF viewer paths. The Staff Directory
  load/refresh/leave/re-entry, Class Transfer package
  review/commit/release, Schedule
  Import parse/review/cancel,
  apply/commit, and cleanup artifacts plus the Calendar workbook/import
  lifecycle and parser-failure boundary are now retained; the class-transfer
  conflict and compact schedule-workbook review fixtures are permanent.
- A v2 large-workspace Sub Prep route within its eventual memory budget; the
  current route completes but exceeds the representative working-set target
  and grows from a 2,948-widget/192-editor class-information graph to
  8,844/576 after two refreshes.
- A v2 large-workspace Classes route within its eventual memory budget; the
  current route keeps 473 navigation descendants but still materializes 192
  class-tab placeholders and repeats broad class-information/teacher queries
  on each refresh.
- Sub Prep visual references for loading and other states beyond the now-retained
  editing/read-only, selected, changed-selection, empty, both-language/theme,
  generated-output, and output-dialog validation-error references.
- Golden generated reports, substitute documents, additional roster variants,
  and PowerPoint output beyond the retained Speaking Evaluation PDF/archive
  and PowerPoint renderer-selection reference. The actual external Office
  automation path remains platform-specific and is not exercised by this
  Windows offscreen baseline.
- Remaining feature page/object lifecycle traces beyond the retained heavy
  workflow boundaries; per-resource payload, decoded-image, PDF/PPTX deferral,
  and resource-pack lease traces are now retained.

## Phase 0 evidence automation and gate status - 2026-09-17

The exit gate requires all 24 orchestrated route IDs on each required platform:
Windows x64 and macOS universal. A valid subset run is not a complete gate.
Windows ARM64 and Linux remain deferred unofficial ports. Runner and validator
usage, including gate-enforced validation, is documented in the
[automation README](../../scripts/phase0/README.md).

- The runner passed its PowerShell parse check and no-write all-route plan.
  Validator self-tests and an independent audit of the 24 route mappings and
  runner safety checks also passed.
- The historical retained Release tree passed artifact-integrity checks for
  28 manifests, 81 PNGs, 29 PDFs, and one ZIP. It contains no orchestrated
  route manifests. The 12-route macOS workflow below is baseline evidence and
  does not replace the required 24-route matrix.
- The fresh Windows x64 matrix passed, but macOS universal route evidence is
  still missing. Native Office automation did not complete in this Windows
  logon environment. Visual and generated-output semantic approval remains a
  human review; Phase 0 remains In Progress.

## Fresh Windows x64 route matrix - 2026-09-17

The fresh packaged Release run completed all 30 commands (five build/package,
24 route, and one final validation) with exit code 0 and no timeouts. The
independent audit confirmed the validator's per-run `pass=true`,
zero failures, and 151/151 required artifacts. Its sole warning is the legacy
memory trend. The separate combined exit gate remains `incomplete` because the
macOS universal matrix is still 0/24; Phase 1 remains blocked.

The resource trace contains 189 entries, ended with `processFinished=true`,
`exitCode=0`, and `timedOut=false`, and recorded a maximum working set of
496,005,120 bytes. It includes 495 samples at or above 250 MiB and none at or
above 512 MiB. This is legacy trend evidence: 250 MiB is the future rewrite
target, not a Phase 0 acceptance limit.

The [retained compact run record](phase-0-evidence/windows-x64-route-matrix-2026-09-17/)
contains `run-manifest.json`, `validation-summary.json`, and a SHA-256 TSV
inventory for the complete raw run root at
`C:\Users\wfelt\AppData\Local\Temp\ClassMngr-QT0-Windows-x64-final-20260916T201256Z\qt0-windows-x64-final-20260916T201256Z-msvc`.
The raw root has 331 files / 147,732,123 bytes and is not copied into Git. The
compact bundle is an audit record, not a validator evidence root; rerunning the
validator requires the full root. The run used Windows PowerShell 5.1.26100.9444,
MSVC 19.51.36257/toolset 14.51.36231, CMake 4.4.2, Ninja 1.13.2, Python
3.14.7, and Qt 6.12.0 MSVC x64.

The output-reference capture added 11 PDFs, 12 white-composited RGB PNG page
renders, and five manifests (8,504,264 bytes) under
`visual-baseline/release/windows-output-reference/`; the captures passed
structural and sample visual review. Office PowerPoint COM automation failed
before PDF generation with `0x80070520` in this logon environment. Two other
Speaking Evaluation UI CTest cases separately failed on the unavailable
noninteractive Windows clipboard (`0x800401d0`); the internal PDF capture slot
and focused roster slots passed. These limitations do not change the route
matrix result or indicate successful external Office automation.

## macOS universal Release baseline (2026-09-17)

The universal Release app and versioned DMG were rebuilt after setting the
macOS deployment minimum to 14.4. The Qt 6.12.0 kit used here also targets
14.4. A release validation pass inspected every embedded Mach-O: all 115
contained both `arm64` and `x86_64` slices and none requires a newer macOS
version. The staged app declares `LSMinimumSystemVersion=14.4`, its main binary
has a 14.4 minimum, its code signature verifies, and only `libqsqlite.dylib`
is present in the SQL plugin directory. The disk image checksum verified.

Build host: macOS 26.6.2, Apple Silicon arm64, Xcode 26.6 / AppleClang 21.0.0,
CMake 4.3.3, Ninja 1.13.2, and Qt 6.12.0 universal (`arm64;x86_64`). The clean
Release installer build used `macos-clang-release-installer`; the universal
Debug startup and dialog test targets were configured at the same 14.4
deployment target. The packaged app was exercised on this host, not on a
macOS 14.4 machine.

Empty-workspace startup completed in 2,906 ms with a 235,356,160-byte working
set, 237,273,088-byte peak working set, and 123,815,016-byte macOS physical
footprint. The populated 96-class startup completed in 3,137 ms at
235,470,848 bytes working set, then the complete 12-route workflow completed
in 9,407 ms. Its peak working set was 508,624,896 bytes; at the workflow
checkpoint it retained 9,678 widgets and all 11 registered pages. The route
performed 96 Sub Prep class-information, teacher, and roster lookups; loaded,
rendered, and released two PDFs; and returned to My Workspace. At the 1-second
settled checkpoint, the working set was 505,069,568 bytes and the physical
footprint was 402,179,512 bytes. These are legacy before-state measurements,
not v2 memory acceptance results.

Retained artifacts are in
`docs/qt-rewrite/visual-baseline/macos-universal/release/`: the
`manifest.json`, empty-startup profile, and 96-class workflow profile, trace,
startup frame, and settled frame. The empty-startup frame was not retained
because the initial setup page displays prefilled campus Wi-Fi credentials.
The 96-class source fixture is shared with the existing
`release/large-classes-visual-states/` fixture. The verified DMG SHA-256 is
`6ebc6d71e7fb9188d606999b6ab5e479e14422ca9e133126527c8ca2adf653ec`.
For reproduction, copy that `.tps` fixture before launching the workflow: the
application writes workspace state on exit.
