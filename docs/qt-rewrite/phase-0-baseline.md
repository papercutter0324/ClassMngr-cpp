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
- Reviewable Release artifacts are retained at
  `docs/qt-rewrite/visual-baseline/release/empty/` and
  `docs/qt-rewrite/visual-baseline/release/representative/`, with the large
  and legacy runs in their matching subdirectories. Each directory contains
  `startup-complete.png`, `settled-final.png`, and `startup-metrics.json`. The
  populated frames show the expected workspace schedule grid; the
  four-language/theme matrices remain available in the Debug/offscreen
  reference directories.

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

- Packaged Release per-workflow memory reports, five-minute idle data, and
  retained-memory measurements after leaving large features.
- Windows ARM64, macOS universal, and Linux Release baselines.
- Packaged Release language/theme variants and visual references for editing,
  read-only, dialogs, loading, errors, and import conflict resolution.
- A large schedule-workbook/conflict fixture; the class-transfer conflict and
  schedule-workbook staged-review fixtures are now permanent.
- Golden generated PDFs, reports, rosters, substitute documents, and
  PowerPoint output.
- QtPdf on-demand open/close traces proving that startup has no loaded PDF and
  that the document resource is released after the viewer session ends.
- Per-resource decoded/resident sizes and page/object lifecycle traces.
