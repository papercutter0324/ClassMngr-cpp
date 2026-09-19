# Phase 1 — Build System and Repository Structure

## Status

- Status: In progress
- Default route: Heavy
- Depends on: Phase 0
- Blocks: Domain, persistence, resource, and UI implementation
- Owner: Unassigned
- Last updated: 2026-09-19
- Current note: Slices 1.1-1.6 establish the parallel executable, asserted
  target boundaries, explicit source ownership, tooling/reports, and a PR
  Debug matrix. The Windows Packaged Release workflow now also runs on
  relevant pull requests using the Qt-supported VS2022 hosted runner. Local
  Windows x64 Ninja/MSVC Debug configure/build and CTest passed 66/66 on source
  commit `4dbe3ca7` under VS 2026/MSVC 19.51; keep this as passing local
  evidence independently of hosted results. On hosted commit `57f5dff6`,
  the latest baseline results are Windows x64 Debug 66/66 and macOS universal
  Debug 67/67 in attempt 2 with JUnit evidence. Attempt 1 lost runner
  communication; no test failure was established as its cause. Attempt 3
  completed with an overall failure only because the informational Linux x64
  Debug job passed 65/66 and its startup checkpoint memory snapshot reported
  `available=false`. Windows and macOS Packaged Release workflows passed;
  Windows ARM64 cross-build and packaging passed as informational results.
  A later local macOS universal Debug build and full 67/67 CTest run also
  passed with normal macOS service access. Linux procfs memory sampling is
  fixed and targeted tests pass locally; full local CTest remains 66/67 because
  updater listener tests receive `EPERM` when creating sockets in the sandbox.
  No fresh hosted Linux or quality run has been verified. Phase 1 remains open
  for the hosted Phase 1 Build Quality workflow.

## Objective

Create a v2 build structure that reflects architectural boundaries and supports parallel migration without destabilizing the existing application.

## Work packages

### 1.1 Parallel executable

Add a v2 executable target named ClassMngrNext or ClassMngrV2.

Keep the current executable target buildable. Do not replace the production target until Phase 12.

#### Progress update - 2026-09-18

`ClassMngrNext` is introduced as a Qt Core-only console bootstrap in
`src/next/main.cpp`, with its target and `ClassMngrNextLaunch` CTest check
defined in `cmake/next.cmake`. A clean Ninja/MSVC Debug configuration built
both `ClassMngr` and `ClassMngrNext` in 351 steps, and
`ClassMngrNextLaunch` passed. This records the initial slice implementation;
the rest of Phase 1 remains in progress.

### 1.2 Target boundaries

Create explicit targets for:

- ClassMngrDomain.
- ClassMngrApplication.
- ClassMngrPersistence.
- ClassMngrResources.
- ClassMngrPlatform.
- ClassMngrUiShared.
- Feature-specific libraries.
- ClassMngrNext.

Production resources, test resources, and developer-only assets must be separate.

#### Progress update - 2026-09-18 (slice 1.2)

`cmake/next.cmake` now defines six source-free layer interface targets:
`ClassMngrNextDomain`, `ClassMngrNextApplication`,
`ClassMngrNextPersistence`, `ClassMngrNextResources`,
`ClassMngrNextPlatform`, and `ClassMngrNextUiShared`. It also defines eleven
source-free feature interface targets: `ClassMngrNextCalendar`,
`ClassMngrNextCampus`, `ClassMngrNextClasses`, `ClassMngrNextDocuments`,
`ClassMngrNextMyInfo`, `ClassMngrNextRoster`, `ClassMngrNextSchedule`,
`ClassMngrNextSetup`, `ClassMngrNextSpeakingEvaluation`,
`ClassMngrNextSubPrep`, and `ClassMngrNextTeacher`. Each has a
`ClassMngrNext::<Name>` alias.

The declared edges are `Application` to `Domain` and `Persistence`,
`Persistence` to `Domain`, `Platform` to `Application`, and `UiShared` to
`Application` and `Resources`. Each feature depends on `Application`,
`Resources`, and `UiShared`; `Domain` and `Resources` have no dependencies.
Features have no cross-feature edges. Configure-time assertions check these
exact relationships. The `ClassMngrNext` executable still links only Qt Core
and is independent of the placeholder targets.

