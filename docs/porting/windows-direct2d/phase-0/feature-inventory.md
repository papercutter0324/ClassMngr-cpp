# Phase 0 feature and workflow inventory

Source baseline: `48fc5c5` (the branch state before the Phase 0 additions).
`PageManager` is the primary screen registry; `ActionRegistry`, `MenuBuilder`, feature `ui` sources, and registered
Qt tests are the command and behavior evidence. “Capture” records the available
Qt observation or the remaining native comparison target.

The companion [capture ledger](capture-ledger.csv) assigns every named page,
dialog, menu/context command, and high-risk editor a stable artifact prefix and
capture status. It is the operational record for this inventory. A row may be
marked `accepted` when the product owner has explicitly accepted a
representative or source-backed Qt baseline and any native follow-up is named;
that acceptance does not establish native parity.

## Product pages and navigation

`PageManager` (`src/ui/shared/pages/pagemanager.h/.cpp`) owns eleven registered
destinations and their lifecycle. `MyWorkspacePage` owns the Personal Details,
My Schedule, and Calendar child surfaces; those three are not separate
`PageManager` destinations. My Workspace is the initial registered page and the
other registered destinations are created lazily by `showPage`; the normal
no-database startup then activates Campus Dashboard. A native navigation
coordinator must retain this distinction and preserve instantiated-page state.

| Page identifier | Current Qt page / major surface | Construction | Evidence | Capture |
| --- | --- | --- | --- | --- |
| `my-workspace` | My Information shell with Details, My Schedule, and Calendar tabs | initial | `features/my_info/ui/my_workspace_page.*` | accepted |
| `personal-details` | My Information / personal details, signature | embedded in My Workspace | `features/my_info/ui/personal_details_page.*` | accepted |
| `calendar` | academic calendar, events, preferences, upcoming events | embedded/lazy child | `features/calendar/ui/calendar_page.*` | accepted |
| `my-schedule` | personal schedule | embedded in My Workspace | `features/schedule/ui/schedule_page.*` | accepted |
| `my-classes` | personal classes | registered/lazy | `features/my_info/ui/my_classes_page.*` | accepted |
| `schedule` | class schedule, table and print | registered/lazy | `features/schedule/ui/schedule_*` | accepted |
| `classes` | class details, notes, roster and navigation tabs | registered/lazy | `features/classes/ui/classes_page.*` | accepted |
| `testing-classes` | testing classes and roster assignment | registered/lazy | `features/classes/ui/testing_classes_page.*` | accepted |
| `teacher-info` | teacher directory/detail | registered/lazy | `features/teacher/ui/teacher_info_page.*` | accepted |
| `native-english-teachers` | staff directory: Native English Teachers | registered/lazy | `features/teacher/ui/staff_directory_page.*` | accepted |
| `gs-team` | staff directory: GS Team | registered/lazy | `features/teacher/ui/staff_directory_page.*` | accepted |
| `campus-dashboard` | campus data, map, directions, housing and address | registered/lazy | `features/campus/ui/campus_dashboard_page_*` | accepted |
| `sub-prep` | substitute-preparation and document workflows | registered/lazy | `features/sub_prep/ui/sub_prep_page.*` | accepted |
| `pdf-viewer` | bundled/user PDF viewing and output controls | registered/lazy | `ui/shared/pages/pdf_viewer_page.*` | accepted |

Navigation also includes sidebar class/teacher tree operations and its context
menus (`ui/shared/widgets/sidebar/*`, `app/controllers/sidebar_controller.*`).
The native port must keep database-open/no-database states, admin-mode campus
access, page leave confirmation, current-page print/export capability changes,
and feature resource acquire/release behavior.

## Application commands and shortcuts

`ActionRegistry` is the complete top-level command registry
(`src/ui/shared/actions/action_registry.h`). `MenuBuilder` places the commands
in File, Edit, Classes, Teachers, Print / Export, Help, Admin, and (when
enabled) Developer menus. Preferences is an Edit-menu command constructed in
`src/app/menu_builder.cpp`.

| Group | Commands | Shortcut/behavior capture |
| --- | --- | --- |
| File | New, Open, Save, Save As, Export, Close, Exit, recent files | `QKeySequence::New/Open/Save/SaveAs`; recent-files state; unsaved-change confirmation |
| Page output | Print current page, Save current page as | enablement follows `PageOutputCapabilities`; PDF/print samples required |
| Edit | Undo, Redo, Cut, Copy, Paste, Preferences | `QKeySequence::Undo/Redo/Cut/Copy/Paste`; focus-sensitive routing; settings persistence and retranslation |
| Classes and teachers | New/Delete class, Import/Export classes, New/Delete teacher, Upcoming Birthdays, Import teachers | sidebar duplicates and context menus; import failures and confirmations |
| Settings | save mode; theme; language; font size; document spacing/background; AI provider and voice; sidebar tooltips/animation; automatic update checks | persistence, retranslation, theme changes, keyboard focus order |
| Help/admin/developer | Check for updates, About, Manage Campuses, Memory Usage Monitor | update/error paths, admin visibility, `Ctrl+Shift+M` monitor shortcut |

