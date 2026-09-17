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

Slice 1.3 measured Qt/Zlib header dependencies per legacy production object
target and moved those modules out of the shared build-settings target. The
legacy runtime keeps the complete module union required to link the current
application and tests.

## Current Position

Slice 1.3's fresh Ninja/MSVC Debug configure and build succeeded for both
ClassMngr and ClassMngrNext in 351 steps. `ClassMngrNextLaunch` and the existing
runtime-backed `ClassMngrSharedPolicyTests` passed (1/1 each). The generated
Domain compile command uses Qt Core and Gui only; the application link keeps
the legacy Qt union including QuickControls2.

Slices 1.1-1.3 are implemented and verified. Phase 1 remains in progress; the
next slice is 1.4, explicit source ownership.

## Next Milestone

Complete the slice 1.3 commit, then begin slice 1.4 by replacing recursive
source discovery with explicit ownership. Keep next-generation target names
distinct from legacy object targets such as ClassMngrDomain and
ClassMngrUiShared.
