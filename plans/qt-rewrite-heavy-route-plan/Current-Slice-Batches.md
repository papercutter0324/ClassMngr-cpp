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
51. **Accepted — Schedule import/output follow-up** (`03ec352f`): five
    one-block applies cover 96 candidates in separate temporary `.tps`
    workspaces; each persisted result reloads into Schedule. Live locale
    switching in the output dialog preserves its open options. Imported
    Schedule output has matching one-page 150-DPI renders; font fallback and
    the offscreen Legacy-widget limitation are documented.
52. **Accepted — Document Catalog live-locale follow-up** (`b6af1279`):
    English/Korean Preferences switching updates catalog and open-viewer labels
    while preserving document identity, folder expansion, PDF/viewer instances,
    readiness, page, and zoom without reopening the file.
53. **In progress — Sub Prep output parity follow-up:** C44's embedded ToUnicode
    map omits used CID `0x001F` (roster U+002D hyphen, 2,400 uses) and CID
    `0x0015` (header U+2022 bullet, 96 uses). Fix text extraction without
    changing painted glyphs; compare expected SQL-derived text and map coverage.
    Assign the 150-DPI all-page check to the SQL-backed Legacy/Next output pair;
    the existing C44 raster is synthetic. The typed Sub Prep Gateway reads are
    already committed; no Gateway extension is required.
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
