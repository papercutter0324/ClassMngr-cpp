# Project Progress

Active deployment plan: Qt Rewrite Phase 1 — Build System and Repository Structure.
Current deployment: qt1_phase1_resume_20260918. Route: Heavy. Commit each
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

Slice 1.5 adds scoped formatting/static-analysis CI for `src/next`, compile
database validation, a configure-time Qt Core-only link assertion for
`ClassMngrNext`, generated Qt-module and resource-pack reports, resource
reference checks, staged-package build reports in the Release workflows, and
startup/memory/performance CTest labels. A clean Windows Ninja/MSVC Debug
configure and full build passed (351 steps); `ClassMngrNextLaunch` passed
(1/1). The module report lists only `Qt6::Core` for `ClassMngrNext`; the
resource check passed for six generated RCC packs and seven runtime IDs, and
the staged-package report probe passed. Cross-platform CI and local
`clang-format`/`clang-tidy` were not run.

## Current Position

Slice 1.6 makes the existing Debug baseline workflow run for relevant pull
requests and covers Windows x64, Windows ARM64, macOS universal, and Linux.
Native jobs run the baseline build/tests including the `ClassMngrNext` launch
probe; the ARM64 job cross-builds `ClassMngr` and `ClassMngrNext` without
executing them on its x64 runner. Release package workflows remain unchanged
and use the production install/deployment paths. Slice 1.6 is committed.
Independent static review and preset checks passed.

On clean source snapshot `6f2f5fb0`, local Windows x64 Debug baseline configure,
build, and CTest passed 66/66, including `ClassMngrNextLaunch` and the startup
performance test. A Release configure/build, CMake install, and Inno Setup
installer target also passed under Visual Studio 2026/MSVC 19.51 with Qt 6.12.
The staged installer launched with exit 0 and `finalProgress=100`; six RCC
packs, seven runtime IDs, and seven references passed the resource/report
checks. This is supplemental local evidence: the Windows 2022/VS17 generator
could not find a VS2022 instance, so the local run does not prove the hosted
shipping toolchain.

After the local Qt 6.12 update completed, a fresh Windows x64 Ninja/MSVC Debug
configure and full build passed on the working tree based on source commit
`4dbe3ca7`. Configure-time ownership validated 653 handwritten source files;
CTest passed 66/66 in 179.41 seconds. The resource-reference check passed for
six RCC packs, seven runtime IDs, and seven references, and the build report
was generated. These local results use VS 2026/MSVC 19.51 and remain
supplemental to the VS2022 hosted toolchain.

A local macOS 27.0 arm64 / Qt 6.12 Debug universal validation passed for
ClassMngr and ClassMngrNext; the source ownership check passed for 654
handwritten files, both executables passed arm64/x86_64 and macOS 14.4 minimum
checks, and ClassMngrNext links only Qt Core. Release installer/DMG creation,
signature and hdiutil verification, 92 bundled Mach-O compatibility checks,
the resource check (6 RCC packs, 7 runtime IDs, 7 references), and build report
passed. The initial full macOS CTest run under restricted Codex execution
reported 62/67. Individual reruns passed the three UI tests and updater, and
InitialSetupWizard CTest later passed 4/4 with normal macOS service access.
The earlier abort came from the restricted environment's LaunchServices
lookup; the full 67-test suite has not been rerun outside the restriction, so
no aggregate macOS CTest pass is claimed.

Phase 1's official acceptance targets are Windows x64 and macOS universal.
Unofficial Linux and Windows ARM64 builds are deferred; their current workflow
results are informational and do not block Phase 1. On hosted commit
`57f5dff6`, Windows x64 Debug passed 66/66. The macOS universal Debug baseline
job failed after GitHub reported that the hosted runner lost communication
with the server. The user observed `ClassMngrUpdaterTests` during the job, but
the job log and JUnit artifact were unavailable, so the test is not confirmed
as the cause. The Windows and macOS Packaged Release workflow runs passed.
The Phase 1 Build Quality workflow has no recorded run; it is absent from the
default branch and there is no open PR to trigger it. Full run details and the
open GitHub Actions test-reliability work are in the Phase 1 plan. These
hosted results do not replace or invalidate the successful local Windows x64
Debug configure/build and 66/66 CTest pass on source commit `4dbe3ca7` using VS
2026/MSVC 19.51 and Qt 6.12.

## Next Milestone

Rerun macOS Debug with usable logs and test reports, and verify the pending
formatting/static-analysis checks through an eligible pull request or directly.
Confirm the official Windows x64 and macOS universal test/launch checks pass
before evaluating the Phase 1 exit gate. The Linux application build/launch
and Windows ARM64 execution are deferred with those unofficial builds. Keep
local passes recorded separately from hosted outcomes. Keep next-generation
target names distinct from legacy object targets such as ClassMngrDomain and
ClassMngrUiShared.
