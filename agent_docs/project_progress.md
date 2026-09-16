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
reference, and the Calendar Import Preferences loading reference. Each focused
route and the full startup-performance suite passed for the current final
Schedule Import slice.

## Current Position

The last accepted slice remains the large Schedule Import conflict-warning
reference at commit `ecb88af0`. The Calendar Import parser-failure
implementation is committed at `ded8b5dc`, and the continuation guard plus
diagnostic handoff/artifacts are preserved in `a8cccb35`; the focused
acceptance test still fails before complete workflow/metrics evidence is
available, so the slice remains unaccepted. Phase 1 has not started. Fresh
Ninja Release compilation succeeded earlier, and the unchanged Calendar Import
success route plus the full startup suite passed earlier. No Calendar Import
error artifact is accepted yet.

The CMake configuration cleanup is complete. Multi-config generators now expose
only `Debug;Release`; every Debug preset explicitly enables tests and every
Release preset disables them; non-QML test targets opt out of unnecessary QML
import scanning; and active build/docs/workflow references use Qt 6.12.0. A
fresh isolated Windows configure produced 195 tests and zero `qmlimportscan`
targets. The empty root-generated `CMakeFiles/` residue was removed and root
CMake output is now ignored; `build/` and `dist/` were not cleaned.

## Next Milestone

On the next device, rebuild the fresh Ninja Release targets and rerun the
focused expected-failure route with complete workflow/metrics inputs and
outputs. Then run the normal success route and full startup suite, manually
inspect the error screenshot, and update the Phase 0 verification record. Do
not begin Phase 1 or make v2 memory claims.
