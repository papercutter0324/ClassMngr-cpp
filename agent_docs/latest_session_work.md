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
- The slice 1.1 clean build directory was
  C:\Users\wfelt\AppData\Local\Temp\ClassMngr-Phase1-1.1-clean-965a9613fb8046b7bbbbd5a520b03740.
  It is local verification output, not a checked-in artifact.
- A direct clean configure with the Visual Studio 18 generator from the normal
  PowerShell environment could not identify its C++ compiler. The clean Ninja
  configure/build under VsDevCmd succeeded; use that route on this host.
- The slice 1.2 working change is limited to `cmake/next.cmake`; slice 1.1
  changed `CMakeLists.txt`, `cmake/next.cmake`, and `src/next/main.cpp`.
- Phase 1 is not complete. No cross-platform v2 build has been verified yet.

## Next Entry Point

Slice 1.1 is committed and slice 1.2 is verified for its own commit. Before
starting slice 1.3, finish the slice 1.2 commit. Then inspect the current Qt
usage and shared build-settings targets to define target-specific dependency
ownership without disturbing ClassMngr or copying the monolithic target graph.
