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
resource-trace, PDF-viewer, Sub Prep, Speaking Evaluation, import, transfer,
staff-directory, and generated-output evidence. Recent slices added a
Speaking Evaluation PowerPoint renderer-selection reference and a Sub Prep
output-dialog validation-error reference. Each focused route and the full
startup-performance suite passed.

## Current Position

Current position is the open Phase 0 evidence gap after the Sub Prep
validation-dialog slice. Phase 1 has not started. The current route remains a
legacy before-state measurement; later phases must avoid regressions and
should lower the affected heavy-route working set.

## Next Milestone

Resume with the next packaged Windows x64 Release heavy-route slice, preferably
Sub Prep editing/read-only/loading or another explicitly open visual/output
gap. Run the focused heavy route, the full startup-performance suite, and
commit the slice before continuing.
