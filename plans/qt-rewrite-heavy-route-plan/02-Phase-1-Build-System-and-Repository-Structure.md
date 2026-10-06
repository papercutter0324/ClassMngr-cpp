# Phase 1 — Build System and Repository Structure

Read [00-Start-Here.md](00-Start-Here.md) first for the overall plan, workflow, and phase sequence.

## Mandatory route for new and resumed work

- Use the Heavy route for every task, slice, follow-up, and reopened gate in this phase. `Default route: Heavy` is a binding instruction.
- At each start or resume, explicitly select Heavy and reread this phase plan and [00-Start-Here.md](00-Start-Here.md). Follow the active `AGENTS.md` Heavy-route instructions and `~/.codex/codex_workflow/heavy_route.md`, including their delegation, verification, and deployment-state requirements.
- A new session, handoff, or context reset does not change the route. Do not continue under Light or Medium. If a required Heavy-route step blocks progress, report the blocker before implementation instead of silently switching routes.

## Build and test verification

- Use `build/windows-x64-debug` as the standard local Windows x64 Debug
  build folder for slice work. Reuse this same configured folder across
  slices and phases; do not create slice-, task-, or reviewer-specific build
  folders. Build the affected targets and their dependencies, and let the
  build system recompile changed or out-of-date inputs.
- If a fresh build is needed, empty the applicable standard build folder
  before configuring and building in it. For Windows x64 Debug, keep using
  `build/windows-x64-debug`; do not create a new folder for the fresh build.
  Apply the same rule when changing the build system, toolchain, or
  dependency configuration; when stale artifacts could explain a result;
  or when a phase gate explicitly requires clean-checkout evidence.
- A gate requiring another platform or configuration must use that CMake
  preset's standard `build/<preset-name>` folder and reuse it for that
  preset, rather than creating a slice-specific folder.
- Keep any phase-specific full builds, test suites, platform matrices, and
  packaging checks required by an exit gate. Record whether verification
  used an incremental or fresh build, which targets were built, and which
  tests ran. Describe focused results as focused; do not report them as a
  full-suite pass.

## Status

- Status: Complete
- Default route: Heavy
- Depends on: Phase 0
- Blocks: Domain, persistence, resource, and UI implementation
- Owner: Unassigned
- Last updated: 2026-09-19
- Historical progress log: [02-Phase-1-Progress-Log.md](02-Phase-1-Progress-Log.md)
- Current note: Slices 1.1-1.6 established the parallel executable, asserted
  target boundaries, explicit source ownership, tooling/reports, and the PR
  Debug matrix. Hosted commit `0883009d` passed the Windows x64, macOS
  universal, Linux x64, and Windows ARM64 baseline jobs; the Phase 1 Build
  Quality, Dialog policy, and Windows/macOS/Linux Release workflows also
  passed. Phase 1's hosted acceptance gate is closed. The additive Phase 2
  domain-contract slice is locally verified on top of that baseline.

### Slice discovery batches

Discover upcoming slices in ordered batches of up to ten (or all remaining
slices if fewer than ten remain). Record each batch as an ordered list under
`Recorded batches` below and work through those slices in order. Begin discovering
and recording the next batch when starting work on the second-last slice in the
current batch. If a discovery pass finds fewer than ten slices, add the exact
standalone line `No other slices were found.` beneath that batch.

Keep the Status `Current note` limited to the latest information relevant to the
current or next slice. Keep only the most recent slice commit in the
`Latest Progress Update` section. When writing a newer update, move the previous
one to this phase's progress log before replacing it.

#### Recorded batches

Add each ordered discovery result here as it is found.

## Objective

Create a v2 build structure that reflects architectural boundaries and supports parallel migration without destabilizing the existing application.

## Work packages

### 1.1 Parallel executable

Add a v2 executable target named ClassMngrNext or ClassMngrV2.

Keep the current executable target buildable. Do not replace the production target until Phase 12.

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

### 1.4 Source ownership

Replace recursive source discovery with explicit source ownership.

Every source file must belong to exactly one production target or one test target. Generated files must be clearly separated from handwritten files.

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

### 1.6 Build configurations

Define and validate:

- Debug.
- Release.
- Packaged Release.
- Official Phase 1 targets: Windows x64 and macOS universal.
- Unofficial builds deferred to later work: Windows ARM64 and Linux.

The packaged Release configuration must use the same deployment process that will be used for real distribution.

#### Closure update - 2026-09-19

The hosted `Refactoring baseline` run [35424488211](https://github.com/papercutter0324/ClassMngr-cpp/actions/runs/35424488211)
passed all matrix jobs, including `macOS universal Debug`, `Windows 11 x64
Debug`, `Linux x64 Debug`, and the informational `Windows ARM64 Debug
cross-build`, on commit `0883009d`.

The hosted `Phase 1 Build Quality` run [35424488214](https://github.com/papercutter0324/ClassMngr-cpp/actions/runs/35424488214)
passed its compile-database, `src/next` formatting/static-analysis,
ClassMngr/ClassMngrNext build, resource/report, and `ClassMngrNextLaunch`
checks. The hosted [Dialog policy](https://github.com/papercutter0324/ClassMngr-cpp/actions/runs/35424488244)
and packaged [Windows](https://github.com/papercutter0324/ClassMngr-cpp/actions/runs/35424488203),
[macOS](https://github.com/papercutter0324/ClassMngr-cpp/actions/runs/35424488198),
and [Linux](https://github.com/papercutter0324/ClassMngr-cpp/actions/runs/35424488209)
release workflows also passed. The Phase 1 exit gate is satisfied; Linux and
Windows ARM64 remain informational for this phase.

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
  boundary and verify it from the standard build folder. Reuse the configured
  folder for routine checks; when clean-checkout evidence is required, empty
  that same standard folder before configuring and building. Keep any legacy
  bridge explicitly temporary with a removal point.
- Do not copy the existing monolithic target structure into v2.
- Do not create a second native UI implementation.
- Do not delete current source during this phase.
- Do not claim memory improvements from binary-size changes alone.
- Make the v2 build reproducible from a clean checkout.
