# Testing Classes and Blocks

Candidate #28 was accepted at source commit `9bdbe3aa`; capture metadata follow-up `c2ec78e9` refreshed the pending-slot records.

## Capture set

The paired captures use the tracked [96-class fixture](../../../release/large-classes-visual-states/generated-large-classes-visual.tps), seeded with 96 testing classes. Details and Notes captures select class ID `3`; the pending-slot state has no selected class and uses Thursday at 10:00. All images use a 1024×768 viewport. The [manifest](testing_classes_paired_capture.json) records 12 Legacy/Next state pairs across Details, Notes, and pending-slot entry; English (`en_US`) and Korean (`ko_KR`); and light and dark themes (24 PNGs total).

| State | Locale / theme | Legacy | Next |
|---|---|---|---|
| Details | en_US / light | [PNG](testing_classes_details_en_US_light_legacy.png) | [PNG](testing_classes_details_en_US_light_next.png) |
| Notes | en_US / light | [PNG](testing_classes_notes_en_US_light_legacy.png) | [PNG](testing_classes_notes_en_US_light_next.png) |
| Pending slot | en_US / light | [PNG](testing_classes_pending_slot_en_US_light_legacy.png) | [PNG](testing_classes_pending_slot_en_US_light_next.png) |
| Details | en_US / dark | [PNG](testing_classes_details_en_US_dark_legacy.png) | [PNG](testing_classes_details_en_US_dark_next.png) |
| Notes | en_US / dark | [PNG](testing_classes_notes_en_US_dark_legacy.png) | [PNG](testing_classes_notes_en_US_dark_next.png) |
| Pending slot | en_US / dark | [PNG](testing_classes_pending_slot_en_US_dark_legacy.png) | [PNG](testing_classes_pending_slot_en_US_dark_next.png) |
| Details | ko_KR / light | [PNG](testing_classes_details_ko_KR_light_legacy.png) | [PNG](testing_classes_details_ko_KR_light_next.png) |
| Notes | ko_KR / light | [PNG](testing_classes_notes_ko_KR_light_legacy.png) | [PNG](testing_classes_notes_ko_KR_light_next.png) |
| Pending slot | ko_KR / light | [PNG](testing_classes_pending_slot_ko_KR_light_legacy.png) | [PNG](testing_classes_pending_slot_ko_KR_light_next.png) |
| Details | ko_KR / dark | [PNG](testing_classes_details_ko_KR_dark_legacy.png) | [PNG](testing_classes_details_ko_KR_dark_next.png) |
| Notes | ko_KR / dark | [PNG](testing_classes_notes_ko_KR_dark_legacy.png) | [PNG](testing_classes_notes_ko_KR_dark_next.png) |
| Pending slot | ko_KR / dark | [PNG](testing_classes_pending_slot_ko_KR_dark_legacy.png) | [PNG](testing_classes_pending_slot_ko_KR_dark_next.png) |

Tester inspected all six Korean Next states and confirmed translated headings, fields, tabs, buttons, and pending-slot text after a translation-context fix. Inspection also covered representative English/light and Korean/dark pairs. In the pending-slot images, Legacy retains “Thursday 10:00” only in manager page state with no visible label; Next visibly labels the create state “New class for Thursday at 10:00” in English and “목요일 10:00에 수업 추가” in Korean. The four pending-slot Next manifest records carry `{ "weekday": "Thursday", "startMinute": 600, "startTime": "10:00" }`, set `newClassDraft` to true, and include the localized display string. The eight non-pending Next records use null for both pending-slot fields. Legacy records the weekday/start string in page state for its four pending-slot captures and null in the other eight. After capture metadata follow-up `c2ec78e9`, the capture target passed 1/1; JSON counts and the English/light and Korean/light Next pending-slot images were rechecked. The routes and shells differ, so these captures do not establish visual parity.

## Manager boundary and lifecycle

The accepted Next boundary is a list/model, one selected Details/Notes editor, create/update/delete, full cascade cleanup, and typed pending-slot create. It does not include Roster UI (candidate #35) or the weekly block/assignment editor (candidates #30/#32). Details and Notes select the same class; the pending-slot view is a new-class state.

Legacy records 178 active widgets and one editor widget. Next records 80 active widgets and one selected editor with 61 descendants. The Details snapshot clears on suspension. The manager is a page workflow, not the full weekly testing schedule or assignment editor.

## Whole-process memory observations

Manifest working-set and private-usage ranges across sequential macOS snapshots are:

| Route | Working set, bytes | Private usage, bytes |
|---|---:|---:|
| Legacy | 38,682,624–92,946,432 | 11,174,608–34,046,912 |
| Next | 87,556,096–93,061,120 | 29,950,912–34,161,600 |

These are whole-process observations, not page allocations or a memory budget.

## Verification and limitations

`ClassMngrNextTestingClassesManagerTests` passed 12/12. `ClassMngrNextTestingClassesVisualCapture` passed, including all Korean capture assertions. Separate Legacy persistence, delete, cancel, and schedule-handoff targets passed. `NextStartupWorkspaceDependencyBoundary`, `git diff --check`, resource inventory `--check`, and the Next route scan passed. The scanned Next UI/App/Platform path has no `ApplicationServices`, `DataService`, SQL, Gateway, or direct database reference.

The wider pre-fix 34-registration result and unchanged Legacy aggregate failures are recorded in the [Phase 7 progress entry](../../../../../../plans/qt-rewrite-heavy-route-plan/08-Phase-7-Feature-Migration.md). No broad Legacy aggregate pass is claimed.

No full-suite, Windows, visual-parity, or memory-budget result is claimed. The 7A evidence and 7A/Phase 7 exit gates remain open.
