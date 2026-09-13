# Phase 1 — Dependency Control and Provenance

**Previous:** [plan overview](00-START-HERE.md)

**Next:** [Phase 2](02-phase-2-winui-build-integration-spike.md) and [Phase 3](03-phase-3-reader-contract-and-format-boundary.md)

## Goal

Make OpenXLSX a reproducible, auditable dependency before product source files
include it or the WinUI build relies on it.

## Why This Is a Separate Phase

`C:\Git\openxlsx` is useful for evaluation, but it is not sufficient release
provenance: it has no local Git metadata, and its visible version declarations
are inconsistent (CMake: 0.5.2; `vcpkg.json`: 0.5.1). Using that path directly
would make developer machines and CI build different inputs.

## Scope

- Select a dependency acquisition model: committed vendor snapshot, a Git
  submodule pinned to a commit, or an internally mirrored source archive with
  checksum.
- Record source URL, immutable revision or archive digest, observed version,
  date acquired, and every local patch.
- Inventory OpenXLSX licensing and its enabled transitive dependencies.
- Define release and update ownership, including how version bumps are tested.
- Set a no-network-at-build policy for the selected packaging route.

## Out of Scope

- Adding OpenXLSX to application code.
- Changing the import dialog or schedule parsing.
- Selecting a different XLSX library merely to avoid this provenance work.

## Required Decisions

### Source layout

Prefer a project-contained, pinned dependency location such as
`third_party/openxlsx/`, or a submodule that resolves there. Do not configure
the product against `C:\Git\openxlsx`.

The selected model must answer all of the following:

- What exact source bytes are used by CI and developers?
- How can a clean checkout reproduce them without an uncontrolled download?
- Where are local patches documented and reviewed?
- How is a security or compatibility update applied and rolled back?

### Dependency features

Record the exact OpenXLSX options to be used. Expected initial posture:

- static OpenXLSX library;
- samples, tests, benchmarks, and documentation disabled for product builds;
- no opportunistic package download or `FetchContent` network fallback;
- explicit choice of ZIP and XML backends, including their licenses and
  Windows-runtime implications.

### Licensing and notices

OpenXLSX identifies as BSD-3-Clause in its local `LICENSE.md`. Confirm that
license and the licenses for every enabled dependency, then add the required
notice/attribution material in the project’s established third-party-notice
location. Do not infer transitive licenses from memory.

## Work Breakdown

1. Inspect the intended OpenXLSX source and its declared version, CMake
   options, dependency discovery, and license files.
2. Obtain an immutable upstream reference or archive checksum for the evaluated
   source. If the local tree includes uncommitted changes, identify and either
   discard them from the chosen source or carry them as explicit patches.
3. Choose and document the source layout and update policy.
4. Create the dependency manifest/notice entries required by this repository.
5. Configure a minimal offline dependency build in a clean work location to
   prove that the selected source does not silently fetch dependencies.
6. Record the configuration variables Phase 2 is allowed to consume.

## Expected File Areas

The exact files depend on the existing third-party convention, but this phase
normally affects only dependency metadata, notice files, and the chosen pinned
source location. It must not modify `ClassMngrEngine` or the import UI.

## Validation

- A clean checkout or clean dependency cache can identify the exact OpenXLSX
  source without relying on `C:\Git\openxlsx`.
- Dependency configuration succeeds with network access disabled or otherwise
  demonstrably does not download arbitrary content.
- The dependency manifest reconciles the selected source revision with the
  effective OpenXLSX version.
- Required license and attribution text is present and verified against the
  enabled source/dependency set.

## Exit Criteria

Phase 2 receives a fixed source path, exact build options, explicit
transitive-dependency strategy, and a documented license obligation. No product
build is allowed to depend on an unpinned developer-local directory.

## Risks and Responses

| Risk | Response |
| --- | --- |
| Version metadata mismatch hides a fork or partial copy | Treat it as untrusted evaluation material until an immutable source is selected. |
| A convenience package manager fetches different versions on CI | Pin versions and disable network fallback in the product path. |
| Static linking omits a required notice | Inventory enabled transitive libraries before release packaging. |
