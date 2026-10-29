# Current Slice Batches

## Batch 4 — active

41. **Accepted — Campus Directory** (`728c8c14`). See the [Phase 7 progress
    log](08-Phase-7-Progress-Log.md) and [C41 capture package](../../docs/qt-rewrite/visual-baseline/next/phase-7/campus-directory-c41/README.md).
42. **In progress — Document catalog:** localized Next navigation from the
    existing metadata-only projection; emit stable document IDs and leave PDF
    body loading to #43.
43. **Discovered — Document viewer lifecycle/actions:** load, save/copy,
    zoom/navigation, release, and reopen.
44. **Discovered — 7J.1 Sub Prep:** large-workspace class information and
    output.
45. **Discovered — General app-wide Preferences UI/menu.**
46. **Discovered — Admin and actual permission-gate parity.**
47. **Discovered — User-initiated update workflow:** startup updater work is
    already owned by Phase 5.
48. **Discovered — Useful Links external navigation.**

The setup schedule-import prerequisite repair is committed in `80f7f1c6` using
the existing path-scoped ports; the protected Gateway pair remains outside
the repair. A fresh hosted diagnostic run is pending the Batch 4 push. No
other slices were found.
