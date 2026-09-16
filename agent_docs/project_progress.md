# Project Progress

Active deployment plan: automate and complete the Qt rewrite Phase 0
baseline/evidence work using the heavy route, with one commit per completed
slice. Current automation deployment: `qt0_evidence_automation_20260917`.
Current merge reconciliation deployment: `qt0_merge_conflicts_20260917`.

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
The new `scripts/phase0/` automation provides a Windows x64 packaged-evidence
runner for 24 existing routes and a standard-library validator/exit-gate
consolidator. Historical Windows x64 Release artifacts passed integrity
validation (28 manifests, 81 PNGs, 29 PDFs, and one ZIP), but that legacy
layout cannot establish coverage for the required 24 orchestrated route IDs.
The exit gate correctly remains incomplete pending full route runs for Windows
x64 and macOS universal. Windows ARM64 and Linux are recorded as deferred, not
blockers.

## Current Position

The macOS universal Phase 0 slice is now complete. The deployment minimum is
14.4, aligned with the selected Qt 6.12.0 kit. The packaged arm64/x86_64
Release app and DMG passed signature/checksum checks; all 115 embedded Mach-O
files were verified universal and no newer than the target. Packaged startup
and the 96-class all-route workflow completed on macOS 26.6.2, and the fresh
14.4-target universal Debug startup-performance and dialog-services CTest
targets passed. Evidence is under
`docs/qt-rewrite/visual-baseline/macos-universal/release/`. The host did not
provide macOS 14.4 for a direct oldest-version runtime check.

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
rejections, validator self-tests, and retained Windows x64 evidence. The full
24-route collection was not executed in this deployment; PowerShell 7 and a
macOS run remain unverified. The exit-gate report requires all 24 routes on
both Windows x64 and macOS universal; it is distinct from a per-run pass.

The CMake configuration cleanup is complete. Multi-config generators now expose
only `Debug;Release`; every Debug preset explicitly enables tests and every
Release preset disables them; non-QML test targets opt out of unnecessary QML
import scanning; and active build/docs/workflow references use Qt 6.12.0. A
fresh isolated Windows configure produced 195 tests and zero `qmlimportscan`
targets. The empty root-generated `CMakeFiles/` residue was removed and root
CMake output is now ignored; `build/` and `dist/` were not cleaned.

## Next Milestone

Run the new 24-route matrix on both required platforms with explicit evidence
parents, address remaining generated-output and visual-review gaps, and
consolidate the evidence with the exit-gate validator. The packaged macOS
universal baseline is retained, but the route matrix remains incomplete.
Preserve Windows ARM64 and Linux as deferred unofficial ports. Do not begin
Phase 1 or make v2 memory claims from the legacy widget graph.
