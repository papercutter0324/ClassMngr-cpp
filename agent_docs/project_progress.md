# Project Progress

Active deployment plan: continue the Qt rewrite Phase 0 baseline/evidence
work using the heavy route, with one commit per completed slice.

## Goal

Complete Phase 0's product contract, packaged Release baseline, visual/output
evidence, resource ownership evidence, and memory trend record before starting
Phase 1. The final `<250 MiB` RAM target applies to the completed rewrite; it
is not a Phase 0 acceptance gate for the legacy widget graph.

## Overall Progress

The packaged Windows x64 Release heavy route has retained startup, lifecycle,
resource-trace, PDF-viewer, Sub Prep, Classes, Speaking Evaluation, import,
transfer, staff-directory, and generated-output evidence. Recent slices added
a Speaking Evaluation PowerPoint renderer-selection reference, a Sub Prep
output-dialog validation-error reference, a Sub Prep editing/read-only
reference, populated Classes visual references across English/Korean and
light/dark variants, real Schedule Import loading references for both cancel
and apply boundaries, a real large-route Schedule Import conflict-warning
reference, the Calendar Import Preferences loading reference, and the Calendar
Import parser-failure boundary. Each focused route and the full
startup-performance suite passed for the accepted Calendar Import slice.

## Current Position

The Calendar Import parser-failure boundary is now accepted. Its implementation
is based on `ded8b5dc`, with the evidence-only error capture scroll fix and fresh
Release artifacts completed in deployment `qt0_calendar_verify_20260917`.
The 69-byte malformed response produced the real missing
`xl/workbook.xml` error, re-enabled Import Events, unchanged calendar events,
strict failure/release ordering, complete workflow/metrics/manifest/trace
evidence, and a manually inspected error screenshot. The unchanged success
route and the opt-in-cleared full startup suite also passed. Phase 1 has not
started.

The CMake configuration cleanup is complete. Multi-config generators now expose
only `Debug;Release`; every Debug preset explicitly enables tests and every
Release preset disables them; non-QML test targets opt out of unnecessary QML
import scanning; and active build/docs/workflow references use Qt 6.12.0. A
fresh isolated Windows configure produced 195 tests and zero `qmlimportscan`
targets. The empty root-generated `CMakeFiles/` residue was removed and root
CMake output is now ignored; `build/` and `dist/` were not cleaned.

## Next Milestone

Continue Phase 0 with the remaining feature loading/error states, generated
output references, cross-platform Release evidence, and final memory trend
record. Do not begin Phase 1 or make v2 memory claims from the legacy widget
graph.
