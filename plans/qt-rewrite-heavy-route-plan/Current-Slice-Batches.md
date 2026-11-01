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
53. **Accepted — Sub Prep PDF text mapping and output parity** (`b6d8a0a1`):
    the Next-only Qt 6.12 finalizer maps emitted codes from captured render
    source while preserving painting and the Legacy path. C44 verifies 2,400
    exact names, 96 bullets and unique complete headers, with zero changed
    pixels across 16 pages. Native Windows SQL evidence independently verifies
    750 names, 30 headers, valid used-code mappings, and zero changes across
    all 17 pages of both matched PDFs. See the C53 evidence README and
    [Windows run](https://github.com/papercutter0324/ClassMngr-cpp/actions/runs/41268987161).
54. **In progress — 7A evidence follow-up:** the eight English/Korean ×
    light/dark Personal Details captures cover the representative screenshot
    subgate. Add setup entry/completion/cancel-recovery lifecycle and
    process-memory observations, repair packaging, and rerun the Windows
    diagnostic. Runs `41023184649` and `41268987055` built Windows Release
    successfully but produced invalid diagnostics because the staged
    executable lacked the Next resource catalog; no scenarios were collected
    and no memory threshold was evaluated. The runner now locates the unique
    catalog at CMake's actual install path while retaining strict resource
    checks. The local Initial Setup capture passed 3/3 on macOS, and Phase 7
    Python tests passed 13/13 (seven lifecycle-validator and six memory-
    validator cases); the native Windows rerun remains pending. The setup
    schedule-import build prerequisite was repaired in `80f7f1c6`; do not
    claim the Phase 9 numeric memory gate.
55. **Discovered — Phase 7 exit review:** reconcile every preservation-matrix
    row and old call path, close full-window Classes empty/dialog/error capture
    gaps, resolve the current Legacy class-transfer fallback-test failure, then
    run the required full suite and Windows checks after implementation.

## Batch 6 — discovered

56. **Discovered — Dynamic Co-Teachers sidebar navigation:** Next defines the
    Co-Teachers group but does not populate it from class assignments; Legacy
    lists assigned teachers there and opens the selected teacher profile.
    Rebuild from the active workspace's assigned co-teacher IDs, preserve
    Legacy ordering, open the matching profile, and refresh on assignment and
    workspace changes. Evidence: `application_bootstrap.cpp:654,6574`,
    `application_shell.cpp:1190`, and Legacy
    `sidebar_controller_refresh.cpp:127,140`.

Independent source-first and preservation-matrix scans found no other
candidate after #55. Candidate #55 must reconcile #56 against the full exit
review and prior decisions before the next batch starts.

Preserve the separate uncommitted Schedule Import edits in the Gateway pair
and `preferences_visual_captures/`.
