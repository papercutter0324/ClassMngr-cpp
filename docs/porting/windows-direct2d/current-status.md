# Windows Direct2D/DirectComposition port — current status

Last recorded: 2026-08-29 (Asia/Seoul)

This is the cross-device handoff for the Windows port. The Phase 0 baseline
gate was accepted by the product owner on 2026-08-29. Phase 1 build-split work
may proceed; native-platform parity checks remain carry-forward work for the
corresponding implementation slices.

## Repository snapshot

- Branch: `NativeWindowsPort`, at `origin/NativeWindowsPort`.
- HEAD: `9e9b15f` (`Fix the /proc memory reader code`).
- The current product source still has no native Direct2D/DirectComposition
  target; this status file and the capture artifacts are Phase 0 evidence.
- The current Phase 0 release gate is x64 only. ARM64 build compatibility is
  unofficial and informational; ARM64 runtime validation, baselines, and
  release support are deferred. UI Automation, Narrator, and high-contrast
  support are also deferred from the current roadmap.

## Phase 0 completed evidence

- Architecture decision record, feature inventory, parity matrix, fixture
  contract, capture protocol, and performance protocol are present.
- The fixture corpus contains eleven SHA-pinned fixtures and executable semantic
  and migration checks.
- The opt-in Windows Qt visual target now registers 56 scenarios and performs
  the production show/settle/capture/close/cleanup lifecycle. This includes
  28 deterministic app-owned dialog scenarios.
- Windows x64 Debug non-visual validation passed 67/67 tests, including the
  database-port fixture tests.
- The visual target passed 16/16 scenarios with
  `CLASSMNGR_PHASE0_DISPLAY_SCALE_PERCENT=100` on the replacement computer
  with one active 2560x1600 monitor. The final run is
  `20260828T083829986Z-18036`; every promoted sidecar reports 100% display
  scale, a 1270x1040 capture window, and `sourceRevision: d9732c8`.
  - The same visual target passed 16/16 scenarios with
  `CLASSMNGR_PHASE0_DISPLAY_SCALE_PERCENT=150` on the current 150%-scaled
  displays. The validated run is
  `20260828T080550115Z-12884`; every sidecar reports 150% display scale and
  `sourceRevision: d9732c8`. The stable 100% baseline remains retained for
  cross-scale comparison.
- The same visual target passed 16/16 scenarios with
  `CLASSMNGR_PHASE0_DISPLAY_SCALE_PERCENT=200` on one active monitor. The
  validated run is `20260828T091020942Z-21232`; every sidecar reports 200%
  display scale, a 1270x780 capture window, and `sourceRevision: d9732c8`.
- The expanded visual target passed 28/28 scenarios in final run
  `20260828T123139612Z-10236` at 150% display scale. It covers populated,
  empty, large, dirty, validation, and error states for the Classes editors;
  all 28 PNG/JSON pairs pass the artifact validator and report
  `sourceRevision: fb4f268`. The product owner reviewed the captures and
  accepted them as matching normal application use; no additional pre-native
  visual recapture is required. The run took 172.16 seconds against the then-current 180-second
  CTest timeout; the source timeout is now 600 seconds and focused filters are
  available before further matrix growth.
- The app-owned dialog subset now passes 28/28 scenarios in combined run
  `20260828T155125967Z-14692` at 150% display scale. It covers the
  deterministic initial-setup, calendar, class/teacher transfer, schedule,
  print-options, speaking-evaluation, substitute-preparation, record,
  updater, Preferences, About, License, and memory-monitor surfaces. All 28
  PNG/JSON pairs pass the artifact validator. The product owner reviewed the
  captures and accepted them as representative current-Qt evidence. Native
  file/folder/printer interaction and native control behavior remain separate
  implementation acceptance work.
- The product owner confirms that Korean IME composition works in routine use
  with the current Qt application. This closes the current-Qt baseline
  question; native editable controls must still prove equivalent IME behavior.
- Phase 0 contract validation passes: 80 capture rows, 21 parity rows, and
  eleven fixtures.
- The current x64 Debug startup probe has three runs at approximately 3.4 s
  startup and a maximum peak of 703.9 MiB working set / 599.6 MiB private
  bytes. These are diagnostic values, not the approved Release budget.
- Windows ARM64 Qt compilation/linking was established as unofficial build
  compatibility evidence. No ARM64 runtime validation is planned in the
  current roadmap.

## Linux Qt validation — complete

- The Linux-specific Phase 0 retained-platform gate is complete.
- `cmake --preset linux-gcc-debug` configured successfully with Qt 6.11.1;
  the retained `ClassMngr` executable and all 67 registered test targets
  rebuilt successfully.
