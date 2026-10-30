# Current Slice Batches

## Batch 5 — active

49. **Accepted — Workspace Calendar parity follow-up** (`f00b9a17`): the
    embedded tab restores the full Calendar content and interactions, serializes
    heavy ownership across routes, refreshes through workspace replacement and
    close, and matches Legacy's visible grid in paired locale/theme captures.
50. **Accepted — Classes parity and transfer boundary** (`66ea9162`): Next
    enforces the 4,096 visible-Class cap inside the import transaction while
    leaving the Legacy API unbounded, preserves pending-slot data without the
    extra visible label, and adds paired page-state captures. Empty/dialog/error
    full-window captures remain for the exit review; the paired capture uses a
    one-route ApplicationShell, not the production navigation tree.
51. **In progress — Schedule import/output follow-up:** close the 96-row apply,
    localized presentation, and print/output evidence gaps recorded by #31–32.
52. **Discovered — Document Catalog live-locale follow-up:** cover live
    Preferences locale changes with a PDF open, including catalog labels,
    selected-document identity, viewer title/controls and state, and preservation
    of folder expansion; close the integration gap recorded by #42.
53. **Discovered — Sub Prep output parity follow-up:** resolve the PDF
    separator text-map limitation and determine ownership of the Windows
    150-DPI raster check.
54. **Discovered — 7A evidence follow-up:** collect representative setup
    screenshots and feature-level lifecycle/memory evidence; repair packaging
    and rerun the Phase 7 Windows diagnostic after the Batch 4 push. Run
    `41023184649` built Windows Release successfully, but its diagnostic
    artifact is invalid: the staged executable lacked the Next resource
    catalog, no scenarios were collected, and no memory threshold was
    evaluated. The setup schedule-import build prerequisite was repaired in
    `80f7f1c6`; do not claim the Phase 9 numeric memory gate.
55. **Discovered — Phase 7 exit review:** reconcile every preservation-matrix
    row and old call path, close full-window Classes empty/dialog/error capture
    gaps, resolve the current Legacy class-transfer fallback-test failure, then
    run the required full suite and Windows checks after implementation.

Begin Batch 6 discovery when #54 starts. The typed Sub Prep print and roster
reads are already committed in `WorkspaceStoreGateway`; Candidate #53 needs no
Gateway extension unless source review identifies a missing operation.
Preserve the separate uncommitted Schedule Import edits in the Gateway pair
and `preferences_visual_captures/`.