A fresh Ninja/MSVC Debug configuration built `ClassMngr` and `ClassMngrNext`
in 351 steps; `ClassMngrNextLaunch` passed 1/1. Phase 1 remains in progress.

### 1.3 Dependency cleanup

Replace the broad shared Qt linkage model with target-specific dependencies.

The intended direction is:

- Domain has no Qt Widgets dependency.
- Application depends on domain contracts, not widgets.
- Persistence exposes application-facing interfaces.
- ResourceSystem exposes typed loading interfaces.
- UI depends on application and resource interfaces.
- Platform adapters implement interfaces owned by application or platform layers.

Remove unused Qt modules only after include and link usage has been measured.

#### Progress update - 2026-09-18 (slice 1.3)

`ClassMngrBuildSettings` provides shared `Qt6::Core`, C++23, the source and
generated include directories, and the `CLASSMNGR_SOURCE_DIR` definition.
Ninja dependency data measured these additional Qt and Zlib dependencies for
the six legacy production object targets:

| Object target | Additional dependencies |
|---|---|
| `ClassMngrCore` | `Qt6::Gui`, `Qt6::Network`, `Qt6::Sql`, `Qt6::Widgets`, `ZLIB::ZLIB` |
| `ClassMngrData` | `Qt6::Gui`, `Qt6::Sql` |
| `ClassMngrDomain` | `Qt6::Gui` |
| `ClassMngrUiShared` | `Qt6::Gui`, `Qt6::Network`, `Qt6::Pdf`, `Qt6::PdfWidgets`, `Qt6::PrintSupport`, `Qt6::Widgets` |
| `ClassMngrFeatures` | `Qt6::Concurrent`, `Qt6::Gui`, `Qt6::Network`, `Qt6::Pdf`, `Qt6::Qml`, `Qt6::Quick`, `Qt6::QuickWidgets`, `Qt6::Sql`, `Qt6::Widgets`, `ZLIB::ZLIB` |
| `ClassMngrAppServices` | `Qt6::Gui`, `Qt6::Network`, `Qt6::Sql`, `Qt6::Widgets` |

`ClassMngrRuntime` and the macOS `ClassMngrTestRuntime` deliberately retain
the complete legacy union: `Qt6::Concurrent`, `Qt6::Gui`, `Qt6::Network`,
`Qt6::Pdf`, `Qt6::PdfWidgets`, `Qt6::PrintSupport`, `Qt6::Qml`, `Qt6::Quick`,
`Qt6::QuickControls2`, `Qt6::QuickWidgets`, `Qt6::Sql`, `Qt6::Widgets`, and
`ZLIB::ZLIB`. `Qt6::QuickControls2` remains for the QML/resource dependency
graph. A compile command confirmed `ClassMngrDomain` has only QtCore and QtGui
include directories, with no Widgets, Sql, or Network includes.

A fresh Ninja/MSVC Debug configure/build succeeded for `ClassMngr` and
`ClassMngrNext` in 351 steps; `ClassMngrNextLaunch` passed 1/1. Independent
`ninja -t commands ClassMngr.exe` inspection confirmed the full legacy Qt link
set, including `Qt6::QuickControls2`. `ClassMngrSharedPolicyTests` built after
importing VS DevCmd, and its targeted CTest passed 1/1. A direct
`ClassMngrNext.exe` launch exited 0. Phase 1 remains in progress; slice 1.4,
explicit source ownership, is next.

The [Qt Rewrite Memory Hotspot Remediation
Plan](memory-hotspot-remediation-plan.md) is the allocation and ownership
companion to these target boundaries. The build must make it possible to keep
compact projections in non-UI targets, isolate model/view code in UI targets,
and keep import, report, and transfer operations behind testable boundaries.

### 1.4 Source ownership

Replace recursive source discovery with explicit source ownership.

Every source file must belong to exactly one production target or one test target. Generated files must be clearly separated from handwritten files.

#### Progress update - 2026-09-18 (slice 1.4 complete)

`cmake/production_sources.cmake` now explicitly lists the six legacy
production object targets:

