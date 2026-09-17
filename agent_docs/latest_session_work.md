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
- The clean build directory was
  C:\Users\wfelt\AppData\Local\Temp\ClassMngr-Phase1-1.1-clean-965a9613fb8046b7bbbbd5a520b03740.
  It is local verification output, not a checked-in artifact.
- A direct clean configure with the Visual Studio 18 generator from the normal
  PowerShell environment could not identify its C++ compiler. The clean Ninja
  configure/build under VsDevCmd succeeded; use that route on this host.
- The initial working tree was clean apart from the branch being one commit
  ahead after Phase 0 closure. Phase 1 implementation changes are limited to
  CMakeLists.txt, cmake/next.cmake, and src/next/main.cpp.
- Phase 1 is not complete. No cross-platform v2 build has been verified yet.

## Next Entry Point

Slice 1.1 is recorded in its own Phase 1 commit. On continuation, inspect
cmake/sources.cmake and the legacy target names: ClassMngrDomain and
ClassMngrUiShared are already occupied by the old executable's object targets.
Use that inventory to define v2 libraries without disturbing ClassMngr or
copying the monolithic target graph.
