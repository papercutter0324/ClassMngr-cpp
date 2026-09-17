# Project Progress

Active deployment plan: Qt Rewrite Phase 1 — Build System and Repository Structure.
Current deployment: qt1_build_structure_20260918. Route: Heavy. Commit each
completed slice before starting the next.

## Goal

Establish the parallel ClassMngrNext build, architectural target boundaries,
explicit source ownership, dependency checks, and reproducible Debug, Release,
and platform build flows while keeping the existing ClassMngr production
target buildable through cutover.

## Overall Progress

Phase 0 completed on 2026-09-18. The combined route gate passed all 24 required
routes on Windows x64 and macOS universal, and the user confirmed the retained
visual references. The Phase 0 plan update in commit f8bb5954 records this
closure; earlier Phase 0 open notes are superseded.

Phase 1 slice 1.1 adds a Qt Core-only ClassMngrNext console bootstrap and a
focused CTest launch probe. It has no window and does not link the legacy
runtime.

## Current Position

A clean Ninja/MSVC Debug configure in a fresh temporary directory built both
ClassMngr and ClassMngrNext in 351 steps. ClassMngrNextLaunch passed (1/1).
Independent link inspection confirmed that ClassMngrNext links Qt Core and
platform runtime libraries, without ClassMngrRuntime, legacy resources, or
deployment rules.

The code is scoped to CMakeLists.txt, cmake/next.cmake, and src/next/main.cpp.
Slice 1.1 is the current committed milestone; slice 1.2 is next and starts
after this commit.

## Next Milestone

Commit slice 1.1, then establish v2 target boundaries for slice 1.2. Account
for the existing legacy object targets that already use ClassMngrDomain and
ClassMngrUiShared before selecting v2 library names.
