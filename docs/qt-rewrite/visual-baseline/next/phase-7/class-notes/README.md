# Class Notes and Time Filler

Candidate #26 was accepted at source/test commit `833e9074`.

## Capture set

The paired captures use the tracked [96-class fixture](../../../release/large-classes-visual-states/generated-large-classes-visual.tps), selected class ID `1`, and a 1024×768 viewport. Legacy and Next Notes/Time Filler surfaces are captured in English (`en_US`) and Korean (`ko_KR`), light and dark: four pairs and eight PNGs. The [paired manifest](classes_notes_paired_capture.json) records the run.

| Locale / theme | Legacy | Next |
|---|---|---|
| en_US / light | [PNG](classes_notes_en_US_light_legacy.png) | [PNG](classes_notes_en_US_light_next.png) |
| en_US / dark | [PNG](classes_notes_en_US_dark_legacy.png) | [PNG](classes_notes_en_US_dark_next.png) |
| ko_KR / light | [PNG](classes_notes_ko_KR_light_legacy.png) | [PNG](classes_notes_ko_KR_light_next.png) |
| ko_KR / dark | [PNG](classes_notes_ko_KR_dark_legacy.png) | [PNG](classes_notes_ko_KR_dark_next.png) |

Independent Tester inspection covered the English/light and Korean/dark pairs.

## Crop, Save button, and page scope

The capture output is a top-left 1024×768 viewport crop. The Legacy Notes page is taller than the viewport; the manifest locates one Save widget in each Legacy capture, but it is hidden and outside the viewport in all four:

| Locale | Legacy page size | Save geometry (x, y, width, height) | State |
|---|---:|---:|---|
| en_US, light and dark | 1024×844 | 35, 817, 100, 30 | Hidden; not in viewport |
| ko_KR, light and dark | 1024×838 | 35, 811, 100, 30 | Hidden; not in viewport |

Next has 230 page widgets while active and after suspension, one selected editor root with 182 descendants, and zero class-scoped widgets; class-bound state clears on suspension. Legacy records 600 active widgets and one loaded, instantiated editor.

The active sequential macOS whole-process snapshots (working-set bytes / private-usage bytes) are:

| Locale / theme | Legacy | Next |
|---|---:|---:|
| en_US / light | 78,610,432 / 25,265,064 | 91,406,336 / 33,735,640 |
| en_US / dark | 91,979,776 / 34,292,696 | 92,078,080 / 34,391,000 |
| ko_KR / light | 93,274,112 / 34,653,144 | 93,732,864 / 35,062,744 |
| ko_KR / dark | 94,027,776 / 35,357,656 | 94,044,160 / 35,374,040 |

These are process-wide observations, not page allocations or a memory budget. The manifest retains the before/active/release or suspension snapshots. Legacy and Next layouts differ; this package does not claim visual parity.

## Verification

Affected targets built. The corrected focused CTest selection passed 13/13 across Bootstrap, PageHost, the dependency boundary, Navigator UI/capture/WorkspaceStore, Notes read/save contracts, Details save/query, Legacy Notes read/save ports, and existing Next Notes feature tests.

The real `.tps` integration test `classNotesPathActionsBoundTextAndPreserveDetailsTransactionally` passed three QtTest results including setup/cleanup. It verifies, for each field, 10,000 ASCII units succeed and 10,001 fail; persisted over-limit reads fail; 5,000 supplementary characters (10,000 UTF-16 units) succeed and 5,001 (10,002 units) fail; trimming; Notes-only writes preserve teacher and Details; trigger rollback and retry; and Details saves preserve both Notes fields. UI coverage includes Automatic debounce, Manual save, failed-draft retention, read retry, teacher-name failure, class-name subtitle fallback, over-limit draft retention, and Save/Discard/Cancel across class or workspace changes. Conflict propagation is tested at the Notes use-case contract; the `.tps` integration verifies rollback/retry, not a concurrent-edit conflict.

`git diff --check`, resource inventory `--check` (194 runtime records), and the bounded source scan passed. No full suite, Windows run, memory-budget result, or visual-parity result is claimed. The 7A screenshot/memory evidence and 7A/Phase 7 exit gates remain open.
