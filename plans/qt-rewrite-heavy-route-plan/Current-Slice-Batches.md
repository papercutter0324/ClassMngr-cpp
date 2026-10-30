# Current Slice Batches

## Batch 5 — active

49. **In progress — Workspace Calendar parity follow-up:** reconcile the
    embedded Workspace Calendar gap recorded by #23 against standalone
    Calendar slices #33–34.
50. **Discovered — Classes parity follow-up:** resolve or disposition the
    legacy visual, pending-slot, and test gaps recorded by #24, #28, and #29.
51. **Discovered — Schedule import/output follow-up:** close the 96-row apply,
    localized presentation, and print/output evidence gaps recorded by #31–32.
52. **Discovered — Document Catalog live-locale follow-up:** verify locale
    changes while Documents is active and close the integration gap recorded
    by #42.
53. **Discovered — Sub Prep output parity follow-up:** resolve the PDF
    separator text-map limitation and determine ownership of the Windows
    150-DPI raster check.
54. **Discovered — 7A evidence follow-up:** collect representative setup
    screenshots and feature-level lifecycle/memory evidence; retry the Phase 7
    Windows diagnostic after the Batch 4 push. The setup schedule-import build
    prerequisite was repaired in `80f7f1c6`; do not claim the Phase 9 numeric
    memory gate.
55. **Discovered — Phase 7 exit review:** reconcile every preservation-matrix
    row and old call path, then run the required full suite and Windows checks
    after implementation follow-ups.

Begin Batch 6 discovery when #54 starts. The existing uncommitted Schedule
Import additions in `src/next/platform/workspace_store_gateway.cpp` and `.h`,
and `preferences_visual_captures/`, are preserved user work outside these
slices; do not stage or modify them.
