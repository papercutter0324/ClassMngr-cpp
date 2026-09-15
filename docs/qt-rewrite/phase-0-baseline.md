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
  usage was `297,848,832` bytes. This remains below the 250 MiB Windows
  working-set target but is a tighter regression boundary than the route-only
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
  contains the 96-class-scale `Sub Prep.pdf` and Daily roster PDF, their
  first-page PNGs, and a manifest with document sizes, page counts, and render
  dimensions.

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

The heavy Sub Prep output slot mirrors the large fixture's 24 teachers, 96
classes, and 7,200 roster cells in an operation-scoped package request. The
current output oracle generates a 19-page Sub Prep PDF and a 16-page Daily
roster PDF, then reopens and renders the first page of each before accepting
the package. This isolates generated-output behavior from the current large
route's high-memory UI boundary while keeping the same stress cardinality.

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

The same release process must later be run on Windows ARM64, macOS universal,
and Linux. Runtime memory reports must record commit, private bytes, working
set/Resident Set Size, peak working set, handles, threads, and the exact
scenario/fixture.

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
6. Large schedule, roster, campus, document, and speaking-evaluation flows.
7. Open a representative PDF in QtPdf, exercise the required viewer actions,
   close or navigate away, and reopen it.
8. Leave each large feature and observe retained memory, including the viewer.
9. Repeated document open/close plus language/theme changes.
10. Heavy PDF/report/sub-prep/PowerPoint output.

## Evidence currently missing

- Feature-specific retained-memory measurements beyond the route-level
  `workflow-page-left` checkpoints; the representative five-minute all-route
  workflow is retained under `visual-baseline/release/workflow-five-minute/`.
- Windows ARM64, macOS universal, and Linux Release baselines.
- Packaged Release language/theme variants and visual references for editing,
  read-only, dialogs, loading, errors, and import conflict resolution.
- Golden large schedule-workbook/conflict output; the class-transfer conflict
  and schedule-workbook review fixtures are now permanent.
- A completed large-workspace Sub Prep route: the current 96-class fixture
- A v2 large-workspace Sub Prep route within its eventual memory budget; the
  current route completes but exceeds the representative working-set target
  with a 2,948-widget/192-editor class-information graph.
- Feature-specific Sub Prep measurements beyond this retained Release
  boundary, including refresh/re-entry/release behavior, actual database
  query/result sizes, and the final v2 budget thresholds.
- Sub Prep visual references for the selected, changed-selection, empty, both
  language/theme, and generated-output states, plus an explicit packaged
  Release memory budget for the v2 acceptance gate.
- Golden generated reports, rosters, substitute documents, and PowerPoint
  output beyond the retained heavy Sub Prep PDF/package reference.
- Per-resource decoded/resident sizes and page/object lifecycle traces.
