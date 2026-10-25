# Current Slice Batches

Phase 4 is In progress on the mandatory Heavy route.

## Batch 2 — locally closed; push pending

F1485 assembles the evidence and closes the Batch 2 commit set when this
acceptance record is committed. Read-only Git inspection before F1485 found
the F1469–F1484 span as 24 commits, from `d282e7f1` through `b331e49e`, after
the Batch 1 tip `eb473fab`. `origin/Qt-Rewrite` was still at `eb473fab`; there
is no Batch 2 push receipt. Push after the F1485 record commit to trigger
F1483's pending Windows/Linux Actions legs.

F1483 is committed as `6428b2ca`. Local macOS verification passed the package
root probe, 193-file stage audit, focused CTest 3/3, and Linux guard fixtures
5/5. Its Windows/Linux Actions legs remain pending the Batch 2 push. Closing
this commit set does not close Phase 4.

## Batch 3 — queued for push

F1486 is planned as a verification-only check of existing tests on native
Windows x64 Debug and Linux GCC Debug. Its scope covers the F1468 loader and
native resource-handle assertions, document content and export ports, QtPdf
session release, and the F1479 no-pack launch criterion. It uses:

- `ClassMngrNextResourceLoaderTests`
- `ClassMngrNextPlatformDocumentContentResourcePortTests`
- `ClassMngrNextPlatformDocumentExportResourcePortTests`
- `ClassMngrNextPlatformQtPdfSessionReleaseTests`
- `ClassMngrNextLaunchFilesystemSideEffectsTests`
- `ClassMngrNextLaunch`

Run F1486 after the Batch 3 push. No F1486 results are available. Its checks are
separate from F1483's package and install-layout matrix.

Phase 4 remains In progress until F1483's native Windows/Linux jobs and F1486
pass. Phase 7/10 rendering parity remains deferred; legacy RCC implementation
and pack inputs remain the compatibility oracle through Phase 12.
