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

Supplementary standard macOS Debug verification ran six targeted F1486 CTests:
5/6 passed. Only `ClassMngrNextResourceLoaderTests` failed: the generated
catalog has 194 records, classified by the generator as 187 runtime-packaged
plus 7 generated. C++ `directCount` counts `PackageFile`: current totals are
194 records (192 `PackageFile`, 2 `QtResource`). The five generated QM files
are among the 192 package files; the other two generated records are Qt QML.
Correct expectations are 192 `PackageFile` plus 2 `QtResource`, 194 total.
The assertion at `cmake/tests/next_resource_loader.cpp:333` expects 198 total
and 196 `PackageFile`. Explorer confirmed these expectations are stale; F1482's
four `excluded_v2` records account for the 198-to-194 change. Executor
corrected counts while preserving coverage; its standard macOS Debug build
and focused CTest passed 1/1. Independent Tester reran all six targeted F1486
CTests on the standard macOS Debug preset; 6/6 passed. F1488 is accepted, and
F1487 is committed as `f86c79fc`. Required Windows/Linux Actions results
remain open; the macOS pass does not replace them.

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

## Batch 4 — pushed

Batch 4 advanced `origin/Qt-Rewrite` from
`1605bac36d4192bf027440a2b83dce8a5e2d99d1` to
`ea46415a93e8285e4cb1f81e9aec5b79f7533286` in two commits: F1487
`f86c79fc81a2b2fd34c6993509b4cd314aacaec1`, then F1488
`ea46415a93e8285e4cb1f81e9aec5b79f7533286`. Local `HEAD` and
`origin/Qt-Rewrite` match at `ea46415a93e8285e4cb1f81e9aec5b79f7533286`.
Actions API returned zero runs and zero check-runs for this exact head; combined
status is pending. No run is claimed.

F1488 is accepted. The corrected resource-loader counts are 194 total: 192
`PackageFile` and 2 `QtResource`; all six focused F1486 CTests passed on the
standard macOS Debug preset. F1483/F1486 native Windows/Linux results remain
open. The push filter targets `Qt-Rewrite` and supports non-default branches;
the workflow file is absent from default `main`, and no dispatch tool is
available. The cause of missing runs is unknown. User direction on extending
workflow scope to `main` remains pending; Phase 4 remains In progress.
