# Current Slice Batches

Phase 4 is In progress on the mandatory Heavy route.

## Batch 2 — pushed

F1485 closed the Batch 2 commit set. The 25 commits span F1469 `d282e7f1`
through F1485 `1475a46d`. The successful push from `Qt-Rewrite` to `origin`
advanced `Qt-Rewrite` from
`eb473fab89410e6dc5f5527d872b56f150f7f6fd` to
`1475a46dae77d8970852681dae7a2032ff907edf`; at the time of the receipt, local
`HEAD` and `origin/Qt-Rewrite` matched at that SHA.

The push was intended to trigger F1483's native Windows/Linux Actions legs.
First and subsequent authenticated GitHub Actions API queries found no workflow
run or checks for head `1475a46d`. No run or result is claimed; the native
package/layout verification remains pending.

## Batch 3 — queued

F1486 is committed as `094cda87` and expands the workflow using existing tests
only. The commit is on local `HEAD`; `origin/Qt-Rewrite` remains at Batch 2 tip
`1475a46d`, so the Batch 3 push is pending. After that push, native Windows x64
Debug and Linux GCC Debug jobs will run the following checks for loader native
resource-handle assertions, document content/export ports, QtPdf session
release, and no-pack startup:

- `ClassMngrNextResourceLoaderTests`
- `ClassMngrNextPlatformDocumentContentResourcePortTests`
- `ClassMngrNextPlatformDocumentExportResourcePortTests`
- `ClassMngrNextPlatformQtPdfSessionReleaseTests`
- `ClassMngrNextLaunchFilesystemSideEffectsTests`
- `ClassMngrNextLaunch`

Linux QtPdf is mandatory and fails clearly if unavailable. Independent workflow
review passed, but no F1486 native results are available yet. F1483's
root-resolver and prefix-guard tests remain in its separate package/layout
matrix. Phase 4 remains In progress until both gates pass.
Phase 7/10 rendering parity remains deferred; legacy RCC implementation and
pack inputs remain the compatibility oracle through Phase 12.