| Target | Source entries |
|---|---:|
| `ClassMngrCore` | 56 |
| `ClassMngrData` | 42 |
| `ClassMngrDomain` | 43 |
| `ClassMngrUiShared` | 113 |
| `ClassMngrFeatures` | 288 |
| `ClassMngrAppServices` | 34 |

The feature manifest includes seven `.inc` fragments marked
`HEADER_FILE_ONLY`. `ClassMngr` owns `src/main.cpp` and the two explicit
calendar QML files; `ClassMngrNext` owns `src/next/main.cpp`. The generated
`src/core/build_info.h.in` remains a `configure_file` input.

`cmake/source_ownership.cmake` compares the handwritten `src/` and `tests/`
inventory with explicit owners and rejects missing or duplicate ownership. It
excludes the Apple-only PowerPoint data-access notice test from non-Apple
inventory.

The five schedule-widget test consumers share the
`ClassMngrScheduleWidgetTestSupport` object target. The `ResourcePackManager`
fake is split into `ClassMngrScheduleWidgetResourcePackTestSupport` for four
consumers; `ClassMngrClassesPageTests` uses the real manager. A clean Windows
Ninja/MSVC Debug configure reported 653 handwritten files. The build passed
for `ClassMngr`, `ClassMngrNext`, and the five affected test targets. Six
targeted CTests passed: `ClassMngrScheduleWidgetTests`,
`ClassMngrTestingClassesPageTests`, `ClassMngrClassesPageTests`,
`ClassMngrScheduleImportDialogTests`, `ClassMngrSubPrepPageTests`, and
`ClassMngrNextLaunch`. This is Windows-only evidence; no cross-platform
verification is claimed. Phase 1 remains in progress, with slice 1.5, tooling
and CI, next.

### 1.5 Tooling and CI

Add:

- Compile commands.
- Formatting checks.
- Static analysis.
- Dependency-boundary checks.
- Direct Qt dialog policy checks.
- Packaged Release builds.
- Platform deployment checks.
- Executable and resource-size reports.
- Linked Qt module reports.
- Resource-pack reference checks.
- Startup and memory test entry points.

#### Progress update - 2026-09-18 (slice 1.5)

`CMAKE_EXPORT_COMPILE_COMMANDS` provides the compile database. Configure fails
if `ClassMngrNext` links beyond Qt Core or the handwritten source-ownership
inventory has unassigned or multiply owned files.
`.github/workflows/phase1-quality.yml` scopes `clang-format` and `clang-tidy`
to `src/next`, verifies the compile database, builds `ClassMngr` and
`ClassMngrNext`, runs the resource-reference/report checks, and invokes
`ClassMngrNextLaunch`.

`cmake/build_reports.cmake` emits the Qt module-link report and
`cmake/resources.cmake` emits the resource-pack manifest. The reference checker
validates manager declarations, source references, runtime IDs, and generated
or staged RCC files; the build-report script reports executable, RCC, and Qt
module data. Windows, macOS, and Linux packaged Release workflows are wired to
invoke these checks on staged packages. `ClassMngrStartupPerformanceTests` is
the labeled CTest entry point for startup, memory, and performance
(`startup;memory;performance`).

The direct Qt dialog policy is part of the Phase 1 dependency-boundary gate.
`QMessageBox` and other concrete dialog types remain confined to the approved
shared dialog adapters; the legacy `src/main.cpp` startup/performance
orchestration must use a Qt-type-free compatibility driver for prompt
inspection, dismissal, default-action activation, and required captures. Do
not widen the policy allowlist for the composition root or feature code.

The compatibility driver is a temporary legacy bridge, not a new application
contract. Keep the semantic `IUserPromptService` operations separate from
test/automation operations, and place any concrete Qt implementation in the
existing shared dialog boundary. Verify the Sub Prep generation-warning,
schedule-import conflict-warning, and schedule-import confirmation flows with
the existing trace and screenshot assertions.

Local evidence is a clean Ninja/MSVC Debug configure, the full 351-step build
of `ClassMngr` and `ClassMngrNext`, and `ClassMngrNextLaunch` passing 1/1. The
resource-reference check passed for six generated RCCs and seven runtime IDs;
the staged-package build report passed. Local `clang-format`/`clang-tidy` were
not run and remain unverified.

