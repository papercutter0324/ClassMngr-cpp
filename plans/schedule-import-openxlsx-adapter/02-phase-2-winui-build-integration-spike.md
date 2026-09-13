# Phase 2 — WinUI Build Integration Spike

**Previous:** [Phase 1](01-phase-1-dependency-control-and-provenance.md)

**Next:** [Phase 3](03-phase-3-reader-contract-and-format-boundary.md) and [Phase 5](05-phase-5-openxlsx-reader-and-fixture-parity.md)

## Goal

Prove that the actual Windows WinUI build can compile, link, stage, and run a
small OpenXLSX consumer in every supported configuration.

## Context

This project is not compiled solely by the root CMake graph. The WinUI route
uses `cmake/platform/windows_winui.cmake`, which invokes
`scripts/build_windows_winui.ps1`, followed by MSBuild of
`src/platform/windows/winui/ClassMngrWinUI.vcxproj`. Adding
`add_subdirectory(OpenXLSX)` to another CMake target alone will not make the
WinUI application see its headers or libraries.

## Scope

- Design one reproducible bridge from the pinned dependency to the `.vcxproj`
  build.
- Build static OpenXLSX and all required dependencies with compatible
  architecture, configuration, compiler runtime, and toolset settings.
- Generate or maintain a narrowly scoped MSBuild property/import mechanism for
  include directories, library directories, and link inputs.
- Validate x64 Debug, x64 Release, and Win32 Release if those remain supported
  application configurations.
- Verify final application staging contains every required runtime dependency.

## Out of Scope

- Implementing the schedule parser.
- Moving all project dependencies to a new package manager.
- Linking OpenXLSX directly into `ClassMngrEngine`.

## Candidate Integration Shape

The spike should compare, then choose, one maintainable route:

1. a controlled CMake dependency build that emits configuration-specific
   artifacts plus a generated `OpenXLSX.props`; or
2. a prebuilt, pinned package layout with an equivalent checked-in/generated
   `.props` import.

The property file should be imported only by the WinUI target that owns the
native codec. It must set, per configuration/platform:

- include paths;
- library paths and exact `.lib` inputs;
- runtime-library compatibility;
- required preprocessor definitions;
- post-build/staging dependencies where static linking is not complete.

## Work Breakdown

1. Map the existing CMake-to-PowerShell-to-MSBuild invocation and supported
   platform/configuration matrix.
2. Build the pinned OpenXLSX source with product-safe options: C++17-compatible
   dependency settings are acceptable because the application is C++23, but
   ABI/runtime settings must match the WinUI target.
3. Implement a minimal codec smoke target or a temporary WinUI-local source
   that includes `OpenXLSX.hpp`, creates an `XLDocument`, and links without
   changing import behavior.
4. Add the selected props/artifact integration to the real WinUI route.
5. Build each supported configuration from the standard repository command,
   not an IDE-only manual configuration.
6. Launch or otherwise load the staged executable to catch missing DLLs and
   loader failures.
7. Remove temporary smoke-only product wiring once the durable bridge is
   established, retaining a small deterministic build check if practical.

## Files Likely to Be Investigated

- `cmake/platform/windows_winui.cmake`
- `scripts/build_windows_winui.ps1`
- `src/platform/windows/winui/ClassMngrWinUI.vcxproj`
- associated `.props`, staging, or CMake helper files selected by the spike

## Build Contract

- No headers or libraries are resolved from an arbitrary developer path.
- Debug code never links Release libraries, and x64 never links Win32 libraries.
- Product builds do not enable OpenXLSX samples, documentation, benchmarks, or
  its upstream tests as an accidental dependency.
- A library link failure is surfaced as a build failure; no fallback header-only
  or dynamically discovered system library is permitted.

## Validation

- Configure and build x64 Debug and x64 Release through the repository’s normal
  Windows WinUI command.
- Build Win32 Release as well if it is a supported packaging target.
- Execute the smoke path against a known harmless workbook or an empty document
  creation path, according to what OpenXLSX supports without modifying input.
- Inspect the staged application dependencies and confirm the executable starts
  without loader errors.
- Repeat one build from a clean dependency/artifact directory to prove include
  and library discovery are deterministic.

## Exit Criteria

The native codec can be introduced in a WinUI-local implementation file with a
stable, configuration-correct build path. Phase 5 may rely on that path without
inventing a second build strategy.

## Risks and Responses

| Risk | Response |
| --- | --- |
| CMake target propagation stops before MSBuild | Generate/import explicit target-local MSBuild metadata. |
| CRT mismatch causes runtime faults | Build dependency artifacts with the same runtime choice as each WinUI configuration. |
| A dependency has non-static runtime needs | Stage and test it explicitly; do not discover it only at customer install time. |