Feature-local editing is also a parity surface. Roster and speaking-evaluation
tables support clipboard operations and Delete; speaking evaluations add
fill-down (`Ctrl+D`) plus undo/redo. These are high-risk custom-control flows,
not merely menu shortcuts.

### Preferences and context commands

Preferences has seven tabs: General, Appearance, Documents, Schedule, Calendar,
Navigation Bar, and AI Comments. Capture setting persistence, immediate visual
updates, English/Korean retranslation, checkbox/radio focus behavior, and the
destructive “Clear Testing Layout” confirmation.

The sidebar exposes context-specific commands in addition to the menu bar:
Add Class, Add Teacher, Export Class, Delete Class, and Delete Teacher. Their
enablement depends on whether a database is open and, for export, on a valid
class identifier. Capture no-database, root, class, teacher, and blank-area
menus.

## Dialogs, wizards, prompts, and file selection

All ordinary prompts are governed by [`docs/dialog-policy.md`](../../../dialog-policy.md).
The Windows-native replacement keeps the same typed request/result behavior,
including safe Escape/default choices and Save/Discard/Cancel unsaved-change
outcomes.

| Area | Dialog/wizard surface | Evidence |
| --- | --- | --- |
| Setup and application | Initial Setup Wizard, About, License, Update, Memory Usage | `features/setup/ui/initial_setup_wizard.*`; `ui/shared/dialogs/*` |
| Calendar | Calendar Event | `features/calendar/ui/calendar_event_dialog.*` |
| Classes and roster | Class Import/Export, Roster Print, Record Selection | `features/classes/ui/*_dialog.*`; `features/roster/ui/roster_print_dialog.*` |
| Schedule | editor, import, import review/conflict resolution, testing assignment, print | `features/schedule/ui/*dialog.*` |
| Teachers | Teacher Import, Upcoming Birthdays | `features/teacher/ui/*dialog.*` |
| Speaking evaluations | notes, AI batch, batch export, report | `features/speaking_eval/ui/*dialog.*` |
| Sub preparation and generic output | Sub Prep Print, PDF Print | `features/sub_prep/ui/sub_prep_print_dialog.*`; `ui/shared/printing/pdf_print_dialog.*` |
| Native file/folder selection | typed open/open-many/save/select-folder requests | `ui/shared/dialogs/file_dialog_service.*` |

## Imports, exports, background work, and platform behavior

| Contract | Qt implementation evidence | Required native observation |
| --- | --- | --- |
| Database lifecycle | `app/controllers/file_controller.*`, `core/database_file_format.*`, `data/data_service.*` | `.tps` create/open/save-as/export; legacy `.db`; recent files; failed and cancelled paths |
| Schema migration/rollback | `data/database/database_schema_manager.*`, `tests/database_schema_manager_tests.cpp` | v0–v6 migration semantics, backups, failed preflight and transaction rollback |
| Class and teacher import/export | `features/classes/*import*`, `features/teacher/*import*`, `app/controllers/sidebar_controller_*` | valid, partial, duplicate, cancelled, bad input, and visible summaries |
| Schedule import | `features/schedule/services/*import*`, `ui/schedule_import*` | workbook conflict review, proposed updates, cancellation, error state |
| PDF/document/print | `ui/shared/pages/pdf_viewer_page.*`, `ui/shared/printing/*`, `features/documents/*` | view/search/zoom if present, print selection/pages, PDF output and margins |
| Reports | schedule, roster, sub-prep, speaking-evaluation report services | English/Korean pagination, printed/exported PDFs, cancellation/failure |
| PowerPoint fallback | `features/speaking_eval/services/speaking_eval_powerpoint_*` | desktop PowerPoint availability, COM launch, timeout/cancel, temp-file cleanup |
| Updates/resource packs | `core/updater/*`, `core/resource_packs/*` | signature failures, download/cancel, installer handoff, mounted-pack precedence |
| Diagnostics | `core/memory_usage_*`, `docs/memory-profiling-windows.md` | lifecycle, rendering/asset release, redaction, metrics export |
| Windows services | `update_signature_verifier.cpp`, `process_memory_snapshot.*`, `pdf_print_dialog_*`, installer CMake | CryptoAPI verification, memory counters, Windows print dialog, Inno Setup upgrade/install |

## Existing automated baseline evidence

The Phase 0 fixture corpus will consolidate currently programmatic coverage;
it must not replace it. Representative registered tests include PageManager,
dialog services/policy, database schema manager, data-service lifecycle,
schedule/teacher/calendar imports, roster and schedule widgets, PDF/print
models, resource packs, updater/signature verifier, campus map/dashboard,
speaking-evaluation report and batch service, and startup performance. See the
`cmake/tests/*.cmake` registrations and the fixture mapping in
[database-fixture-contract.md](database-fixture-contract.md).

## Required manual capture state

These are the full native comparison targets. Before the native target exists,
use representative captures and explicit owner acceptance for repeated or
well-known Qt behavior, and carry native-only differences forward to the
relevant slice gate. UI Automation, Narrator, and high-contrast evidence are
deferred from the current roadmap. Use
[reference-capture.md](reference-capture.md) for the exact artifact layout.