- `ClassMngrDatabasePortFixtureTests` passed, verifying all eleven checked-in
  database fixtures and their migration/rollback expectations.
- The complete Linux Qt Debug suite passed 67/67 tests when run with
  `QT_QPA_PLATFORM=offscreen` and normal loopback access. All updater tests
  passed once the local HTTP test server was allowed to bind.
- The Linux Qt Release preset also configured and built successfully; its
  complete CTest suite passed 67/67 with loopback access.
- Release installation and deployment succeeded with bundled Qt libraries,
  QML files, plugins, translations, licenses, and resource packs. The
  installed XCB bundle launched successfully on the host display, and the
  generated Linux archive checksum verified.
- The Linux `/proc` memory reader now reads until `QFile::readLine()` returns a
  null value instead of using `QFile::atEnd()`, whose zero-size result for
  procfs files caused unavailable snapshots. The memory snapshot and
  dependent startup-memory tests pass.
- The recent Linux diagnostics pull is included in this handoff; its
  diagnostics are considered complete and no Linux diagnostics item blocks
  Phase 1.

## macOS Qt validation

Validated 2026-08-29 on macOS 26.6.2 using Qt 6.11.1, CMake 4.3.3, AppleClang
21.0.0, and an arm64 host. The Debug and Release builds used the universal
`arm64;x86_64` architecture setting and the macOS 13.0 deployment target.

- A clean-source Debug configure and rebuild completed successfully, including
  the retained `ClassMngr` application, shared runtime, and all 68 registered
  test targets.
- `ClassMngrDatabasePortFixtureTests` passed all eleven checked-in fixtures, including
  migration, rollback, and Korean-text semantic checks.
- The complete retained Qt CTest suite passed **68/68** in 29.89 seconds with
  loopback access enabled. The Initial Setup wizard test runs with Cocoa on
  Apple platforms because Qt 6.11.1 aborts when `QWizard` is forced through
  the offscreen plugin; other platforms retain their existing headless setup.
- A clean universal Release build completed successfully; `lipo -info`
  confirmed both `arm64` and `x86_64` slices.
- The only repository change for this validation is the test-platform
  selection in `cmake/tests/foundations.cmake`; application source,
  database schema, and macOS release settings were not changed.

The reproducible validation commands are:

```bash
export QT_MACOS_PREFIX=/Users/papercutter0324/Qt/6.11.1/macos
cmake --preset macos-clang-debug
cmake --build build/macos-clang-debug --parallel 2
build/macos-clang-debug/ClassMngrDatabasePortFixtureGenerator \
  --verify-directory tests/fixtures/database-port
env QT_QPA_PLATFORM=offscreen \
  ctest --test-dir build/macos-clang-debug --output-on-failure

cmake --preset macos-clang-release
cmake --build build/macos-clang-release --parallel 2
```

Run the suite on an interactive macOS session with permission for its local
loopback test server; the Cocoa wizard case cannot be validated by an
offscreen-only runner.

## Current visual evidence

The latest validated editor capture run is:

```text
artifacts/phase0/windows-qt-visual/20260828T123139612Z-10236/
```

It contains 28 PNG/JSON pairs, validated against the Phase 0 artifact
contract, at 150% display scale. The validated 100%/150%/200% baseline runs
remain available for cross-scale comparison. The stable 100% baseline at
`artifacts/phase0/windows-qt-visual/20260828T025557225Z-39876/` was replaced
with the single-monitor run `20260828T083829986Z-18036` so the canonical
evidence no longer includes the previous second-monitor capture. The validated
150% companion remains at
`artifacts/phase0/windows-qt-visual/20260828T080550115Z-12884/`; the 200% run
was generated on the same single-monitor setup.

The latest validated app-owned dialog capture run is:

```text
artifacts/phase0/windows-qt-visual/20260828T155125967Z-14692/
```

It contains 28 PNG/JSON pairs at 150% display scale and is validated against
the Phase 0 artifact contract. It is an owner-accepted current-Qt baseline;
it does not claim native parity.

The repository HEAD is `9e9b15f`; the promoted 100%/150%/200% capture sidecars
correctly retain their captured-product `sourceRevision: d9732c8`. The
expanded editor run records `sourceRevision: fb4f268` and is an owner-accepted
current-Qt baseline; it is not native parity or a requirement for a pixel-
identical native implementation.

## Phase 1 build-boundary implementation — local evidence

