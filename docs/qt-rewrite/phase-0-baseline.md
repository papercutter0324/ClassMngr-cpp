# Qt Rewrite Phase 0 - Baseline and Evidence Log

Status: In progress
Snapshot: `75755460`
Date started: `2026-09-15`

## Evidence policy

Only clean, freshly configured builds from the current source snapshot are
authoritative. Existing build directories are useful for discovery but are not
accepted as baseline evidence because they contain stale cache variables from
an older tree. The obsolete historical docs were removed from `docs/`; this log
does not recreate or rely on them.

## Initial static evidence

- Source snapshot: `75755460`.
- Raw assets: 226 files, 65,729,685 bytes.
- Current source inventory: 58 core, 42 data, 43 domain, 115 shared UI, 283
  feature, and 34 app files; 73 test files.
- A stale Windows x64 Debug build enumerated 66 CTest tests. It must be
  reconfigured before test results are attached to this baseline.
- Current startup code still includes a splash screen and resource-pack
  initialization; these are explicit v2 removal/replacement acceptance items,
  not changes made in Phase 0.

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

- Source state: `bfb80585` plus the Phase 0 fixture changes in the worktree.
- Build directory: `build/qt-rewrite-phase0-startup-ninja`.
- Configuration: clean Ninja Debug, Windows x64, Qt `6.12.0`, MSVC
  `19.51.36256.0`, `BUILD_TESTING=ON`, and startup update checks disabled.
- Configure and generation succeeded; configure took `398.8 s` because the
  current Qt setup scans the repository's many test targets.
- The focused target build completed all `352` steps, including
  `src/features/my_info/ui/my_classes_page_content.cpp`, the application, and
  `ClassMngrStartupPerformanceTests.exe`.
- `ClassMngrStartupPerformanceTests.exe -v1` passed both tests with exit code
  `0`. The representative profile was materialized from
  `tests/fixtures/workspaces/representative_startup.sql` through the current
  schema manager.
- Representative checkpoints reported `202,162,176` bytes peak working set at
  `startup-complete` and `221,011,968` bytes at the five-second settled
  checkpoint, with `153,092,096` bytes private usage at the latter checkpoint.
  These are Debug/offscreen measurements and are preliminary, not the packaged
  Release gate.
- The existing Visual Studio build directory still has a Qt QML metadata
  regeneration loop and remains excluded from acceptance evidence.

## Fixture added in this pass

`tests/fixtures/workspaces/representative_startup.sql` is the permanent,
reviewable source for the representative startup profile. The startup test
creates a temporary `.tps` database, applies the latest schema, executes the
fixture statements, and then validates integrity plus the expected teachers,
classes, schedules, roster rows, and saved schedule settings. This removes the
dependency on the deleted startup-optimization plan and makes the fixture
reproducible on a clean checkout.

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
  -DCMAKE_INSTALL_PREFIX=dist/qt-rewrite-phase0-windows-x64
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
7. Leave each large feature and observe retained memory.
8. Repeated open/close plus language/theme changes.
9. Heavy PDF/report/sub-prep/PowerPoint output.

## Evidence currently missing

- Clean packaged Release build/test result from the current source snapshot.
- Packaged Release startup and per-workflow memory reports.
- English/Korean and light/dark screenshot set.
- Empty, normal, large, legacy, conflict, corrupt, and locked fixtures.
- Golden generated PDFs, reports, rosters, substitute documents, and
  PowerPoint output.
- Per-resource decoded/resident sizes and page/object lifecycle traces.
