# Phase 1 — Build System and Repository Structure

## Status

- Status: Not started
- Default route: Heavy
- Depends on: Phase 0
- Blocks: Domain, persistence, resource, and UI implementation
- Owner: Unassigned
- Last updated: 2026-09-15
- Current note: Establish explicit target ownership before moving behavior.

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

The [Qt Rewrite Memory Hotspot Remediation
Plan](memory-hotspot-remediation-plan.md) is the allocation and ownership
companion to these target boundaries. The build must make it possible to keep
compact projections in non-UI targets, isolate model/view code in UI targets,
and keep import, report, and transfer operations behind testable boundaries.

### 1.4 Source ownership

Replace recursive source discovery with explicit source ownership.

Every source file must belong to exactly one production target or one test target. Generated files must be clearly separated from handwritten files.

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
