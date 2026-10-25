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

## Batch 3 — pushed

F1486 is committed as `094cda87` and expands the workflow using existing tests
only. The successful push from `Qt-Rewrite` to `origin` advanced the ref in two
commits, from Batch 2 tip
`1475a46dae77d8970852681dae7a2032ff907edf` to
`a2e506a74c316bf902d6a4b5cd956970286471b3` (F1486 workflow commit
`094cda87`, followed by its documentation commit `a2e506a7`). Local `HEAD` and
`origin/Qt-Rewrite` match at the new tip. The intended Windows x64 Debug and
Linux GCC Debug jobs cover loader native resource-handle assertions, document
content/export ports, QtPdf session release, and no-pack startup:

- `ClassMngrNextResourceLoaderTests`
- `ClassMngrNextPlatformDocumentContentResourcePortTests`
- `ClassMngrNextPlatformDocumentExportResourcePortTests`
- `ClassMngrNextPlatformQtPdfSessionReleaseTests`
- `ClassMngrNextLaunchFilesystemSideEffectsTests`
- `ClassMngrNextLaunch`

Linux QtPdf is mandatory and fails clearly if unavailable. Independent workflow
review passed. Authenticated Actions API queries after Batch 2 head `1475a46d`
and Batch 3 head `a2e506a7` found no workflow runs; combined commit status and
check-runs are empty or pending. No run or result is claimed, and F1483/F1486
native outcomes remain pending. F1483's root-resolver and prefix-guard tests
remain in its separate package/layout matrix. Phase 4 remains In progress until
both gates pass.
Phase 7/10 rendering parity remains deferred; legacy RCC implementation and
pack inputs remain the compatibility oracle through Phase 12.
