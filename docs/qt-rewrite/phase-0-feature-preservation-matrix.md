# Qt Rewrite Phase 0 - Feature Preservation Matrix

Status: In progress
Snapshot: `75755460`

Every row is a required v2 vertical slice. A row is not accepted merely because
its page renders: persistence, localization, permissions, resources, output,
and failure behavior must be covered together.

| Feature slice | Current entry point | Behavior that must remain | Current acceptance evidence | v2 status |
| --- | --- | --- | --- | --- |
| Setup and workspace/file flows | `MainWindow`, `FileController`, `InitialSetupWizard` | Create/open/save/save-as/close/export, recent files, initial setup, backup/cancel behavior, no-database state | `database_file_format_tests`, `database_schema_manager_tests`, `initial_setup_wizard_tests`, `data_service_lifecycle_tests`; representative startup fixture | Inventory complete; representative fixture added; failure fixtures pending |
| My Workspace and personal information | `MyWorkspacePage`, `PersonalDetailsPage` | My Details, My Schedule, deferred Calendar tab, personal details/signature, autosave/manual save, Korean input | `my_workspace_page_tests`, `typed_signature_renderer_tests`, `signature_image_processor_tests`, `calendar_*` tests | Inventory complete; visual baseline pending |
| Teachers and staff | `TeacherInfoPage`, `StaffDirectoryPage`, sidebar teacher routes | Teacher CRUD, preferred names, birthday validation, connectivity/notes, Korean/native/GS directories, Excel import | `teacher_info_page_tests`, `teacher_info_section_tests`, `staff_directory_page_tests`, `teacher_import_tests`, `teacher_import_dialog_tests`, `upcoming_birthdays_tests` | Inventory complete; fixture pending |
| Classes | `ClassesPage` | Class selection, day/grade filters, Details/Roster/Analytics/Evaluations/Co-Teacher/Notes, class CRUD, class transfer | `classes_page_tests`, `class_transfer_tests`, `class_analytics_ranking_model_tests`, `evaluation_default_selection_tests`, `roster_model_tests`, `speaking_evaluation_service_tests`; representative startup fixture and `tests/fixtures/transfers/conflict_source.json` | Inventory complete; populated and conflict fixtures added; visual baseline pending |
| Schedule and imports | `SchedulePage`, `ScheduleWidget`, import dialogs | Regular/intensive schedule, editor, conflict review, teacher/schedule import, testing-class entry, PDF/print output | `schedule_widget_tests`, `schedule_builder_tests`, `schedule_import_tests`, `schedule_import_dialog_tests`, `schedule_print_model_tests`, `schedule_print_pdf_tests`, `class_transfer_tests`; representative/large workspace fixtures and class-transfer conflict fixture | Inventory complete; regular/intensive and class-transfer conflict fixtures added; schedule-workbook conflict fixture pending |
| Calendar | `CalendarPage`, embedded QML `EventCalendar` | Month navigation, event CRUD/import, upcoming events, theme/language behavior, deferred QML construction | `academic_calendar_tests`, `calendar_event_repository_tests`, `calendar_event_cache_tests`, `calendar_import_tests` | Inventory complete; cross-platform capture pending |
| Rosters | `RosterEditorWidget`, `RosterModel` | Dynamic columns/widths, names and validation, duplicate-name handling, row transfer, score import, print/template output | `roster_model_tests`, `roster_template_print_service_tests`, `sub_prep_print_pdf_tests`; representative startup fixture | Inventory complete; two-roster fixture added; large roster fixture pending |
| Speaking evaluations | `SpeakingEvalPage` and report dialogs/services | Evaluation tables, undo/redo, notes, name matching, analytics, AI prompt workflows, batch reports, PowerPoint/PDF output | `speaking_evaluation_service_tests`, `speaking_analytics_tests`, `speaking_eval_report_widget_tests`, `speaking_eval_batch_report_service_tests`, `powerpoint_data_access_notice_tests` | Inventory complete; output references pending |
| Campus and documents | `CampusDashboardPage`, `DocumentCatalog`, PDF viewer | Campus information/address/directions/housing/maps, admin editing, metadata-only document catalog startup, on-demand QtPdf open/copy/save/zoom/navigation, release on close or leave, localized names | `campus_map_tests`, `campus_dashboard_page_tests`, `document_catalog_tests` | Inventory complete; document/catalog fixture and viewer lifecycle evidence pending |
| Substitute preparation and output | `SubPrepPage`, package/print services | Substitute settings, class information, materials, grading, package generation, roster/sub-prep PDFs | `sub_prep_page_tests`, `sub_prep_class_information_model_tests`, `sub_prep_package_service_tests`, `sub_prep_print_pdf_tests` | Inventory complete; large-workspace fixture pending |
| Shell, preferences, permissions, updates | `MainWindow`, `MenuBuilder`, `ActionRegistry`, controllers | Menus/actions, shortcuts, language/theme/font settings, admin gates, updater | `sidebar_structure_tests`, `navigation_tab_widget_tests`, `startup_visual_settings_tests`, `language_service_tests`, `fontmanager_tests`, `updater_tests` | Inventory complete; screenshot/state snapshots pending |

## Explicit v2 exclusions

The developer-only Memory Usage Monitor and its in-app process-memory,
attribution, and lifecycle diagnostics are not part of the rewrite. They do not
require a parity slice or v2 replacement. Release memory acceptance remains an
external measurement concern, and startup-performance instrumentation remains
available to the engineering test harness.

## Required parity dimensions

- English and Korean translations, including live retranslation.
- System, light, and dark theme paths plus font-size choices.
- Empty, populated, read-only, loading, error, warning, and confirmation
  states.
- Automatic/manual save behavior and unsaved-change prompts.
- Admin-only and developer-only actions.
- Keyboard shortcuts, context menus, table editing behavior, and focus order.
- Printed and generated PDF, roster, report, substitute, and PowerPoint output.
- Document viewer loading, ready/error, close, release, and reopen behavior.
- Startup with no database, a normal database, and a large representative
  database.

## Acceptance rule

Before a v2 slice begins, add or identify a fixture and an automated acceptance
check for its persistent behavior. Phase 0 still has open fixture and visual
work, so no v2 feature implementation is authorized by this matrix yet.
