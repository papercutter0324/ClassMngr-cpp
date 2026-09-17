# Phase 1 — Build System and Repository Structure

## Status

- Status: In progress
- Default route: Heavy
- Depends on: Phase 0
- Blocks: Domain, persistence, resource, and UI implementation
- Owner: Unassigned
- Last updated: 2026-09-18
- Current note: Slices 1.1-1.6 establish the parallel executable, asserted
  target boundaries, explicit source ownership, tooling/reports, and a PR
  Debug matrix. Independent static review of that matrix passed. Hosted Debug
  and Packaged Release results are pending; Phase 1 remains in progress.

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

Local evidence is a clean Ninja/MSVC Debug configure, the full 351-step build
of `ClassMngr` and `ClassMngrNext`, and `ClassMngrNextLaunch` passing 1/1. The
resource-reference check passed for six generated RCCs and seven runtime IDs;
the staged-package build report passed. Cross-platform CI and local
`clang-format`/`clang-tidy` were not run, so those checks remain unverified.
Phase 1 remains in progress.

### 1.6 Build configurations

Define and validate:

- Debug.
- Release.
- Packaged Release.
- Windows x64.
- Windows ARM64.
- macOS universal.
- Linux.

The packaged Release configuration must use the same deployment process that will be used for real distribution.

#### Progress update - 2026-09-18 (slice 1.6)

`.github/workflows/refactoring-baseline.yml` now runs PR-triggered Debug
validation for Windows x64, Windows ARM64, macOS universal, and Linux. Native
jobs use the existing baseline build/test script, including
`ClassMngrNextLaunch`; the ARM64 job cross-builds both executables on x64 and
does not run them. Existing Windows installer, macOS DMG, and Linux install
tree archive workflows remain the Packaged Release paths.

Independent static review passed the four-preset matrix, path filters, Qt
host/target setup, execution policy, and preset-list/JSON assertions. No
hosted job, ARM64 cross-build, actionlint/YAML parser, or new cross-platform
build has run. Phase 1 remains in progress. Resume/close only after hosted
Debug matrix and Packaged Release results are available and the Phase 1 exit
gate is reviewed.

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

The v2 shell builds and launches on all supported platforms. The current application still builds, and existing tests remain runnable.

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
