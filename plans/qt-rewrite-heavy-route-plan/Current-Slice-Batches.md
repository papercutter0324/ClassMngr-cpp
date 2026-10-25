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
`origin/Qt-Rewrite` matched at the new tip at that time. Current local `HEAD`
and `origin/Qt-Rewrite` both resolve to
`1605bac36d4192bf027440a2b83dce8a5e2d99d1`, parent
`a2e506a74c316bf902d6a4b5cd956970286471b3`, tree
`12f431a05225f30aecb076fd88886403a2e0e8fc`. This is the same tree and parent
as the former local receipt commit `9fe1f26fe3ace50996d58b87cf045f9afab31afb`,
now superseded by the published commit. The intended Windows x64 Debug and
Linux GCC Debug jobs cover loader native resource-handle assertions, document
content/export ports, QtPdf session release, and no-pack startup:

- `ClassMngrNextResourceLoaderTests`
- `ClassMngrNextPlatformDocumentContentResourcePortTests`
- `ClassMngrNextPlatformDocumentExportResourcePortTests`
- `ClassMngrNextPlatformQtPdfSessionReleaseTests`
- `ClassMngrNextLaunchFilesystemSideEffectsTests`
- `ClassMngrNextLaunch`

Linux QtPdf is mandatory and fails clearly if unavailable. Independent workflow
review passed. Authenticated Actions API queries found no workflow runs/checks
for these heads: Batch 2 `1475a46d`, Batch 3 `a2e506a7`, and current remote
`1605bac3`. Combined status for `1605bac3` is pending. No run or result is
claimed, and F1483/F1486 native outcomes remain pending. The push filter targets
`Qt-Rewrite` and push workflows can run on non-default branches. The repository
default is `main`, where this workflow file is absent; `workflow_dispatch`
requires it on the default branch. No dispatch tool is exposed. The missing run
cause is unknown; do not assume credential suppression or Actions settings.
User direction is needed on whether to extend workflow scope to `main`. F1483's
root-resolver and prefix-guard tests remain in its separate package/layout
matrix. Phase 4 remains In progress pending both native gates and that
direction.
Phase 7/10 rendering parity remains deferred; legacy RCC implementation and
pack inputs remain the compatibility oracle through Phase 12.
