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

Slice 1.4 replaces recursive production source discovery with explicit lists
for all six legacy object targets, the two executable entry points, and the
calendar QML files. Included roster `.inc` fragments are explicit header-only
inputs. A configure-time ownership check compares the handwritten `src` and
active test inventories with their targets; shared schedule test doubles now
have one object-library owner each.

## Current Position

Slice 1.4's clean Ninja/MSVC Debug configure and targeted build succeeded for
ClassMngr, ClassMngrNext, and all five tests that consume the shared schedule
stubs. The configure-time ownership check validated 653 handwritten files for
the Windows configuration. `ClassMngrNextLaunch` and those five test programs
passed CTest (6/6).

Slices 1.1-1.4 are implemented and verified. Phase 1 remains in progress; the
next slice is 1.5, tooling and CI.

## Next Milestone

Commit slice 1.4 before starting slice 1.5. Keep next-generation target names
distinct from legacy object targets such as ClassMngrDomain and
ClassMngrUiShared.
