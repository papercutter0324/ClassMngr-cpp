# Latest Session Work

Qt Rewrite Phase 1 has started on branch Qt-Rewrite. Phase 0 was completed on
2026-09-18: the combined Windows x64 and macOS universal exit gate passed all
24 required routes on both platforms, and the user approved the retained visual
references. The Phase 0 plan update in commit f8bb5954 is authoritative over
older Phase 0 open notes in this document's history.

## Current Deployment Handoff

- Deployment: qt1_build_structure_20260918, Heavy route.
- User requirement: commit each completed slice before starting the next.
- Slice 1.1 adds ClassMngrNext as a console bootstrap implemented with
  QCoreApplication. Its target links only Qt Core and has an explicit CTest
  launch probe. It has no window, legacy runtime, production resources, or
  deployment hook.
- A fresh temporary Ninja/MSVC Debug build configured with Qt 6.12 and built
  both ClassMngr and ClassMngrNext successfully (351 build steps). The focused
  ClassMngrNextLaunch CTest passed (1/1). Independent review confirmed the
  new executable links Qt Core plus MSVC/UCRT and Windows system libraries,
  without ClassMngrRuntime.
- Slice 1.2 adds six source-free next-generation layer interface targets and
  eleven source-free feature interface targets with `ClassMngrNext::` aliases.
  CMake asserts each target's exact allowed dependency list: Application →
  Domain/Persistence, Persistence → Domain, Platform → Application,
  UiShared → Application/Resources, and each feature →
  Application/Resources/UiShared. Domain and Resources have no dependencies;
  features do not depend on one another. ClassMngrNext remains linked only to
  Qt Core and does not link these placeholders.
- A fresh Ninja/MSVC Debug configure and build for slice 1.2 passed in 351
  steps for both executables; `ClassMngrNextLaunch` passed (1/1). The clean
  build directory was
  `C:\Users\wfelt\AppData\Local\Temp\ClassMngr-Phase1-1.2-clean-13839b2038864e939f913007c0b33405`.
  It is local verification output, not a checked-in artifact.
- Slice 1.3 moves Qt modules out of `ClassMngrBuildSettings` and assigns the
  measured module sets to the six existing legacy production object targets.
  `ClassMngrRuntime` and the macOS `ClassMngrTestRuntime` retain the complete
  legacy module union, including QuickControls2 for the QML/resource graph.
  The Phase 1 plan records the per-target module map.
- A fresh Ninja/MSVC Debug configure and build for slice 1.3 passed for both
  executables in 351 steps; `ClassMngrNextLaunch` passed (1/1). Independent
  inspection confirmed the Domain compile command has only QtCore/QtGui
  include paths, the `ClassMngr.exe` link command retains the full legacy Qt
  set, and `ClassMngrSharedPolicyTests` built and passed (1/1). The clean build
  directory was
  `C:\Users\wfelt\AppData\Local\Temp\ClassMngr-Phase1-1.3-clean-4ffa6756081f4d0bb0044b1a12758723`.
  It is local verification output, not a checked-in artifact.
- Slice 1.4 replaces recursive production source discovery with explicit
  manifests for the six legacy object targets, both executable entry points,
  and the two calendar QML files. Seven included roster `.inc` fragments are
  marked header-only and assigned to `ClassMngrFeatures`; the generated build
  info header remains a distinct `configure_file()` input. The new configure-
  time check treats source globs only as an inventory of handwritten `src`
  and active test files, and requires one target owner per file. Shared
  schedule test doubles now compile once in test-support object libraries; the
  ResourcePackManager fake is split out because ClassesPage uses the real
  manager.
- A clean Ninja/MSVC Debug configure and targeted build succeeded for
  `ClassMngr`, `ClassMngrNext`, and the five test executables using the shared
  schedule stubs. The ownership check validated 653 handwritten files for
  Windows x64, excluding the Apple-only PowerPoint notice test. The
  `ClassMngrNextLaunch` probe and all five affected tests passed CTest (6/6).
  A temporary unassigned `src/` probe was rejected by the ownership check; it
  was removed and a clean reconfigure passed again.
  The clean build directory was
  `C:\Users\wfelt\AppData\Local\Temp\ClassMngr-Phase1-1.4-clean-5d33542afed54ad8bbefc3546133c8e2`.
  It is local verification output, not a checked-in artifact.
- Slice 1.5 adds formatting and static-analysis checks scoped to `src/next`,
  asserts that `ClassMngrNext` links only Qt Core, writes Qt module and
  resource-pack reports, checks runtime resource references, and adds staged
  package checks and size reports to the Windows, macOS, and Linux Release
  workflows. Startup performance CTest is labeled
  `startup;memory;performance`.
- A clean Ninja/MSVC Debug configure and full build passed for `ClassMngr` and
  `ClassMngrNext` (351 steps); `ClassMngrNextLaunch` passed (1/1). The Qt module
  report listed only `Qt6::Core` for `ClassMngrNext`; the resource checker
  passed for six generated RCC packs and seven runtime IDs/references; a
  staged-package report probe passed. Startup/memory labels were discovered,
  but the performance test was not built or run. Cross-platform CI and local
  `clang-format`/`clang-tidy` were not run. The clean build directory was
  `C:\Users\wfelt\AppData\Local\Temp\ClassMngr-Phase1-1.5-clean`.
  It is local verification output, not a checked-in artifact.
- The slice 1.1 clean build directory was
  C:\Users\wfelt\AppData\Local\Temp\ClassMngr-Phase1-1.1-clean-965a9613fb8046b7bbbbd5a520b03740.
  It is local verification output, not a checked-in artifact.
- A direct clean configure with the Visual Studio 18 generator from the normal
  PowerShell environment could not identify its C++ compiler. The clean Ninja
  configure/build under VsDevCmd succeeded; use that route on this host.
- Slice 1.3 changes `cmake/sources.cmake`; slice 1.2 changes
  `cmake/next.cmake`; slice 1.1 changed `CMakeLists.txt`, `cmake/next.cmake`,
  and `src/next/main.cpp`.
- Phase 1 is not complete. No cross-platform v2 build has been verified yet.

## Next Entry Point

Slices 1.1-1.4 are committed. Slice 1.5 is verified on Windows and ready to
commit before beginning slice 1.6, build configurations. Keep the legacy
production target and the new v2 boundaries buildable while validating the
platform/configuration matrix.
