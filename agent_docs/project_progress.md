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

Slice 1.2 establishes explicit next-generation layer and feature interface
targets in `cmake/next.cmake`. Configure-time assertions enforce the planned
dependency edges. The targets are intentionally source-free at this stage;
`ClassMngrNext` remains independently linked only to Qt Core.

## Current Position

Slice 1.2's fresh Ninja/MSVC Debug configure passed its dependency assertions,
and both ClassMngr and ClassMngrNext built successfully in 351 steps.
ClassMngrNextLaunch passed (1/1). This validates the boundary declarations
without changing production sources or the legacy target graph.

Slices 1.1 and 1.2 are implemented and verified. Phase 1 remains in progress;
the next slice is 1.3, target-specific dependency cleanup.

## Next Milestone

Complete the slice 1.2 commit, then begin slice 1.3. Keep the next-generation
target names distinct from legacy object targets such as ClassMngrDomain and
ClassMngrUiShared.
