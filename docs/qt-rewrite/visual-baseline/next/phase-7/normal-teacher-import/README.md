# Normal Teacher Import captures

Source/test commit: `6dedd431` (`Phase7 - Add normal teacher workbook import`).

These captures document the normal Next workspace import path, separate from
Initial Setup. `Teachers > Import Teachers…` requires an open profile, blocks
pending setup/recovery, and resolves active-page dirty state before opening the
`.xlsx` picker. Next validates the workbook asynchronously and builds a typed
review request with Korean M1/M2/H1 decisions; Native English and GS Team rows
are included automatically. Before applying, it reads the latest imported
source date and asks for confirmation if the new date is the same or older;
Cancel is the default. Apply runs through the `WorkspaceStoreGateway`. Success
refreshes Korean navigation and loaded staff pages and reports
created/updated/unchanged counts. A refresh failure does not undo the committed
import and requires the affected directory to be reloaded.

The fixture is
[`sectioned_review_large.xlsx`](../../../../../../tests/fixtures/teacher_import/sectioned_review_large.xlsx).
It has source date `2026-09-01`, 122 Korean candidates (120 checked), and 40
automatic Native English plus 40 GS Team rows in the review. Both locales show
the nonblank file date `2026-09-01` and current date `2026-10-01` in the version
prompts.

## Captures

The six manifest-listed captures for each locale show selection, review, and
the source-date confirmation prompt. Selection and review dimensions match
legacy: 460×218 and 460×501 for English/light; 460×212 and 460×477 for
Korean/dark. Version prompts are 400×151 and 400×143, respectively.

| State | English / light | Korean / dark |
|---|---|---|
| Selection | [Legacy](en_US_light/legacy_selection.png) · [Next](en_US_light/next_selection.png) | [Legacy](ko_KR_dark/legacy_selection.png) · [Next](ko_KR_dark/next_selection.png) |
| Review | [Legacy](en_US_light/legacy_review.png) · [Next](en_US_light/next_review.png) | [Legacy](ko_KR_dark/legacy_review.png) · [Next](ko_KR_dark/next_review.png) |
| Version prompt | [Legacy](en_US_light/legacy_version_prompt.png) · [Next](en_US_light/next_version_prompt.png) | [Legacy](ko_KR_dark/legacy_version_prompt.png) · [Next](ko_KR_dark/next_version_prompt.png) |

The [English/light manifest](en_US_light/manifest.json) and
[Korean/dark manifest](ko_KR_dark/manifest.json) contain the parity assertions
and process snapshots. The capture test checks workbook/template/source-date
parity, review fields and candidate counts, localized title/actions, matching
version prompts, dialog extents and control geometry, and fit without a
horizontal scrollbar.

## Accepted visual differences

Main and Tester reviewed both locales. Next uses preview/review/apply guidance
and labels the workbook date as “Source date”; legacy uses import/status copy
and “Version.” Next presents automatic Native English/GS counts in a combined
sentence. The review viewport shows early Korean candidate rows at the bottom
where legacy shows the table heading/scroll area; this visible-row difference
was accepted. The version confirmation text and both dates match across legacy
and Next in each locale.

## Verification and limits

The independent Tester used the standard `build/macos-clang-debug` build and
passed the focused Next CTest suite 16/16. Earlier direct Bootstrap cases,
inventory checks, and `git diff --check` passed; the Tester recommended
acceptance. These captures are offscreen and do not verify native-dialog
behavior. No full suite, Windows run, or phase exit gate was verified.

The manifests record sequential whole-process macOS resident snapshots:

| Stage | English / light | Korean / dark |
|---|---:|---:|
| Before parse | 35.14 MiB | 35.22 MiB |
| After parse | 36.78 MiB | 37.00 MiB |
| After review | 75.44 MiB | 75.34 MiB |
| After successful apply | 75.72 MiB | 75.55 MiB |
| Peak after cleanup | 75.73 MiB | 75.56 MiB |

These samples include Qt, SQLite, parser, and fixture state. They do not isolate
per-import allocation or establish a memory budget. The 7A screenshot
comparison and memory evidence and the 7A/Phase 7 exit gates remain open.
