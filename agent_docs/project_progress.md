# Project Progress

Active deployment plan: automate and complete the Qt rewrite Phase 0
baseline/evidence work using the heavy route, with one commit per completed
slice. Current deployment: `qt0_windows_completion_20260917`. The prior
automation and merge-reconciliation deployments are complete; merge
reconciliation is recorded in local commit `7a9cb3c`.

## Goal

Complete Phase 0's product contract, packaged Release baseline, visual/output
evidence, resource ownership evidence, and memory trend record before starting
Phase 1. Windows x64 and macOS universal are supported baseline targets;
Windows ARM64 and Linux are unofficial ports deferred by user decision. The
final `<250 MiB` RAM target applies to the completed rewrite; it is not a Phase
0 acceptance gate for the legacy widget graph.

## Overall Progress

The packaged Windows x64 Release route has retained startup, lifecycle,
resource-trace, PDF-viewer, Sub Prep, Classes, Speaking Evaluation, import,
transfer, staff-directory, and generated-output evidence. The macOS universal
Release baseline is now also retained at a 14.4 deployment target, including
native packaged startup, all-page workflow, PDF lifecycle, and visual evidence.
Recent Windows slices added a Speaking Evaluation PowerPoint renderer-selection
reference, a Sub Prep
output-dialog validation-error reference, a Sub Prep editing/read-only
reference, populated Classes visual references across English/Korean and
light/dark variants, real Schedule Import loading references for both cancel
and apply boundaries, a real large-route Schedule Import conflict-warning
reference, the Calendar Import Preferences loading reference, and the Calendar
Import parser-failure boundary. Each focused route and the full
startup-performance suite passed for the accepted Calendar Import slice.
The `scripts/phase0/` automation now has a fresh full Windows x64 result: all 24
packaged routes passed the runner and validator, with 151/151 required files
present. The compact audit record and per-file hash inventory are retained at
`docs/qt-rewrite/phase-0-evidence/windows-x64-route-matrix-2026-09-17/`; the
147,732,123-byte full run root remains in the local temporary directory and is
not a portable Git evidence root. The macOS universal packaged baseline is
retained, but its 24-route matrix is still pending, so the combined Phase 0
exit gate remains incomplete. Windows ARM64 and Linux are deferred, not
blockers.

## Current Position

The macOS universal packaged baseline is complete. The deployment minimum is
14.4, aligned with the selected Qt 6.12.0 kit. The packaged arm64/x86_64
Release app and DMG passed signature/checksum checks; all 115 embedded Mach-O
files were verified universal and no newer than the target. Packaged startup
and the 96-class all-route workflow completed on macOS 26.6.2, and the fresh
14.4-target universal Debug startup-performance and dialog-services CTest
targets passed. Evidence is under
`docs/qt-rewrite/visual-baseline/macos-universal/release/`. The host did not
provide macOS 14.4 for a direct oldest-version runtime check.

The fresh Windows x64 Release runner completed all 24 route IDs from a newly
built/package set. Its validator reports zero failures and 151/151 required
files; independent audit confirmed all 30 build/package, route, and validation
commands exited 0 without timeout.
The resource trace contains 189 entries and
records normal completion without timeout. The compact manifest, validation
summary, and per-file hash inventory are retained in the Phase 0 evidence
folder; the complete run root remains at the local temporary path recorded in
that folder's README. Its 495 samples at or above 250 MiB are trend data; the
maximum working set was 496,005,120 bytes, below the diagnostic 512 MiB ceiling.
The macOS universal 24-route run remains pending.

The Calendar Import parser-failure boundary remains accepted. Its implementation
is based on `ded8b5dc`, with the evidence-only error capture scroll fix and fresh
Release artifacts completed in deployment `qt0_calendar_verify_20260917`.
The 69-byte malformed response produced the real missing
`xl/workbook.xml` error, re-enabled Import Events, unchanged calendar events,
strict failure/release ordering, complete workflow/metrics/manifest/trace
evidence, and a manually inspected error screenshot. The unchanged success
route and the opt-in-cleared full startup suite also passed. Phase 1 has not
started.

Phase 0 automation is implemented under `scripts/phase0/`. Independent checks
confirmed all 24 runner route mappings, a no-write all-route plan, root-safety
rejections, validator self-tests, and the full Windows x64 24-route run. The
exit-gate report requires all 24 routes on both Windows x64 and macOS
universal; the Windows per-run pass does not complete the combined gate.

The CMake configuration cleanup is complete. Multi-config generators now expose
only `Debug;Release`; every Debug preset explicitly enables tests and every
Release preset disables them; non-QML test targets opt out of unnecessary QML
import scanning; and active build/docs/workflow references use Qt 6.12.0. A
fresh isolated Windows configure produced 195 tests and zero `qmlimportscan`
targets. The empty root-generated `CMakeFiles/` residue was removed and root
CMake output is now ignored; `build/` and `dist/` were not cleaned.

## Next Milestone

Run the 24-route matrix on macOS universal and enforce the combined exit gate
with the retained Windows run root. Continue the remaining feature-state and
golden-output review. The Windows x64 matrix and macOS packaged baseline are
complete; Phase 0 remains open because macOS route coverage and some visual and
output review remain. Preserve Windows ARM64 and Linux as deferred unofficial
ports. Do not begin Phase 1 or make v2 memory claims from the legacy widget
graph.
