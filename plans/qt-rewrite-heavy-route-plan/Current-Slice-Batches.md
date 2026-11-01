# Current Slice Batches

## Batch 6 — active

56. **Active — Dynamic Co-Teachers sidebar navigation:** populate the Next
    Co-Teachers group from the active workspace's assigned co-teacher IDs,
    preserve Legacy ordering, open the selected teacher profile, and refresh
    after assignment and workspace changes. Evidence: Next defines the empty
    group in `src/next/bootstrap/application_bootstrap.cpp:654`;
    `src/next/ui/application_shell.cpp:1190` builds dynamic Korean Teacher
    rows but not Co-Teachers. Legacy's sidebar refresh route is
    `src/app/controllers/sidebar_controller_refresh.cpp:127,140`.

## Batch 7 — discovered

No other slices were found.

Batch 7 discovery was triggered when work on this one-candidate batch started.
The two independent source-first and preservation-matrix scans found no
additional candidate after #55. The Phase 7 full suite and final Windows checks
follow implementation.

Preserve the separate uncommitted Schedule Import edits in the Gateway pair
and `preferences_visual_captures/`.