Phase 1 implementation is in progress in the working tree on top of HEAD
`9e9b15f`; this section records build-boundary evidence only and does not
promote native feature parity or close the Phase 1 cross-platform gate.

- Product selection now supports a Qt-only retained route, a dual-build
  transition route, and a Windows x64 native-only route. The native-only
  configure reports Qt desktop `OFF`, Windows native `ON`, and Qt transition
  `OFF`; its fresh Debug and Release caches contain zero `Qt6*_DIR` entries.
- `ClassMngrEngine` is a Qt/Win32-free static library containing the extracted
  semantic-version parser/comparator. Its ordinary CTest executable passes,
  and the retained `ClassMngrUpdaterTests` adapter/network test target passes
  after the extraction.
- The native Windows foundation selects SDK `10.0.26100.0`, compiles the
  Direct2D 1.3 capability guard, embeds the reviewed manifest, and builds the
  minimal `ClassMngrNative.exe` shell. Native Debug CTest passes **5/5**:
  engine, SDK capability, shell smoke, embedded manifest, and catalog-backed
  resource-manifest verification.
- Native Release installation and post-install smoke/manifest checks pass.
  The stage is `dist/ClassMngr-windows-native-x64/` and contains the native
  executable, a 190-entry SHA-256 resource manifest, and the three font
  licenses; it contains no Qt DLLs, QML/plugins/translations directories,
  `qt.conf`, or retained `ClassMngr.exe`. Local `dumpbin` inspection reports
  an x64 PE image with no Qt imports.
- A separate Windows native foundation workflow has been added. It installs
  no Qt, configures fresh native Debug/Release trees, runs the native gates,
  installs the Release stage, and checks the cache, PE architecture, imports,
  manifest, and Qt-free install tree. It has not yet run on GitHub Actions.
- The retained Windows Qt transition target still builds as `ClassMngr.exe`.
  The complete retained Windows Qt non-visual CTest suite passes **68/68**
  locally, and the opt-in Phase 0 visual-capture target rebuilds successfully
  through the transition graph. The earlier isolated speaking-evaluation
  batch-report failure was a clipboard timing limitation in a non-interactive
  invocation; the ordered suite passes it, so no retained-platform failure is
  currently attributed to the Phase 1 engine/build changes.

## Carry-forward native implementation validation

1. Validate the native file/folder/printer services, command flows, output
   artifacts, and native control focus as each Phase 1+ feature slice is
   implemented.
2. Re-test Korean IME, focus restoration, and unsaved-change behavior on the
   native controls. The current Qt baseline is owner-accepted; this is not a
   waiver of the native input gate.
3. Collect native Windows x64 Release, resize, scrolling, first-paint, output,
   and device-recovery measurements when the native target can produce them.

## Reproduce on another Windows device

From the repository root, with the required Qt 6.11.1 kit, Visual Studio,
Windows SDK, and an interactive monitor configured at 100%, 150%, or 200%
scaling:

```powershell
cmake --preset windows-x64-phase0-visual
cmake --build build/windows-x64-phase0-visual --config Debug --target ClassMngrWindowsQtVisualCaptureTests --parallel 2
$env:CLASSMNGR_PHASE0_DISPLAY_SCALE_PERCENT = "100" # or "150" or "200"
ctest --test-dir build/windows-x64-phase0-visual -C Debug -R ClassMngrWindowsQtVisualCaptureTests --output-on-failure
```

To run only the app-owned dialog candidates on an interactive Windows runner,
set the registry filter before `ctest`:

```powershell
$env:CLASSMNGR_PHASE0_SCENARIO_FILTER = "dialog.*"
$env:CLASSMNGR_PHASE0_DISPLAY_SCALE_PERCENT = "150"
ctest --test-dir build/windows-x64-phase0-visual -C Debug `
  -R ClassMngrWindowsQtVisualCaptureTests --output-on-failure
```

The test creates a timestamped directory below
`artifacts/phase0/windows-qt-visual/`. Validate the newly created directory
and the checked-in contracts with:

```powershell
.\scripts\porting\windows\validate_phase0_capture_artifacts.ps1 `
  -ArtifactRoot artifacts\phase0\windows-qt-visual\<new-run> `
  -ProjectRoot .
.\scripts\porting\windows\validate_phase0_contracts.ps1 -ProjectRoot .
```

## Documents of record

- [Phase 0 README](README.md)
- [Baseline results](phase-0/baseline-results.md)
- [Capture ledger](phase-0/capture-ledger.csv)
- [Performance baseline](phase-0/performance-baseline.md)
- [Port plan](../../../plans/windows-direct2d-directcomposition-port-plan.md)