#### GitHub Actions test reliability (open)

Record local and hosted results independently, with the source commit and
toolchain/platform for each. A successful local build or test run remains
passing local evidence even if GitHub Actions fails; a hosted failure is a
separate issue to diagnose and resolve. Do not erase local passes or describe
them as hosted validation. Phase 1's supported-build acceptance covers
Windows x64 and macOS universal. Linux and Windows ARM64 are unofficial,
informational builds and are deferred to later work; failures on those targets
do not block this phase.

For official Windows x64 and macOS universal jobs, use the job log, JUnit
report, and runner environment to identify the exact CTest target and QtTest
case when a test fails or stalls. Fix test or workflow assumptions while
preserving assertions, thresholds, and coverage; do not skip failing tests or
hide failures with `continue-on-error`. Rerun the complete affected official
Debug job after the fix. Record Linux and Windows ARM64 results as
informational; defer fixes for those unofficial builds.

#### Hosted workflow evidence - 2026-09-18

The local Windows x64 Ninja/MSVC Debug configure/build and full CTest pass
recorded above was run on source commit `4dbe3ca7` with VS 2026/MSVC 19.51 and
Qt 6.12. It passed 66/66 tests in 179.41 seconds. This remains valid local
evidence and is separate from the hosted run below.

The [Refactoring baseline run](https://github.com/papercutter0324/ClassMngr-cpp/actions/runs/35334835542)
used hosted commit `57f5dff6`. Its latest attempt (attempt 3) completed with an
overall failure because the informational Linux job failed:

- Official target: Windows x64 Debug passed 66/66 tests on the rerun,
  including `ClassMngrNextLaunch` and `ClassMngrStartupPerformanceTests`.
- Official target: macOS universal Debug passed 67/67 in attempt 2, with the
  JUnit artifact uploaded. Attempt 1 had lost runner communication; no test
  failure was established as its cause. The successful macOS result was
  retained in attempt 3.
- Unofficial target: Windows ARM64 Debug cross-build passed; the ARM64
  executables were not run. This result is informational and deferred.
- Unofficial target: Linux x64 Debug configure/build passed, but CTest passed
  65/66. The failure was
  `ClassMngrStartupPerformanceTests::reportsStartupMetricsAndHonorsThresholds`:
  the checkpoint memory snapshot reported `available=false` at
  `tests/startup_performance_tests.cpp:2515`. This result is informational and
  deferred.

The [Windows Packaged Release run](https://github.com/papercutter0324/ClassMngr-cpp/actions/runs/35334838494)
passed in attempt 2 for x64 and ARM64. The x64 run included the packaged
startup smoke test, architecture check, resource-reference check, and build
report. The [macOS Packaged Release run](https://github.com/papercutter0324/ClassMngr-cpp/actions/runs/35334841306)
passed. The [Linux Packaged Release run](https://github.com/papercutter0324/ClassMngr-cpp/actions/runs/35334844253)
also passed as informational evidence.

The Phase 1 Build Quality workflow has no recorded run, so hosted
formatting/static-analysis and its platform-independent checks remain
unverified. The remaining Phase 1 gate is that workflow. The Linux
memory-snapshot fix has not yet been verified by a fresh hosted run; native
Windows ARM64 launch remains deferred with that unofficial build. Preserve
test coverage and assertions while resolving the quality gate.

The `Qt-Rewrite` push triggers for `refactoring-baseline.yml` and
`windows-release.yml` were committed and pushed in `2154d56d`. A matching push
to the baseline workflow runs its full platform matrix; the Windows Release
workflow builds x64 and ARM64. The hosted results above used source commit
`57f5dff6`. The bounded macOS retry and isolated local rerun changes below are
not yet part of the pushed branch.

#### Linux memory snapshot follow-up - 2026-09-19

The earlier hosted [Refactoring baseline run](https://github.com/papercutter0324/ClassMngr-cpp/actions/runs/35334835542),
on commit `57f5dff6`, remains historical: its Linux x64 Debug job passed 65/66
and `/proc/self/status` RSS sampling reported unavailable. The cause was
`QFile::atEnd()` treating zero-sized procfs pseudo-files as exhausted. Commit
`898cd3fc` reads status lines until `readLine()` returns empty and supports an
injected procfs root. `ClassMngrProcessMemorySnapshotTests` and
`ClassMngrStartupPerformanceTests` passed locally (startup target 1/1,
43.44 seconds). Snapshot tests cover parsing and units, fallback behavior,
unavailable RSS, and live sampling. Full local CTest passed 66/67; 11
`ClassMngrUpdaterTests` listener failures were caused by sandbox socket
creation returning `EPERM`.

The separate Linux Phase 0 runner and workflow are in commit `749c9ba6`;
their local Xvfb limitation is recorded in the [Phase 0 baseline
log](../../docs/qt-rewrite/phase-0-baseline.md#supplemental-linux-phase-0-automation).
Local build, resource-reference/report, and `ClassMngrNextLaunch` checks passed.
`clang-format`, `clang-tidy`, and `actionlint` were unavailable; PyYAML parsed
the workflows. No hosted run after the earlier result is verified. Phase 1
remains open for the hosted Build Quality gate.

### 1.6 Build configurations

Define and validate:

- Debug.
- Release.
- Packaged Release.
- Official Phase 1 targets: Windows x64 and macOS universal.
- Unofficial builds deferred to later work: Windows ARM64 and Linux.

The packaged Release configuration must use the same deployment process that will be used for real distribution.

#### Progress update - 2026-09-18 (slice 1.6)

`.github/workflows/refactoring-baseline.yml` now runs PR-triggered Debug
validation for Windows x64, Windows ARM64, macOS universal, and Linux. Native
jobs use the existing baseline build/test script, including
`ClassMngrNextLaunch`; the ARM64 job cross-builds both executables on x64 and
does not run them. Existing Windows installer, macOS DMG, and Linux install
tree archive workflows remain the Packaged Release paths.

Independent static review passed the four-preset matrix, path filters, Qt
host/target setup, execution policy, and preset-list/JSON assertions. From a
clean source snapshot at `6f2f5fb0`, an elevated local Windows x64 Debug
configure/build and CTest passed 66/66, including `ClassMngrNextLaunch` and
`ClassMngrStartupPerformanceTests`.

A local Windows x64 Release run with VS 2026/MSVC 19.51 and Qt 6.12 built
`ClassMngr`, ran `windeployqt`, installed the staged tree, and compiled the
production Inno installer. Staged startup exited 0 in 3.6 seconds with
`finalProgress=100`; resource/report checks passed for six RCCs, seven runtime
IDs, and seven references. The staged tree contained 129 files (207,477,952
bytes), and the installer was 93,160,351 bytes. These VS 2026 results are
supplemental and do not validate the VS2022 shipping toolchain: the exact VS17
configure failed because no VS2022 instance is installed; VS18 generator
attempts hit FileTracker access failures. Elevated Ninja/MSVC succeeded.

At this earlier local checkpoint, macOS 27.0 arm64 validation built the
universal Debug targets and
packaged Release DMG, verified architecture/minimum-version constraints,
resource references, and reports. Its restricted full CTest run was 62/67;
focused reruns passed, and no aggregate macOS suite pass was claimed at that
checkpoint. The later full-suite follow-up is recorded below. Linux and
Windows ARM64 are unofficial and deferred; no local builds for those targets
are claimed. The latest detailed macOS evidence and environment findings are
in `agent_docs/latest_session_work.md`.

#### Progress update - 2026-09-18 (packaged Release pull-request coverage)

The Windows Release workflow now has the same relevant-source pull-request
coverage as the macOS and Linux package workflows. Its `windows-2022` jobs use
Visual Studio 2022 and Qt 6.12, build x64 and ARM64 installers, and upload the
checksums, build reports, and resource-reference reports. The PR Debug matrix
and quality workflow cover native builds/tests, the ARM64 cross-build,
formatting/static analysis, and `ClassMngrNextLaunch`. Phase 1 acceptance is
gated on official Windows x64 and macOS universal results; Linux and Windows
ARM64 remain informational and deferred.

#### Progress update - 2026-09-18 (Qt update retry and Windows test reliability)

After the local Qt update completed, a fresh Windows x64 Ninja/MSVC Debug
configure and full build passed on the working tree based on source commit
`4dbe3ca7`. CMake's explicit ownership check passed for 653 handwritten files,
and CTest passed 66/66 in 179.41 seconds. The resource-reference check passed
for six RCC packs, seven runtime IDs, and seven references; a build report was
generated. The local compiler was VS 2026/MSVC 19.51, so the result supplements
the VS2022 hosted validation.

Windows CTest now prepends the selected Qt runtime directory for each target
test and `ClassMngrNextLaunch`, instead of depending on the machine's global
`PATH`. `StartupVisualSettingsTests` uses a build-local settings root. The
startup performance test runs serially because concurrent heavy tests pushed
its measured startup from about three to eight seconds and caused its first
full-suite run to fail. Focused reruns passed, followed by the complete 66/66
CTest pass. At this checkpoint, hosted results remained pending; later hosted
Packaged Release runs passed, while the official Debug matrix remains open.

#### Progress update - 2026-09-18 (macOS baseline recovery and local full-suite run)

The baseline runner now configures and builds in the explicit `--build-dir`,
allowing local verification in a fresh directory without replacing existing
CTest evidence. The macOS workflow adds a downstream job that inspects the
failed macOS matrix job and its report artifact. On the first workflow attempt,
it requests one rerun with runner debug logging only when the failed macOS job
published no result artifact. A published baseline artifact preserves the
original outcome without retry. If GitHub denies the rerun, the original
baseline remains failed for manual review. Five local unit tests cover the
retry decision. This automation has not yet been exercised on a hosted run.

On macOS 27.0 arm64 with Qt 6.12.0, a clean universal Debug configure passed
the explicit source-ownership check for 654 handwritten files, and the full
build passed all 656 steps for `ClassMngr`, `ClassMngrNext`, and the tests.
Both executables contain arm64 and x86_64 slices, target macOS 14.4, and
`ClassMngrNext` links only Qt Core. The first CTest run in the restricted
environment passed 62/67; its five failures came from LaunchServices, display,
pasteboard, and loopback restrictions. Running the complete suite against the
same clean build with normal macOS service access passed 67/67 in 67.70
seconds. The passing JUnit report is
`build/phase1-macos-debug-local-20260918/Testing/normal-services.junit.xml`;
the full test log is `build/phase1-macos-debug-local-20260918/Testing/Temporary/LastTest.log`.
This is separate local evidence. The hosted macOS universal Debug gate passed
67/67 in attempt 2 above; the new retry code itself has not yet run on GitHub.

## Architectural rules

- No domain target links Qt Widgets.
- No persistence target exposes widget types.
- No feature target reaches into another feature's private source.
- UI code does not issue SQL.
- New code does not add process-wide mutable singletons.
- New caches require an explicit budget and release policy.
- New large structures require an ownership explanation.
- New platform code is isolated behind an interface.
- Memory instrumentation and large-data fixtures must have explicit test-target
  ownership and must not be deployed as production resources.

## Deliverables

- V2 executable target.
- Layered CMake targets.
- Explicit source lists.
- Platform build matrix.
- Packaged Release CI artifacts.
- Dependency and resource checks.
- Build-size and deployment reports.

## Exit gate

The v2 shell builds and launches on the official Phase 1 platforms: Windows
x64 and macOS universal. The current application still builds, and existing
tests remain runnable. Unofficial Linux and Windows ARM64 builds are outside
this phase's acceptance gate and are deferred.

The build can identify which target owns every production source file and which Qt modules each target actually requires.

## Heavy-route requirements

- For every Phase 1 slice, use the heavy route: establish the intended v2
  boundary, verify the slice from a clean build, and keep any legacy bridge
  explicitly temporary with a removal point.
- Do not copy the existing monolithic target structure into v2.
- Do not create a second native UI implementation.
- Do not delete current source during this phase.
- Do not claim memory improvements from binary-size changes alone.
- Make the v2 build reproducible from a clean checkout.
