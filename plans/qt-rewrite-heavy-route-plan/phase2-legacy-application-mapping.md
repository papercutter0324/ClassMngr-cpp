# Phase 2 legacy application mapping

This document maps the current legacy application boundary to the committed
v2 application contracts. The typed Sidebar/MainWindow catalog cutover follows
baseline commit `662e5f2`; the earlier mapping, resolver, and document-folder
handoffs are retained below. The runtime worker bridge, limited document route
slice, bounded resource/platform document resolver, typed catalog ownership,
calendar read-projection adapter, metadata enrichment, calendar cache
worker-boundary slice, narrow typed calendar cache/model boundary, and narrow
typed upcoming-events retrieval and next-ten prefetch read cutovers, typed
calendar activation-read, non-repeat single-event save and delete,
repeat-series suffix-delete, this-and-following repeat-series edit/save, and
this-event-only repeat-occurrence save, and new-repeat series-create
and calendar-dialog edit-draft and constructor/input ownership boundaries,
theme and language preference bridges are implemented, as are the custom-color
palette caller boundary, Calendar Import planning/signature-query/application
use-case seams with the shared six-field signature identity carried as a typed
value end-to-end, the partial Schedule
import state-validation contract and repository
pre-write cutover, the shared Qt-free Class Transfer review-decision contract
and repository validation,
and Sub Prep print-source, selected-class details, and schedule-summary read
adapters plus the Sub Prep information-sheet output wiring. Personal-details
save, personal-signature, and current-campus preference caller boundaries
also use their existing typed ports through `ApplicationServices`. A partial
content-session integration covers referenced
`PdfViewerPage` descriptors; broader legacy ownership and feature cutover
remain open, including generic settings persistence.

## Authority and current boundary

- Legacy facade: [`ApplicationServices` header](../../src/core/application_services.h), [`ApplicationServices` implementation](../../src/core/application_services.cpp).
- Legacy file lifecycle/UI boundary: [`FileController` header](../../src/app/controllers/file_controller.h) and [`FileController` implementation](../../src/app/controllers/file_controller.cpp).
- Legacy service ownership: [`DataService`](../../src/data/data_service.h) and [`feature_services.h`](../../src/app/services/feature_services.h).
- v2 workspace contracts: [`workspace_contracts.h`](../../src/next/application/workspace_contracts.h), [`workspace_use_case.h`](../../src/next/application/workspace_use_case.h), [`workspace_coordinator.h`](../../src/next/application/workspace_coordinator.h), [`workspace_state.h`](../../src/next/application/workspace_state.h), and [`selection_state.h`](../../src/next/application/selection_state.h).
- v2 result shape: [`operation_result.h`](../../src/next/domain/operation_result.h).

`ApplicationServices` owns a `DataService`, a `ThemeService`, and a
`DocumentCatalog`; the feature-service accessors lazily create services that
hold the legacy data/session boundary. `FileController` owns the Qt-facing
file dialogs, path normalization, recent-file updates, warning display, and
window/action state around those calls. The v2 application headers remain
legacy-free; the committed outer `Platform` document-catalog adapter maps
metadata, `DocumentContentResourcePort` resolves resource-backed primary and
optional export paths, and `NavigationController` projects the optional
content reference to the viewer. `PdfViewerPage` now owns the partial
content-session lifecycle for referenced descriptors. `Sidebar` owns a copied
or move-assigned `Application::DocumentCatalogProjection` with no legacy
`DocumentCatalog` pointer, include, or dependency. `MainWindow::initializeSidebar`
and `MainWindow::retranslateUi` request locale-specific projections through
`Platform::ApplicationServicesDocumentCatalogPort` and pass them by value;
projection failure supplies an empty projection. Broader facade/service cutover
remains outside this slice. The theme bridge is described in the verified
handoff below; the language bridge is also recorded below. Generic settings
persistence, the schedule-output theme read, and other feature-service
migrations remain later slices.

## Document-folder hierarchy metadata prerequisite

After baseline commit `fd695fd`, `DocumentFolderMetadata` carries bounded
`parentPath` metadata, empty for roots, and projection validation handles it.
`ApplicationServicesDocumentCatalogPort` copies legacy
`DocumentFolderDefinition::parentPath`. The transfer preserves nested folder
hierarchy for the completed `Sidebar`/`MainWindow` projection cutover while the
application layer remains Qt-free and aggregate initialization remains
compatible.

Configure/ownership/dependency checks passed at 705 handwritten files;
focused projection/adapter CTest passed 2/2; Qt-free application and
standalone syntax checks passed; and `git diff --check` passed. The embedded
fixture contains root folders only, so nested adapter transfer lacks runtime
coverage; nested projection behavior is covered. This prerequisite was
preparatory and did not itself cut over `Sidebar`/`MainWindow`; the completed
cutover is recorded below. Phase 2 remains in progress.

## Document route and content-session status

The accepted document work is intentionally partial. `NavigationController`
uses the projected `DocumentContentReference` when building
`PdfViewerDocumentDescriptor`. For referenced descriptors,
`PdfViewerPage` requests and begins loading before `QPdfDocument` loading,
maps Qt `Ready`/`Error` to the session state, and releases the session after
`QPdfDocument::close()` on replacement, navigation, or destruction.
Descriptors without a content reference retain direct-loading compatibility.

The bounded resource/platform resolver is complete. `DocumentContentResourcePort`
accepts `ResourcePackManager&`, validates `resource://documents/` references
including malformed, empty, and traversal rejection, acquires one
documents-pack lease, resolves primary and optional export paths, and returns
a move-only lease/path value. `NavigationController` uses it instead of
directly acquiring or parsing `ResourcePaths::Documents`; `PdfViewerPage`
closes the PDF before releasing the lease, and descriptors without a reference
retain direct-loading compatibility. The application layer remains Qt-free.
Close-before-release is source-order verified; invalid UTF-8 and live UI
integration lack direct coverage. Typed `MainWindow`/`Sidebar` catalog
ownership is now complete, but the partial content-session/resolver integration,
other legacy service accessors, and feature migrations do not complete Phase 2.

## Legacy container construction and lifetime mapping

| Legacy symbol | Current role | v2 destination / status | Owner / layer |
| --- | --- | --- | --- |
| Public `ApplicationServices()` and `ApplicationServices(std::unique_ptr<ThemeService>)` constructors plus `~ApplicationServices()` ([`application_services.h`](../../src/core/application_services.h#L23)) | The default constructor delegates to the injectable-theme constructor, which creates the owned `DataService`, default-or-injected `ThemeService`, and `DocumentCatalog`; feature services remain lazy. The destructor releases the `unique_ptr`-owned aggregate after legacy consumers are torn down. | No v2 application contract reproduces this aggregate service container or its public lifecycle API. A future outer composition root/adapter must own and sequence the legacy container; that work is not implemented. | Legacy application root now; future outer composition root/adapter. |

## Workspace lifecycle and persistence mapping

The v2 destination is an outer `WorkspaceGateway` implementation called by
`WorkspaceUseCase`; `WorkspaceCoordinator` supplies the state guards and
commits successful transitions to `WorkspaceState` and `SelectionState`.
`WorkspaceLocation` is an adapter-neutral `std::string` value. The gateway
seam, not the v2 contracts, owns conversion to or from `QString` and legacy
path types. v2 operations return `Domain::Result<T>` or `Domain::Result<void>`
with `Domain::OperationError`.

| Legacy symbol | Current call sites / role | v2 destination / status | Owner / layer |
| --- | --- | --- | --- |
| `openDatabase(QString)` | [`FileController::loadDatabase`](../../src/app/controllers/file_controller.cpp#L552) routes the selected path through the workspace coordinator; new-database and initial-setup paths prepare replacement in FileController before coordinator create/open. | `WorkspaceGateway::openWorkspace` -> `WorkspaceUseCase::openWorkspace` -> `WorkspaceCoordinator::openWorkspace`; successful commit uses `WorkspaceState::open` and clears `SelectionState`. Integrated in FileController through `ApplicationServicesWorkspacePort` and `WorkspaceCoordinator`. | Qt/file outer adapter; v2 application use case, coordinator, and state. |
| `closeDatabase()` | [`FileController::closeActiveDatabase`](../../src/app/controllers/file_controller.cpp#L1108) uses the workspace coordinator for an open v2 session, then clears UI file state on success. | `WorkspaceGateway::closeWorkspace` -> `WorkspaceUseCase::closeWorkspace` -> `WorkspaceCoordinator::closeWorkspace`; successful close uses `WorkspaceState::close` and clears `SelectionState`. Integrated in FileController through `ApplicationServicesWorkspacePort` and `WorkspaceCoordinator`. | FileController UI boundary, `ApplicationServicesWorkspacePort`, and v2 coordinator/state. |
| `hasOpenDatabase()` | FileController and MainWindow retain direct `ApplicationServices::hasOpenDatabase()` guards; the workspace port also provides the gateway read used for handle validation. | Read `WorkspaceState::snapshot()` and its optional session/lifecycle; do not add a v2 boolean wrapper around `ApplicationServices`. `ApplicationServicesWorkspacePort` provides the gateway read; FileController and MainWindow retain direct legacy availability guards at the outer UI boundary. | v2 application state; UI adapter consumes a copy. |
| `currentDatabasePath()` | [`FileController`](../../src/app/controllers/file_controller.cpp) still reads it for UI dialog/current-file paths; the workspace port normalizes it into the gateway handle and session location. | Read the current `WorkspaceSession::location()` from `WorkspaceStateSnapshot`; convert the adapter-neutral location back to a UI path at the outer boundary. The port normalizes the legacy path into the workspace gateway; FileController retains legacy path reads for UI location and fallback behavior. | v2 workspace state; Qt/file adapter owns conversion. |
| `saveDatabase()` | [`FileController::saveDatabase`](../../src/app/controllers/file_controller.cpp#L912) routes an open v2 session through the coordinator for manual save and autosave; its legacy fallback calls void `DataService::save()`. | `WorkspaceGateway::saveWorkspace` -> `WorkspaceUseCase::saveWorkspace` -> `WorkspaceCoordinator::saveWorkspace` -> `WorkspaceState::markSaved`. Integrated through the port and coordinator; the adapter checks the open-workspace postcondition after the legacy void save and returns a structured result. | FileController UI boundary, `ApplicationServicesWorkspacePort`, and v2 coordinator/state. |
| `saveDatabaseAs(QString)` | [`FileController::saveDatabaseAs`](../../src/app/controllers/file_controller.cpp#L951) normalizes the destination and routes an open v2 session through the coordinator; the legacy fallback calls the facade then reloads. | `WorkspaceGateway::saveWorkspaceAs` -> `WorkspaceUseCase::saveWorkspaceAs` -> `WorkspaceCoordinator::saveWorkspaceAs` -> `WorkspaceState::markSavedAs`; the port saves and reopens the destination before returning the observed location. Integrated through FileController, the port, and coordinator. | Qt/file outer adapter; v2 use case, coordinator, and state. |
| `exportDatabaseAs(QString)` | [`FileController::exportDatabaseAs`](../../src/app/controllers/file_controller.cpp#L1033) normalizes the destination and routes an open v2 session through the coordinator; success remembers the directory without replacing the workspace. | `WorkspaceGateway::exportWorkspace` -> `WorkspaceUseCase::exportWorkspace` -> `WorkspaceCoordinator::exportWorkspace`; the coordinator does not mutate workspace or selection state. Integrated through FileController, the port, and coordinator. | Qt/file outer adapter; v2 use case/coordinator. |

The v2 coordinator guards dirty replacement and dirty close before gateway
calls, commits state only after a successful result, and clears selection only
after a successful open/create/close. Closed-workspace save, save-as, and
export return structured `NotFound` without calling the use case. These are
the committed v2 semantics in [`workspace_coordinator.h`](../../src/next/application/workspace_coordinator.h). FileController composes `ApplicationServicesWorkspacePort` with the gateway, use case, and coordinator for create/open/close/save/save-as/export. The [`F42 FileController tests`](../../tests/file_controller_workspace_lifecycle_tests.cpp) verify that failed production replacement-open preserves the active database, settings sentinel, recent/last-file entries, and action availability; create aborts before target preparation if coordinator close fails. The formal WorkspaceGateway/WorkspaceCoordinator create criterion is satisfied. Separate non-gating UI caveats remain: FileController obtains dirty-page approval from MainWindow, and new/initial-setup create closes the active session before later file preparation and coordinator creation succeed. Same-path replacement and the complete MainWindow snapshot have no direct integration coverage.

`WorkspaceGateway::createWorkspace` has no one-to-one public
`ApplicationServices` method. The current new/initial-setup paths perform file
replacement or backup work in [`FileController`](../../src/app/controllers/file_controller.cpp#L173)
before calling `openDatabase`; creation therefore needs its own adapter
decision and must not be inferred from the open mapping.

## Service and data accessors: future slices

The following public accessors remain legacy compatibility entry points. They
must not be wrapped into a v2 facade in this slice. Each feature service needs
its own typed application contract, adapter, tests, and release boundary in a
later Phase 2 slice.

| Legacy symbol | Current call sites / role | v2 destination / status | Owner / layer |
| --- | --- | --- | --- |
| `dataService()` | Used by `ApplicationServices` lazy factories; [`DataService`](../../src/data/data_service.h#L52) is the compatibility persistence facade. | No v2 destination yet. Future persistence/application slices replace operations, not the raw pointer accessor. | `ApplicationServices` owns it; legacy data layer. |
| `settingsService()` | Menu preferences, setup, personal-info, class-detail, and speaking-evaluation UI; representative call sites are [`menu_builder.cpp`](../../src/app/menu_builder.cpp#L246) and [`initial_setup_wizard.cpp`](../../src/features/setup/ui/initial_setup_wizard.cpp#L380). | Future settings/preferences contract; no v2 wrapper. | Legacy feature service; future v2 application slice. |
| `teacherService()` | Navigation, setup/import, roster, and teacher-facing UI; representative call sites are [`navigation_controller.cpp`](../../src/app/controllers/navigation_controller.cpp#L118) and [`initial_setup_wizard.cpp`](../../src/features/setup/ui/initial_setup_wizard.cpp#L154). | Partial F44 Teacher Import review-decision boundary: Qt-free [`import_review_session.h`](../../src/next/application/import_review_session.h) is shared by dialog readiness/plan creation and repository apply validation. The repository retains teacher identity checks, source-date handling, and transactional persistence. F46 adds Qt-free [`Domain::KoreanTeacherKey`](../../src/next/domain/korean_teacher_key.h), used by [`TeacherImportNameUtils`](../../src/features/teacher/import/teacher_import_name_utils.h) and Schedule Import matching via [`ScheduleImportMatchingProjection`](../../src/next/application/schedule_import_matching_projection.h). It filters the established Hangul code-unit ranges without trimming or normalization and preserves existing empty-key caller behavior. Neither slice migrates `TeacherService` or broad teacher workflows. | Domain identity and decision contracts with legacy UI/repository edges; remaining teacher-service behavior. |
| `classService()` | Navigation, classes, roster, schedule, speaking evaluation, and setup use class CRUD/import data; representative calls are [`classes_page.cpp`](../../src/features/classes/ui/classes_page.cpp#L179) and [`navigation_controller.cpp`](../../src/app/controllers/navigation_controller.cpp#L234). | Partial F43 Class Transfer boundary remains. F45 adds only the Qt-free [`Domain::Course`](../../src/next/domain/course.h) grade/level catalog: [`ClassInfoConfig`](../../src/features/classes/config/class_info_config.cpp) adapts it to existing Qt lists, and [`ScheduleImportPlanValidator`](../../src/features/schedule/services/schedule_import_plan_validator.cpp) validates imported pairs through it. F55 adds the shared grade-only classifier used by Classes, Evaluation Default Selection, and Schedule; each caller retains its own normalization boundary and policy. F57 exercises production `EvaluationDefaultSelection::forClass` against current/fallback periods, missing saved schedule, and missing ClassInfo. This remains a bounded Domain/Application rule cutover, not a broad `classService()` migration; other class CRUD/import behavior remains legacy/future. | Domain course values and Evaluation Default Selection contract with legacy Qt-list/repository edges; remaining class service behavior. |
| `scheduleService()` | Menu, schedule UI, testing-class UI, and roster output consume schedule/testing operations; representative calls are [`menu_builder.cpp`](../../src/app/menu_builder.cpp#L195) and [`schedule_widget.cpp`](../../src/features/schedule/ui/schedule_widget.cpp#L307). | Partial F37/F38/F39/F40/F41/F47/F48 boundary: `Application::validateScheduleImportState` validates the adapted plan and snapshots before writes; [`ScheduleImportMatchingProjection`](../../src/next/application/schedule_import_matching_projection.h) powers [`ScheduleImportRepository::preview`](../../src/data/repositories/schedule_import_repository.cpp), replacing the legacy matcher. F58 types its app-less teacher/class identity keys as `Domain::TeacherId`/`Domain::ClassId` and returns the suggested class as optional; the legacy repository adapter converts numeric IDs back to the existing integer preview. F39's [`schedule_import_overlap_projection.h`](../../src/next/application/schedule_import_overlap_projection.h) is shared by review presentation and apply-time validation; F40 supplies `Domain::Weekday`/`Domain::ScheduleTime` values through that projection. F41's [`schedule_import_review_decisions.h`](../../src/next/application/schedule_import_review_decisions.h) validates teacher/class choices for dialog readiness and PlanValidator. F47 adds Course-owned weekly meeting-day policy used by production parse and plan/apply validation; raw parsing and translated diagnostics remain at the feature edge. F48 adds Qt-free [`Domain::ScheduleEntry`](../../src/next/domain/schedule_entry.h), pairing typed `ClassId` and validated `ScheduleTime`; after resolving actual target class IDs, the repository builds entries from final times and passes them to the persistence writer, including retained/skipped rows. The writer preserves original day/time SQL text at the adapter edge and checks it against the typed values; existing write order, transaction, rollback, Skip, and intensive branches remain intact. Qt matching keys are normalized at the edge while raw room text remains for display; translations remain at the UI edge. Required fixtures cover Schedule preview, conflict review/apply, explicit review choices through persisted apply, F47 accepted-pattern persistence/rejected-pattern no-write behavior, and F48 typed-entry persistence plus seeded overlap rejection; broader schedule persistence and other schedule/testing operations remain legacy. | Application validation, matching, overlap projection, review-decision, and typed Domain time/course/schedule-entry rules plus legacy repository/UI edges; remaining schedule feature and persistence behavior. |
| `calendarService()` | Menu still consumes legacy calendar events directly ([`menu_builder.cpp`](../../src/app/menu_builder.cpp#L391)); Sub Prep routes its interval read through the typed query and Platform adapter ([`sub_prep_page.cpp`](../../src/features/sub_prep/ui/sub_prep_page.cpp#L488)). Calendar feature callers use typed Platform/Application ports; a source search found no `ApplicationServices::calendarService()` getter calls in `src/features/calendar/`. | `Platform::ApplicationServicesCalendarEventPort` maps `CalendarService::eventsInRange` into an owned typed `Application::CalendarEventProjection`, with bounded copied metadata including `eventType`, `timeStatus`, and optional `repeatSeriesId`, typed IDs, explicit all-day/unknown-time policy, and validation of range, service, technical, ID/metadata, partial-time, and capacity failures. Sub Prep uses `SubPrepCalendarEventIntervalsQuery` and `ApplicationServicesSubPrepCalendarEventIntervalsPort` for Vacation/Holiday intervals from January 1 of the captured current calendar year through December 31 of the following calendar year (at most two years), bypassing this generic projection cap. The port also exposes typed `projectionById(int)` for activation reads, while `ApplicationServicesCalendarEventDeletePort` maps typed `CalendarEventId` deletion to legacy `CalendarService::deleteEvent` and returns typed failure results for its boundary cases. `CalendarEventDialog::eventData()` returns a typed `CalendarEventEditDraft`; private `legacyEventData()` retains the dialog-local Qt/legacy conversion used for validation, and the constructor/member now use the draft by value. `ApplicationServicesCalendarEventSavePort` maps typed draft-derived requests to `CalendarService::saveEvents({event})` and returns `Result<CalendarEventId>`; `ApplicationServicesCalendarEventSeriesEditPort` maps a typed `CalendarEventSeriesEditRequest` by loading `repeatSeriesFromDate(repeatSeriesId, startDate)`, applying the date offset, duration, and edited-field propagation, then saving through `saveEvents(updatedEvents)` with `Result<void>`; `ApplicationServicesCalendarEventSeriesCreatePort` maps a typed Qt-free `CalendarEventSeriesCreateRequest` to one ordered legacy batch and one atomic `saveEvents(events)` call, returning typed occurrence IDs; `ApplicationServicesCalendarEventSeriesDeletePort` maps a bounded typed repeat-series suffix-delete request to legacy `CalendarService::deleteRepeatSeriesFromDate` and returns `Result<void>`. It trims only `repeatSeriesId`, preserves existing mapped-field whitespace, and returns structured failures for blank, over-bounds, or malformed fields. `CalendarEventCache` retains typed summaries and exposes date-scoped and range-scoped typed projections; `CalendarEventModel` consumes typed rows and converts to QML types only at the UI boundary. Legacy `eventsForDate`/`eventsInRange` compatibility remains for other callers; `calendar_page_upcoming_events.cpp` uses `CalendarEventSummary` for its narrow retrieval/filtering/formatting/row-rendering path, and `CalendarPage::ensureNextTenEvents` uses the typed range projection with the typed `filterUpcomingEvents` overload. `calendar_page_events.cpp` maps typed summary values directly to `CalendarEventEditDraft` on activation and creates drafts for new events; edit and mutation paths no longer round-trip through a legacy `CalendarEvent` record. It consumes drafts for all typed save, series-create, and series-edit requests; existing typed edit/save/delete/dialog paths, defaults, validation, inline errors, warnings, repeat/delete/mutation behavior, `schedule_use_24h`, routing, and invalidation/refresh remain preserved. | Platform calendar read adapter, dialog edit-draft and constructor/input ownership boundaries, cache/model, upcoming-read, next-ten-prefetch, activation-read, non-repeat save/delete, repeat-occurrence save, this-and-following repeat-series edit/save, new-repeat series-create/batch-save, repeat-series suffix-delete, and Sub Prep interval query/Platform adapter/page cutover; worker-boundary adapter and the current-campus availability and upcoming-event option-preference caller boundaries; broader calendar input/lookup and typed UI/page migration remain future. |
| `rosterService()` | Roster editors/printing, class pages, sub-prep, and speaking evaluation use roster operations; representative calls are [`roster_editor_widget.cpp`](../../src/features/roster/ui/roster_editor_widget.cpp#L64) and [`roster_print_dialog.cpp`](../../src/features/roster/ui/roster_print_dialog.cpp#L445). | Future roster use cases/projections; no v2 wrapper. | Legacy feature service; future roster slice. |
| `speakingEvaluationService()` | Speaking-evaluation pages, analytics, roster score import, and Evaluation Default Selection use saved evaluation data; representative calls are [`speaking_eval_page.cpp`](../../src/features/speaking_eval/ui/speaking_eval_page.cpp#L198) and [`class_analytics_page.cpp`](../../src/features/classes/ui/class_analytics_page.cpp#L526). | F57 adds Qt-free [`Application::EvaluationPeriod`](../../src/next/application/evaluation_default_selection.h) selection and an Evaluation Default Selection feature adapter while retaining the legacy service edge and exact period labels. `EvaluationDefaultSelection::forClass` has temporary-database integration coverage through `ApplicationServices`; failed class-info or evaluation reads return no default. The integration explicitly covers missing ClassInfo and missing saved schedule, but not an evaluation-read failure result. No broad speaking-evaluation service migration. | Qt-free Application period contract with feature adapter at the legacy service edge; remaining evaluation/analytics behavior. |
| `themeService()` | [`MainWindow`](../../src/app/mainwindow.cpp#L480) is passed explicitly to the theme controller; `ScheduleWidget` resolves the current theme at the caller boundary. | `Platform::ThemePreferencePort` maps typed `Application::ThemePreference` (`SystemDefault`, `Light`, `Dark`) to the legacy `ThemeService`. `ThemeController` owns typed `UserPreferencesState`, synchronizes the persisted `ActionRegistry` theme without reapplying at connection, and applies valid changes through the port while preserving invalid-input/state atomicity, persistence, icon refresh, and live palette behavior. `ScheduleOutputController` receives resolved `Theme` explicitly and no longer includes or accesses `ThemeService`/`currentTheme`; the direct-theme accessor is closed. | Platform theme adapter plus UI controller and schedule-output boundary; generic settings/application-services seams remain open. |
| `LanguageService` | `MainWindow` passes the legacy service explicitly to `LanguageController`; the controller owns the typed preference state and remains the UI-facing language-change owner. | `Platform::LanguagePreferencePort` maps typed `Application::LanguagePreference` (`SystemDefault`, `English`, `Korean`) to `LanguageService`. `LanguageController` synchronizes the persisted `ActionRegistry` language without reapplying during action connection and applies valid changes through the port while preserving font refresh, retranslation, and persistence behavior. Generic settings persistence remains open; the nullable legacy `MainWindow` pointer remains an outer compatibility boundary. | Platform language adapter plus UI controller; generic settings persistence and other feature services remain future slices. |
| `documentCatalog()` | [`NavigationController`](../../src/app/controllers/navigation_controller.cpp#L377) resolves its document route from the legacy catalog boundary; MainWindow no longer passes a legacy catalog pointer to Sidebar. | `Platform::ApplicationServicesDocumentCatalogPort` maps legacy metadata, including bounded `DocumentFolderDefinition::parentPath`, into bounded typed `DocumentCatalogProjection` values. `MainWindow::initializeSidebar` and `retranslateUi` request locale-specific projections and pass them by value; failure passes an empty projection. Sidebar maps typed parent paths, folder IDs, keys, and display names while preserving nested hierarchy, order, localized labels, and empty projections. `DocumentCatalogUseCase` supplies the optional `DocumentContentReference`; `DocumentContentResourcePort` resolves referenced resource content and optional export paths from one documents-pack lease. `NavigationController` projects the reference into `PdfViewerDocumentDescriptor` and uses the resource port instead of direct `ResourcePaths::Documents` acquisition/parsing; `PdfViewerPage` integrates `DocumentContentSession`, closes before lease release, and preserves direct no-reference loading. Full document-service migration remains open. | Platform metadata/content-resource adapter plus v2 application use case, typed Sidebar/MainWindow projection boundary, and partial viewer session integration. |

## Outer-adapter responsibilities

The adapter is the only permitted place to combine the legacy Qt facade with
the v2 contracts.

1. **Path conversion.** Accept `QString` from `FileController`, preserve the
   current input/output normalization performed through
   [`DatabaseFileFormat`](../../src/core/database_file_format.cpp), and convert
   at the `WorkspaceLocation` seam. No `QString`, `QFileInfo`, `QDir`, or other
   platform path type crosses into v2 application headers.
2. **Legacy result mapping.** Legacy [`Status`](../../src/core/result.h) is
   `std::expected<void, QString>` and carries text but no error code or
   recoverability. Map success/failure to `Domain::Result`; preserve the
   legacy error text in `OperationError.message`, and define/test the code and
   recoverability classification for each verified failure path. `saveDatabase`
   needs an explicit, tested result source because the legacy method is void.
3. **DataService and service lifetime.** Keep the `ApplicationServices` owner
   alive for every gateway call. Its `unique_ptr`-owned `DataService` and lazy
   feature services must not be exposed through v2 snapshots or retained by a
   v2 contract after the adapter releases the legacy boundary.
4. **Thread/queue handoff.** For import/report work, the outer Qt adapter maps
   worker-thread delivery to the adapter-neutral event sinks in
   [`import_job_coordinator.h`](../../src/next/application/import_job_coordinator.h)
   and [`report_job_coordinator.h`](../../src/next/application/report_job_coordinator.h).
   Workers post bounded, generation-tagged events; the application owner drains
   and pumps them. Workers do not mutate application state or widgets directly.
   The Qt runtime worker/cancellation bridge is committed for the existing
   import/report ports; workers remain behind those ports and the application
   owner remains the only state mutator.
5. **Release boundaries.** The caller owns the copyable `WorkspaceSession`;
   `WorkspaceState` and `SelectionState` own only value snapshots. Successful
   workspace replacement/close clears the selection. The adapter must release
   temporary Qt path values after conversion and must not let v2 contracts keep
   legacy service references, stale sessions, or worker event sources alive
   past their owning operation. The `ApplicationServices` owner releases its
   legacy objects only after all legacy consumers are torn down. The existing
   job coordinators likewise apply events only during `pump()` and reject stale
   generations.

## Forbidden v2 dependencies

The v2 application contracts must remain independent of:

- `QString`, Qt widgets, `MainWindow`, `PageManager`, dialogs, or page pointers;
- `ApplicationServices`, `DataService`, `DatabaseSession`, repositories, raw
  feature-service pointers, or legacy `Status`;
- singleton state or hidden current-workspace state;
- direct worker mutation of `WorkspaceState`, job state, or widgets; and
- Qt signal/thread objects inside the application contracts. Qt queued delivery
  belongs in the outer adapter, terminating at the adapter-neutral worker ports
  and bounded event queues.

These constraints match the Phase 2 [objective and exit gate](03-Phase-2-Domain-Model-and-Application-Contracts.md#exit-gate)
and the explicit adapter-neutral seams in the current v2 headers.

## Staged migration order and reversible gates

| Stage | Change | Acceptance gate and rollback point |
| --- | --- | --- |
| 0. Mapping and bounded document slice | Keep remaining uncutover legacy paths unchanged and record the committed document metadata adapter, bounded folder `parentPath` prerequisite, typed Sidebar/MainWindow projection cutover, content-reference projection, bounded resource/platform resolver, and partial `PdfViewerPage` content-session lifecycle integration. | Configure/source-ownership/dependency checks pass; focused projection/adapter CTest passes 2/2; focused resolver/navigation CTest passes 2/2; Sidebar CTest passes 1/1; adjacent catalog/projection/port tests pass 4/4; the exact nine-target CTest passes 9/9; resource validation covers 6 RCC packs, 7 runtime IDs, and 7 references. Referenced descriptors cover request/load/Ready-or-Error/close-before-release while direct no-reference descriptors preserve legacy loading. The embedded fixture has root folders only, so nested adapter transfer remains a non-blocking runtime-coverage gap; no live MainWindow projection-failure/retranslation integration test exists. |
| 1. Workspace gateway adapter | Completed: `ApplicationServicesWorkspacePort` implements the gateway around `ApplicationServices`; its create/open/close/save/save-as/export mappings retain the separate non-destructive create hook and tested legacy result handling. | Existing app-less coordinator and adapter suites cover lifecycle behavior, path conversion, legacy error text, the void-save postcondition, create mapping, and failure atomicity; this read-only audit did not rerun them. |
| 2. File-controller integration | Completed: FileController routes create/open/close/save/save-as/export through the adapter and coordinator while dialogs, recent files, warnings, and window/action updates remain in the Qt/controller layer. | F42 adds integration verification that failed replacement-open preserves the active database/UI action state and create stops without target preparation after close failure. Interactive new/initial-setup create still closes before later preparation/create; dirty approval still comes from MainWindow. Same-path and full MainWindow snapshot coverage remain non-gating gaps. |
| 3. Worker bridge | Qt worker delivery to the existing import/report sinks and application-owner `pump()` is committed behind the worker ports. | Bounded FIFO, generation isolation, cancellation request/acknowledgement, and terminal release tests passed with no worker/widget mutation. Stress/TSAN and direct report queue-post-failure coverage remain non-blocking gaps. |
| 4. Feature slices | Migrate generic settings persistence, teachers, classes, schedule, calendar, roster, speaking evaluation, and the remaining document ownership/content boundaries as separate typed contracts. Calendar database-query/worker ownership, the dialog edit-draft and constructor/input ownership boundaries, the narrow typed cache/model boundary, the narrow upcoming-events read path, the typed activation-read seam, the non-repeat single-event save and delete seams, the repeat-occurrence save seam, the this-and-following repeat-series edit/save seam, the new-repeat series-create/batch-save seam, and the repeat-series suffix-delete seam are complete; broader typed calendar UI/page migration remains a separate future slice. The accepted preference work includes the theme, language, and schedule-output explicit-theme boundaries; accepted document work includes metadata projection, typed Sidebar/MainWindow catalog ownership, content-reference propagation, the bounded resource/platform resolver, and the partial `PdfViewerPage` session lifecycle. | Each slice has its own owner, adapter, parity tests, and release boundary; no v2 contract exposes a legacy service pointer. The theme, language, and schedule-output explicit-theme boundaries, typed `MainWindow`/`Sidebar` catalog ownership, narrow typed calendar cache/model boundary, narrow upcoming-events read path, typed activation-read seam, non-repeat single-event save and delete seams, repeat-occurrence save seam, this-and-following repeat-series edit/save seam, new-repeat series-create/batch-save seam, repeat-series suffix-delete seam, and dialog edit-draft and constructor/input ownership boundaries are complete; generic settings persistence, full document-service migration, dialog ownership, broader typed calendar UI/page migration, and other unstarted services remain future slices, with legacy accessors retained until their own cutovers. |

The Phase 2 [deliverables](03-Phase-2-Domain-Model-and-Application-Contracts.md#deliverables)
require this mapping, but the Phase 2 exit gate is not met by documentation
alone: the accepted document slice is bounded, and full document-service
migration plus the remaining feature slices remain work.

## Verified resolver-slice handoff

Against baseline commit `e031317c`, configure/source-ownership/dependency
checks passed at 705 sources. Focused resolver/navigation CTest passed 2/2;
the exact nine-target CTest passed 9/9; resource validation passed for 6 RCC
packs, 7 runtime IDs, and 7 references; and `git diff --check` passed with
LF-to-CRLF warnings only. An initial MSBuild FileTracker `E_ACCESSDENIED`
required an elevated retry; the focused build/link passed. Invalid UTF-8 and
live UI integration lack direct coverage. Phase 2 remains in progress.

## Verified document-folder prerequisite handoff

After baseline commit `fd695fd`, configure/ownership/dependency checks passed
at 705 handwritten files; focused projection/adapter CTest passed 2/2;
Qt-free application and standalone syntax checks passed; and `git diff
--check` passed. `DocumentFolderMetadata` now carries bounded `parentPath`
(empty for roots), projection validation handles it, and
`ApplicationServicesDocumentCatalogPort` copies legacy
`DocumentFolderDefinition::parentPath`. The embedded fixture has root folders
only, so nested adapter transfer lacks runtime coverage; nested projection
behavior is covered. The follow-on typed `Sidebar`/`MainWindow` cutover is
recorded below; other feature migration remains open. Phase 2 remains in
progress.

## Verified Sidebar/MainWindow typed catalog cutover

After baseline commit `662e5f2`, `Sidebar` owns a copied or move-assigned
`Application::DocumentCatalogProjection`; it has no legacy `DocumentCatalog`
pointer, include, or dependency. Its tree maps typed `parentPath`, folder IDs,
keys, and display names while preserving nested hierarchy, order, localized
labels, and empty-projection behavior.

`MainWindow::initializeSidebar` and `MainWindow::retranslateUi` request
locale-specific projections through
`Platform::ApplicationServicesDocumentCatalogPort` and pass them to Sidebar by
value. Projection failure supplies an empty projection.

Configure/ownership/dependency checks passed at 705 handwritten sources;
Sidebar CTest passed 1/1; adjacent catalog/projection/port tests passed 4/4;
the exact nine-target regression passed 9/9; and resource validation covered 6
RCC packs, 7 runtime IDs, and 7 references. Application Qt-free and projection
standalone syntax checks passed. An elevated MSBuild retry was required after
`E_ACCESSDENIED`; the retry passed, and `git diff --check` passed.

No live MainWindow projection-failure/retranslation integration test exists;
static and production-compilation coverage is present. This is a non-blocking
gap. Phase 2 remains open for the remaining feature-service migrations and is
not complete.

## Verified theme preference bridge handoff

After baseline commit `9f0d86d`, `Platform::ThemePreferencePort` explicitly
maps typed `Application::ThemePreference` (`SystemDefault`, `Light`, `Dark`)
to the legacy `ThemeService`. `ThemeController` owns typed
`UserPreferencesState`, synchronizes the persisted `ActionRegistry` theme
without reapplying it at connection, and applies valid changes through the
port. Invalid-input and state transitions remain atomic; valid changes retain
persistence, icon refresh, and live palette behavior. `MainWindow` passes an
explicit `ThemeService` reference. `schedule_output_controller.cpp` keeps its
legacy theme read and remains open for a later slice.

Configure/ownership/dependency checks passed at 706 sources; focused
`StartupVisualSettings` passed 1/1; next preferences/launch checks passed 2/2;
the exact nine-target CTest passed 9/9; and resource validation covered 6 RCC
packs, 7 runtime IDs, and 7 references. Qt-free application checks passed.
`git diff --check` passed with CRLF warnings, and the Ninja/MSVC fallback build
passed after the environment/FileTracker issue. No dedicated icon-pixel
assertion exists; this is a non-blocking gap. Phase 2 remains in progress.

## Verified language preference bridge handoff

After baseline commit `3ad3ef1`, `Platform::LanguagePreferencePort` explicitly
maps typed `Application::LanguagePreference` (`SystemDefault`, `English`, or
`Korean`) to the legacy `LanguageService`. `LanguageController` owns typed
`UserPreferencesState`, synchronizes the persisted `ActionRegistry` language
without reapplying it during action connection, and applies valid changes
through the port while preserving font refresh, retranslation, and persistence
behavior. `MainWindow` now passes an explicit `LanguageService` reference.
Generic settings persistence and other feature-service migrations remain open.

Configure/ownership/dependency checks passed at 708 sources; focused
language/controller CTest passed 3/3; the exact nine-target CTest passed 9/9;
`LanguageService`/startup visual tests passed; and resource validation covered
6 RCC packs, 7 runtime IDs, and 7 references. Qt-free/raw-pointer checks and
`git diff --check` passed, and an elevated FileTracker retry passed. No live
`MainWindow::retranslateUi` assertion exists, failure rollback is not
deterministically exercised, and the nullable legacy `MainWindow`
`LanguageService` pointer has no null-construction coverage. Phase 2 remains
in progress.

## Verified schedule output explicit-theme handoff

Against baseline commit `7f185ca`, `ScheduleOutputController` now receives
the resolved `Theme` explicitly and no longer includes or accesses
`ThemeService` or `currentTheme`. `ScheduleWidget` resolves the current theme
at the caller boundary and preserves the legacy Dark fallback when no theme
service is available. Existing settings-service username behavior,
print/save action selection, style/orientation selection, and show-English-
names behavior remain intact.

Focused `ScheduleWidget` and `SchedulePrintPdf` CTest passed 2/2. Tests cover
widget theme propagation/fallback and PDF `CurrentAppearance` Light/Dark
behavior while preserving explicit Light/Dark/Excel styles. The exact
nine-target regression passed 9/9; resource validation covered 6 RCC packs,
7 runtime IDs, and 7 references; `git diff --check` passed with CRLF warnings
only; and static controller review passed. Fresh configure could not find a
compiler in the verifier shell, but existing configured VS Debug artifacts
were current and passed. The schedule output direct-theme accessor is closed;
generic settings/application-services seams and other feature migrations
remain open. Phase 2 remains in progress and is not complete.

## Verified calendar read-projection adapter handoff

Against baseline commit `08b86215`,
`src/next/platform/application_services_calendar_event_port.h` maps
`CalendarService::eventsInRange` into an owned typed
`Application::CalendarEventProjection`. The adapter copies bounded metadata
and typed IDs, validates ordered valid ranges, unavailable service, technical
failures, invalid IDs/metadata, partial timed ranges, and projection capacity,
and keeps all-day and unknown-time policy explicit. No legacy pointers escape;
the Application layer remains Qt-free. The malformed partial-time fixture is
inserted directly with `QSqlQuery` at the persistence boundary because
`CalendarService::saveEvent` correctly rejects malformed input.

The production header is registered in `cmake/next.cmake` and the focused test
in `cmake/tests/next.cmake`. Focused adapter CTest passed 1/1; existing
`ClassMngrCalendarEventCacheTests` passed 1/1; the workspace control test
passed 1/1; and the exact existing nine-target regression passed 9/9 in
58.92s. Configure/ownership/dependency checks passed at 710 sources;
resource validation passed for 6 RCC packs, 7 runtime IDs, and 7 references;
strict UTF-8/encoding review passed; and `git diff --check` passed with only
LF-to-CRLF warnings. The focused build passed after an environmental
FileTracker `E_ACCESSDENIED` retry.

The calendar read-projection boundary is complete. The narrow typed cache/model
cutover is recorded below; broader typed calendar UI/page migration remains
future. The later worker-boundary handoff separates database-query and worker
ownership while leaving legacy compatibility conversions available. The
metadata enrichment handoff is recorded below. Generic settings and other
feature migrations remain open; Phase 2 remains in progress and is not
complete.

## Verified calendar-event projection enrichment handoff

Against baseline commit `695d1065`, `CalendarEventSummary` now owns bounded
`eventType` and `timeStatus` strings plus an optional bounded `repeatSeriesId`.
The projection remains Qt-free, copyable, bounded, typed-ID based, ordered,
and pointer-free. Existing all-day/unknown-time, order, ID, capacity, and
legacy mapped-field byte semantics remain intact.

`ApplicationServicesCalendarEventPort` maps and validates those fields,
trims only `repeatSeriesId` for normalization, preserves surrounding
whitespace for existing title/date/time/etc. fields, and returns structured
failures for blank, over-bounds, or malformed fields. Tests extend application
and adapter coverage for bounds, validation, copy/equality/lookups, legacy
value/repeat-series preservation, malformed persisted input, and title
whitespace compatibility. CMake registrations remain unchanged; the narrow
typed cache/model cutover is recorded below, while broader typed calendar
UI/page migration remains future.

The elevated VS Debug build passed after an environmental FileTracker
`UnauthorizedAccessException` retry. Focused application, adapter,
calendar-cache, and workspace-control tests all passed 1/1; the exact
nine-target regression passed 9/9; configure/ownership/dependency passed with
710 handwritten sources and one explicit owner; resource validation passed 6
RCC packs, 7 runtime IDs, and 7 references; `git diff --check` passed with
LF/CRLF warnings only; and static Qt-free/pointer review passed.

Phase 2 remains open. The typed projection now feeds the completed narrow
cache/model boundary, and the later worker-boundary handoff separates calendar
database-query/worker ownership while legacy compatibility conversions remain
for unchanged callers. Broader typed calendar UI/page migration, generic
settings, and other migrations remain open.

## Verified typed calendar query-port handoff

Against baseline commit `0859cd82`, added
`src/next/application/calendar_event_query_port.h`, a Qt-free, value-only
application port for copied range/next-event requests and typed projection,
date, and error results. Adapted
`src/features/calendar/calendar_event_projection_query.h/.cpp` and
`CalendarEventCache` h/cpp to use a clear per-worker port/factory lifetime
while retaining unique SQLite ownership in the worker adapter and the existing
cache scheduling/public API.
Added `tests/next_application_calendar_event_query_port_tests.cpp` and
extended `tests/calendar_event_cache_tests.cpp`; the new application/test
sources are registered in `cmake/next.cmake` and `cmake/tests/next.cmake`,
while production-source and `data_and_imports` registrations remain unchanged.
UI/model/page call sites and unrelated modules remain untouched.

No Qt, `QObject`, service, repository, SQLite/QSql object, or legacy pointer
crosses the application port. Query parity remains covered for repeat-series,
multi-day membership, dedupe, retention, ordering/filtering, next-event
lookup, cancellation-by-invalidation, generation/stale-result rejection, and
errors. Legacy `eventsForDate`/`eventsInRange` compatibility conversions
remain for unchanged callers; the narrow typed cache/model boundary is
recorded below.

The elevated VS Debug query-port/cache build passed after an environmental
FileTracker `E_ACCESSDENIED` retry. Focused query-port/cache tests passed 2/2;
relevant calendar tests passed 4/4; the exact nine-target regression passed
9/9 in 58.91s; configure/ownership/dependency passed with 714 handwritten
sources and one owner each; resource validation passed 6 RCC packs, 7 runtime
IDs, and 7 references; `git diff --check` passed with LF/CRLF warnings only;
and static Qt-free/worker review passed. Failure-path SQLite cleanup is
statically verified but not runtime fault-injected.

Phase 2 remains open. The typed worker query-port and narrow cache/model
boundaries are complete while legacy page/upcoming compatibility callers
remain; broader typed calendar UI/page migration, generic settings, and other
feature migrations remain open.

## Verified typed calendar cache/model cutover handoff

Against baseline commit `0b5b8eb6`, `CalendarEventCache` retains typed
`CalendarEventSummary` values and exposes the date-scoped
`eventProjectionForDate` accessor. `eventsForDate` and `eventsInRange` remain
legacy compatibility conversions. `CalendarEventModel` consumes the typed
projection and `CalendarEventSummary` for QML rows, converting dates, times,
and `QVariant` only at the UI boundary. Calendar pages and upcoming-event
callers remain unchanged.

Campus filtering adds the typed-summary path while retaining the necessary
legacy `CalendarEvent` compatibility wrapper for unchanged upcoming-page and
import-test callers; filtering semantics remain preserved. Tests cover typed
projection/model parity, ordering/filtering, all-day and unknown-time cases,
repeat metadata, legacy compatibility, generation/stale-result behavior, and
relevant calendar paths. No CMake changes were made.

The elevated VS Debug build passed after the `constFind` fix. Focused and
relevant tests passed 7/7; the exact nine-target regression passed 9/9 in
57.83s, including startup performance; configure/ownership/dependency checks
passed with 714 handwritten sources; resource validation passed for 6 RCC
packs, 7 runtime IDs, and 7 references; `git diff --check` passed with CRLF
warnings only; and static review passed for typed model/filter usage. The
non-blocking warnings are missing Vulkan headers and existing MSBuild
custom-build dependency warnings.

This closes the narrow typed cache/model boundary. Phase 2 remains open:
legacy page/upcoming callers remain, while broader typed calendar UI/page
migration, generic settings, and other feature migrations remain future work.

## Verified typed upcoming-events read cutover handoff

Against baseline commit `88bd88dd`, `CalendarEventCache` adds a range-scoped
typed projection accessor while `eventsInRange` and `ensureNextTenEvents`
compatibility remain intact. `calendar_page_upcoming_events.cpp` migrates only
upcoming-event retrieval, filtering, date-time formatting, and row rendering
to `CalendarEventSummary`. `calendar_page_events.cpp`, edit dialogs, service
calls outside this path, and integer-ID activation remain unchanged.

Scope loading, retention, generation invalidation/stale cancellation,
dedupe, ordering, active-type/campus/start-of-term filtering, the ten-event
limit, display text, row IDs, edit navigation, all-day/unknown-time handling,
repeat metadata, and legacy parity remain preserved. The projection boundary
uses `events()`, `pop_back()`, and `empty()` correctly. Tests cover range
projection ordering/filtering inputs, metadata, typed/legacy parity, and the
relevant calendar/page paths; no CMake changes were made.

The elevated current-source VS Debug build passed; focused calendar tests
passed 3/3 and page tests 2/2; the exact nine-target regression passed 9/9 in
58.77s; configure/ownership/dependency passed with 714 sources; resource
validation passed 6 RCC packs, 7 IDs, and 7 references; `git diff --check`
passed with CRLF warnings only; and static review passed. No dedicated live
upcoming-page UI test exists.

This closes the narrow upcoming-events typed read path. Phase 2 remains open:
`calendar_page_events.cpp`, edit dialogs, other legacy callers, and broader
typed page migration remain future work, as do generic settings and other
migrations.

## Verified final next-ten-events read cutover handoff

Against baseline commit `4de5c8c2`, the only implementation change was
`src/features/calendar/ui/calendar_page_events.cpp`.
`CalendarPage::ensureNextTenEvents` now uses
`eventProjectionInRange(...).events()` with the typed
`filterUpcomingEvents` overload; no `eventsInRange()` remains in that file.
`requestNextEventMonth`, loading guards, retention/invalidation, the ten-event
prefetch threshold, and surrounding page behavior remain unchanged. Legacy
`eventsInRange` APIs remain for other callers; edit/activation remains
integer-ID legacy.

The cutover changed no tests, CMake, or documentation; existing cache/page
coverage is reused. Elevated current-source VS Debug builds passed; focused
calendar/page tests passed 5/5; the exact nine-target regression passed 9/9 in
58.63s; configure/ownership/dependency passed for 714 sources; resource
validation passed 6 RCC packs, 7 IDs, and 7 references; `git diff --check`
passed with CRLF warnings only; static review passed; and exactly one file was
dirty. No dedicated live upcoming-row UI test exists; this is non-blocking.

The typed read-only calendar paths are now covered through upcoming retrieval,
next-ten prefetch, and the activation-read seam. Dialog/mutation/repeat
operations and other legacy callers remain future work, as do generic
settings and other migrations. Phase 2 remains open.

## Verified typed calendar activation-read handoff

Against baseline commit `a7498732`,
`src/next/platform/application_services_calendar_event_port.h` now exposes
typed `projectionById(int)`. It preserves ID, title, event type, status,
repeat-series, all-day, unknown-time, date, and time fields, with structured
unavailable, missing, invalid, partial-time, overflow, and malformed-repeat
failures. `calendar_page_events.cpp` reads activation data through the adapter
and converts the typed result to legacy `CalendarEvent` only at the existing
UI boundary before opening the unchanged dialog.

`tests/next_platform_application_services_calendar_event_port_tests.cpp`
covers valid by-ID field preservation, missing/unavailable/malformed cases,
and boundary checks. No CMake changes were made. CalendarEventDialog,
save/delete/repeat mutation operations, schedule settings, integer-ID
semantics, and other callers remain unchanged; invalid reads cause no
mutation.

The elevated current-source VS Debug build passed; focused tests passed
11/11; the exact nine-target regression passed 9/9 (57.90s startup, 60.68s
total); configure/ownership passed with 714 sources; dependency assertions
passed (9 production targets, `ClassMngrNext -> Qt6::Core`); resource
validation passed 6 RCC packs and 7 IDs/references; diff/static checks
passed; and exactly three scoped files were dirty. Non-blocking warnings were
missing Vulkan headers and existing MSVC `/FORCE`/duplicate-stub linker
warnings.

The typed activation-read seam is closed. Dialog/mutation/repeat operations,
other legacy callers, generic settings, and broader migrations remain future
work; Phase 2 remains open.

## Verified typed single-event delete handoff

Against baseline commit `0fe9ca9d`, added
`src/next/application/calendar_event_delete_port.h`, a Qt-free typed
`Result<void>` contract for `CalendarEventId`, and
`src/next/platform/application_services_calendar_event_delete_port.h`, the
legacy `CalendarService::deleteEvent` adapter. The platform header is
registered in `cmake/next.cmake`; the existing platform-port test target is
reused.

Only the non-repeat delete branch in `calendar_page_events.cpp` changed: it
converts the event ID at the UI boundary and calls the typed port. Save,
repeat-series deletion, `CalendarEventDialog`, schedule settings,
integer-ID semantics, warnings/errors, close/return behavior, and generation
invalidation remain unchanged. The platform-port tests now cover valid delete,
invalid ID, unavailable service, and injected failure; fixtures persist valid
09:00–10:00 events.

The elevated current-source Debug build passed; focused tests passed 11/11;
the exact nine-target regression passed 9/9 (63.22s; startup 60.38s);
configure/ownership passed with 716 sources; dependency assertions passed
(9 production targets, `ClassMngrNext -> Qt6::Core`); resource checks passed
6 RCC packs, 7 IDs, and 7 references; and diff/cached-diff/static checks
passed. The static checker recognized the `CalendarEventDeleteResult` alias;
no source issue was found.

The non-repeat single-event delete and repeat-series suffix-delete seams are
closed. Repeat-series edit/save, dialog ownership, generic settings, and other
migrations remain future work; Phase 2 remains open.

## Verified typed non-repeat calendar-event save handoff

Against baseline commit `e197891f`, added
`src/next/application/calendar_event_save_port.h`, a Qt-free typed
`CalendarEventSaveRequest` returning `Result<CalendarEventId>`, and
`src/next/platform/application_services_calendar_event_save_port.h`, which
maps through `CalendarService::saveEvents({event})`. The platform header is
registered in `cmake/next.cmake`; the existing platform-port test target is
reused.

Only the non-repeat save branch in `calendar_page_events.cpp` changed. The
repeat edit/save path and dialog behavior remain on their existing legacy
path; single-event delete, repeat-series suffix deletion, schedule behavior,
and other branches remain unchanged.

The executor and independent tester both reported a passing Debug build,
focused port tests 11/11, calendar tests 7/7, the exact nine-target regression
9/9, and launch 1/1. Configure/ownership passed with 718 sources; dependency
checks passed for 9 production targets (`ClassMngrNext -> Qt6::Core`);
resource checks passed 6 RCC packs, 7 runtime IDs, and 7 runtime references;
diff, cached-diff, trailing-whitespace, and static checks passed. Existing
Vulkan, Qt-zlib, and MSVC notices are non-blocking.

The typed non-repeat save seam is closed; this-and-following repeat-series
edit/save is recorded below. Dialog ownership, generic settings, remaining
repeat paths, and broader page, document, and feature migrations remain future
work; Phase 2 remains open.

## Verified typed this-and-following repeat-series edit/save handoff

Against baseline commit `03c1fecc`, added
`src/next/application/calendar_event_series_edit_port.h`, a Qt-free
`CalendarEventSeriesEditRequest` with `CalendarEventSeriesEditResult` as
`Result<void>`, and
`src/next/platform/application_services_calendar_event_series_edit_port.h`.
The platform adapter loads the suffix through
`repeatSeriesFromDate(repeatSeriesId, startDate)`, computes the start-date
offset and edited duration, propagates the edited fields to each selected
occurrence, and persists the updated list through `saveEvents(updatedEvents)`.

Only the `thisAndFollowing` branch in `calendar_page_events.cpp` migrated to
the typed port. Recurrence selection, dialog behavior, single-event delete,
non-repeat save, and other calendar paths remain preserved; the adapter keeps
the legacy recurrence/persistence boundary outside the Qt-free request.

The executor and independent tester reported a passing Debug build, focused
port tests 11/11, calendar tests 7/7, the exact nine-target regression 9/9
including startup verification, and launch 1/1. Configure/ownership passed
with 722 sources; dependency checks passed for 9 production targets
(`ClassMngrNext -> Qt6::Core`); resource checks passed 6 RCC packs, 7 runtime
IDs, and 7 runtime references; diff, cached-diff, trailing-whitespace, and
static checks passed. Existing Vulkan, Qt-zlib, and MSVC notices are
non-blocking.

The typed this-and-following repeat-series edit/save seam is closed. Dialog
ownership, generic settings, remaining repeat paths, and broader page,
document, and feature migrations remain future work; Phase 2 remains open.

## Verified typed this-event-only repeat-occurrence save handoff

Against baseline commit `963d4578`, only the
`repeatSeriesEvent && !thisAndFollowing` branch in
`calendar_page_events.cpp` now routes through the existing typed
`CalendarEventSavePort`. It clears `repeatSeriesId` before mapping the
occurrence ID and edited fields into the existing save request. The existing
warning and early-return behavior remains intact: failures return before
invalidation, while success preserves the existing refresh. The
this-and-following, new-repeat, delete, dialog, non-repeat, and other paths
remain unchanged.

The executor and independent tester reported a passing Debug build, focused
tests 11/11, the exact nine-target regression 9/9 including startup
verification, and launch 1/1. Configure/ownership passed with 722 sources;
dependency checks passed for 9 production targets (`ClassMngrNext ->
Qt6::Core`); resource checks passed 6 RCC packs, 7 runtime IDs, and 7 runtime
references; static, diff, cached-diff, and trailing-whitespace checks passed.
Existing Vulkan, Qt-zlib, and MSVC notices are non-blocking.

The typed this-event-only repeat-occurrence save seam is closed. Dialog
ownership, generic settings, remaining repeat paths, and broader page,
document, and feature migrations remain future work; Phase 2 remains open.

## Verified typed new-repeat series creation/batch save handoff

Against baseline commit `f94f78fa`, added
`src/next/application/calendar_event_series_create_port.h`, a Qt-free
`CalendarEventSeriesCreateRequest`/port over typed occurrence save requests,
returning typed event IDs, and
`src/next/platform/application_services_calendar_event_series_create_port.h`.
The platform adapter copies the bounded series ID and occurrence fields into
one ordered legacy batch and makes one atomic `saveEvents(events)` call; the
typed result returns one `CalendarEventId` per saved occurrence.

Only the `dialog.repeatEnabled()` branch in `calendar_page_events.cpp` now
uses the series-create port. Existing `repeatedCalendarEvents` generation and
all typed edit/save/delete/dialog paths remain preserved. Daily, weekly, and
monthly recurrence parity is covered, and injected batch failure rolls back
without partial rows.

The executor and independent tester reported a passing Debug build, focused
tests 12/12, the exact nine-target regression 9/9 including startup
verification, and launch 1/1. Configure/ownership passed with 724 sources;
dependency checks passed for 9 production targets (`ClassMngrNext ->
Qt6::Core`); resource checks passed 6 RCC packs, 7 runtime IDs, and 7 runtime
references; static, diff, cached-diff, and trailing-whitespace checks passed.
Existing Vulkan, Qt-zlib, and MSVC notices are non-blocking.

The typed new-repeat series-create/batch-save seam is closed. Dialog
ownership, generic settings, and broader page, document, and feature
migrations remain future work; Phase 2 remains open.

## Verified typed calendar-dialog edit-draft handoff

Against baseline commit `2e2cc65c`, added the Qt-free bounded
`CalendarEventEditDraft`. `CalendarEventDialog::eventData()` now returns the
draft, while the legacy `CalendarEvent` conversion is private to the dialog as
`legacyEventData()`. `calendar_page_events.cpp` consumes the draft for all
existing typed save, series-create, and series-edit requests; no legacy Qt
value conversion is required at those application request boundaries.

Dialog defaults, validation, inline errors, warnings, repeat controls, delete
and mutation behavior, and page invalidation/refresh behavior remain
preserved. The draft boundary is additive; full dialog ownership migration
remains open.

The executor and independent tester reported a passing Debug build, focused
tests 12/12, the exact nine-target regression 9/9 including startup
verification, and launch 1/1. Configure/ownership passed with 725 sources;
dependency checks passed for 9 production targets (`ClassMngrNext ->
Qt6::Core`); resource checks passed 6 RCC packs, 7 runtime IDs, and 7 runtime
references; static, diff, cached-diff, and trailing-whitespace checks passed.
A non-blocking PTY/CreateProcess retry was required; existing Vulkan, Qt-zlib,
and MSVC notices are also non-blocking.

The typed calendar-dialog edit-draft boundary is closed. Dialog ownership,
generic settings, and broader page, document, and feature migrations remain
future work; Phase 2 remains open.

## Verified typed calendar-dialog constructor/input ownership handoff

Against baseline commit `d687bf69`, `CalendarEventDialog` now accepts and
stores the existing Qt-free `CalendarEventEditDraft` by value. The page maps
legacy activation and new-event values into a draft before construction;
legacy conversion helpers remain private to the dialog implementation, and
public `eventData()` continues to return the draft.

Defaults, validation, inline errors, warnings, delete and repeat controls,
`schedule_use_24h`, routing, and invalidation/refresh behavior remain
preserved. The typed draft continues through the existing save, series-create,
and series-edit paths without expanding legacy ownership. Verification covered
the four-file scope: `calendar_event_dialog.cpp`,
`calendar_event_dialog.h`, `calendar_page_events.cpp`, and
`tests/dialog_shell_tests.cpp`.

The executor and independent tester reported a passing Debug build, focused
tests 12/12, the exact nine-target regression 9/9 including startup
verification, and launch 1/1. Configure/ownership passed with 725 sources;
dependency checks passed for 9 production targets (`ClassMngrNext ->
Qt6::Core`); resource checks passed 6 RCC packs, 7 runtime IDs, and 7 runtime
references; static and diff checks passed. A static-checker correction was
required and then passed; existing Vulkan, Qt-zlib, and MSVC notices remain
non-blocking.

The typed calendar-dialog constructor/input ownership seam is closed. Full
dialog ownership, generic settings, and broader page, document, and feature
migrations remain future work; Phase 2 remains open.

## Verified typed read-only schedule_use_24h settings bridge handoff

Against baseline commit `2d785250`, added the Qt-free
`ScheduleDisplayPreferences` value and read-only
`ScheduleDisplayPreferencesPort`, plus the Qt-boundary
`ApplicationServicesScheduleDisplayPreferencesPort`. The adapter preserves
the legacy `SettingsService` `schedule_use_24h` true/false behavior, including
the missing or unavailable-settings false fallback and legacy `QVariant`
coercion. Only the calendar and upcoming-event callers now use the bridge;
there are no writes or other settings changes, and date/time formatting is
unchanged.

The executor and independent tester reported a passing Debug build, focused
tests 14/14, the exact nine-target regression 9/9 including startup
verification, and launch 1/1. Configure/ownership passed with 728 sources;
dependency checks passed for 9 production targets (`ClassMngrNext ->
Qt6::Core`); resource checks passed 6 RCC packs, 7 runtime IDs, and 7 runtime
references; direct-read, read-only, static, and diff checks passed.
`clang-format` and `clang-tidy` were unavailable; normal Vulkan, Qt-zlib, and
MSVC warnings remain non-blocking.

The typed read-only schedule display-preferences seam is closed. Residual
`SettingsService` callers, generic settings persistence, and broader page,
document, and feature migrations remain future work; Phase 2 remains open.

## Verified typed five-key schedule-preferences persistence handoff

Against baseline commit `2ebac724`, extended the existing Qt-free schedule
display-preferences contract and adapter to all five booleans:
`use24HourTime`, `showEnglishNames`, `showWeekends`,
`showAllIntensiveHours`, and `testingAffectsM1`. The port now exposes an
atomic typed save returning `Result<void>`; the adapter maps one bundle to the
legacy settings store and preserves rollback on failure.

Only `src/app/menu_builder.cpp` and
`src/features/schedule/ui/schedule_widget.cpp` use the five-key persistence
boundary. The legacy `ScheduleSettingsPreferences` compatibility files and
other callers remain intact; menu/widget rendering and controls, the existing
calendar `use24h` read, and all formatting behavior are preserved.

Serial full and focused builds passed. A transient parallel MSVC `LNK1163`
was non-blocking and cleared on the serial rerun. Focused tests passed 10/10;
the exact nine-target regression passed 9/9; launch passed 1/1.
Configure/ownership passed with 728 sources; dependency checks passed for 9
production targets (`ClassMngrNext -> Qt6::Core`); resource checks passed 6
RCC packs, 7 runtime IDs, and 7 runtime references. Five-key, atomic-rollback,
call-site, Qt-free, and diff checks passed. `clang-format` and `clang-tidy`
were unavailable; normal Vulkan, Qt-zlib, and MSVC warnings remain
non-blocking.

The typed five-key schedule-preferences persistence seam is closed. Residual
settings callers, generic settings persistence, and broader page, document,
and feature migrations remain future work; Phase 2 remains open.

## Verified typed excelImportTimeoutSeconds handoff

Against baseline commit `b4fca5b1`, added the Qt-free bounded
`ExcelImportTimeoutPreferences` value and read/write
`ExcelImportTimeoutPreferencesPort`, with the
`SettingsManagerExcelImportTimeoutPort` adapter. The policy preserves default
120 seconds, supports only 30/60/120/300, normalizes invalid values to 120,
and round-trips the legacy `imports/excelTimeoutSeconds` key. Shared timeout
policy was narrowly centralized in `user_preferences_state.h` while the
existing Excel-specific and import-timeout APIs remain available.

Only `src/app/menu_builder.cpp`,
`src/features/teacher/ui/teacher_import_dialog.cpp`, and
`src/features/schedule/ui/schedule_import_dialog.cpp` use the typed boundary.
No worker or dialog workflow changed; existing import behavior and other
settings callers remain intact.

The executor and independent tester reported a passing Debug build, focused
tests 8/8, the exact nine-target regression 9/9, and a passing launch check.
Configure/ownership passed with 731 sources; dependency checks passed for 9
production targets (`ClassMngrNext -> Qt6::Core`); resource checks passed 6
RCC packs, 7 runtime IDs, and 7 runtime references. Caller-scope, Qt-free,
normalization/round-trip, and diff checks passed. Normal warnings remained
non-blocking; `clang-format` and `clang-tidy` were unavailable.

The typed Excel import-timeout seam is closed. Residual settings callers,
generic settings persistence, and broader page, document, and feature
migrations remain future work; Phase 2 remains open.

## Verified typed sidebar display preferences handoff

Against baseline commit `ea10176b`, added the Qt-free paired
`SidebarDisplayPreferences` value and `SidebarDisplayPreferencesPort`, plus
the `SettingsManagerSidebarDisplayPreferencesPort` adapter. It maps the exact
legacy keys `options/sidebarTooltipsEnabled` and
`options/sidebarMarqueeEnabled`, preserves enabled defaults for missing or
invalid values, retains legacy `QVariant` boolean coercion, and round-trips
both values. The legacy writes are void, so persistence failure is not
observable and save failure is not applicable at this boundary.

Only `src/ui/shared/actions/action_registry.cpp` is cut over. Checked states,
toggle persistence, unrelated actions, and existing sidebar rendering/control
behavior remain preserved; other settings callers are unchanged.

The executor and independent tester reported a passing Debug build, focused
tests 5/5, the exact nine-target regression 9/9, and launch 1/1.
Configure/ownership passed with 734 sources; dependency checks passed for 9
production targets (`ClassMngrNext -> Qt6::Core`); resource checks passed 6
RCC packs, 7 runtime IDs, and 7 runtime references. Call-site, Qt-free,
static, and diff checks passed. `clang-format` and `clang-tidy` were
unavailable; normal warnings remain non-blocking.

The typed sidebar display-preferences seam is closed. Residual settings
callers, generic settings persistence, and broader page, document, and feature
migrations remain future work; Phase 2 remains open.

## Verified typed PowerPoint data-access notice handoff

Against baseline commit `f462a162`, added the Qt-free
`PowerPointDataAccessNoticePreferences` value and
`PowerPointDataAccessNoticePreferencesPort`, plus the
`SettingsManagerPowerPointDataAccessNoticePort` adapter. It preserves the
exact `options/showPowerPointDataAccessNotice` key, an enabled default for
missing or unavailable values, legacy `QVariant` boolean coercion, and
round-trip behavior. The legacy write is void, so persistence failure is not
observable and save failure is not applicable at this boundary.

Only `src/ui/shared/actions/action_registry.cpp` and
`src/features/speaking_eval/ui/speaking_eval_batch_export_dialog.cpp` use the
typed preference. The `Q_OS_MACOS` confirmation/bypass before PowerPoint
export remains preserved, as does export behavior and non-Apple behavior.

The executor and independent tester reported a passing Debug build, focused
tests 9/9, the exact nine-target regression 9/9, and a passing launch check.
Configure/ownership passed with 740 sources; dependency checks passed for 9
production targets (`ClassMngrNext -> Qt6::Core`); resource checks passed 6
RCC packs, 7 runtime IDs, and 7 runtime references. Qt-free, call-site,
static, and diff checks passed. The Apple-only runtime test was unavailable on
Windows; non-Apple guards passed. Normal warnings remained non-blocking;
`clang-format` and `clang-tidy` were unavailable.

The typed PowerPoint data-access notice seam is closed. Residual settings
callers, generic settings persistence, and broader page, document, and feature
migrations remain future work; Phase 2 remains open.

## Verified typed skipped-update-version persistence handoff

Against baseline commit `7fdc315f`, added the Qt-free optional
`SkippedUpdateVersionPreferences` value and typed read/write/clear
`SkippedUpdateVersionPreferencesPort`, plus the
`SettingsManagerSkippedUpdateVersionPort` adapter. It maps the exact
`updates/skippedVersion` key, represents missing or empty storage as
`std::nullopt`, trims values on read and write, round-trips non-empty values,
and clears the setting for an empty optional.

Only `src/app/controllers/update_controller.cpp` uses the typed boundary.
`Version::parse` still validates and normalizes skipped versions before write;
skip/unskip, reconciliation clearing, prompt suppression, dialog display, and
the `QString` conversion boundary remain preserved.

The executor and independent tester reported a passing Debug build after a
transient MSVC `LNK1163` retry, focused tests 5/5, the exact nine-target
regression 9/9, and a passing launch check. Configure/ownership passed with
743 sources; dependency checks passed for 9 production targets
(`ClassMngrNext -> Qt6::Core`); resource checks passed 6 RCC packs, 7 runtime
IDs, and 7 runtime references. Qt-free, call-site, static, and diff checks
passed. Normal warnings remained non-blocking; `clang-format` and
`clang-tidy` were unavailable.

The typed skipped-update-version seam is closed. Residual settings callers,
generic settings persistence, and broader page, document, and feature
migrations remain future work; Phase 2 remains open.

## Verified typed recent-workspace history handoff

Against baseline commit `3fc8ec4a`, added the Qt-free
`RecentWorkspaceHistory` value and `RecentWorkspaceHistoryPort`, plus the
`SettingsManagerRecentWorkspaceHistoryPort` adapter for the recent-file list
and last-file value only. UTF-8/Unicode and path normalization are preserved;
raw and normalized paths deduplicate, entries remain newest-first with a
ten-entry cap, and prune/clear behavior is retained. Missing or unavailable
storage remains empty, while `mostRecentDatabasePath()` prefers the list and
falls back to the last file.

`FileController` now routes recent/last-file reads and writes through the
typed boundary, preserving recent-menu and startup behavior. Last-directory
persistence and the workspace lifecycle remain unchanged; no direct raw
recent/last-file access remains in `FileController`.

The executor and independent tester reported focused tests 7/7 including
launch/startup verification, and the exact nine-target regression 9/9.
Configure/ownership passed with 746 sources; dependency checks passed for 9
production targets (`ClassMngrNext -> Qt6::Core`); resource checks passed 6
RCC packs, 7 runtime IDs, and 7 runtime references. Qt-free, direct-call,
static, and diff checks passed. Normal warnings remained non-blocking;
`clang-format` and `clang-tidy` were unavailable.

The typed recent-workspace history seam is closed. Residual settings callers,
generic settings persistence, and broader page, document, and feature
migrations remain future work; Phase 2 remains open.

## Verified typed evaluation-default-policy handoff

Against baseline commit `9fe09c65`, added the Qt-free
`EvaluationDefaultPolicy` contract with only `All` and
`CurrentOrPreviousTerm`, plus the
`ApplicationServicesEvaluationDefaultPolicyPort` adapter. It preserves the
exact `classes_navigation_evaluation_default_policy` key, stores the two
legacy values, and falls back to `All` for missing, invalid, or unavailable
settings.

Only `src/app/menu_builder.cpp` and
`src/features/classes/evaluation_default_selection_service.cpp` use the typed
boundary. Menu persistence and radio selection behavior remain preserved; the
other four class-navigation preference keys are untouched.

The executor and independent tester reported a passing Debug build, focused
tests 8/8, the exact nine-target regression 9/9, and a passing launch check.
Configure/ownership passed with 749 sources; dependency checks passed for 9
production targets (`ClassMngrNext -> Qt6::Core`); resource checks passed 6
RCC packs, 7 runtime IDs, and 7 runtime references. Qt-free, call-site,
static, and diff checks passed. Normal warnings remained non-blocking;
`clang-format` and `clang-tidy` were unavailable.

The typed evaluation-default-policy seam is closed. Residual settings callers,
generic settings persistence, and broader page, document, and feature
migrations remain future work; Phase 2 remains open.

## Verified typed automatic-update preference handoff

Against baseline commit `374461a0`, added the Qt-free
`AutomaticUpdatePreferences` value and `AutomaticUpdatePreferencesPort`, plus
the `SettingsManagerAutomaticUpdatePreferencesPort` adapter. It preserves the
exact `updates/automaticChecksEnabled` key, enabled defaults for missing or
invalid values, legacy `QVariant` boolean coercion, and round-trip behavior.
The legacy write is void, so persistence failure is not observable and save
failure is not applicable at this boundary.

Only `src/ui/shared/actions/action_registry.cpp` and
`src/app/controllers/update_controller.cpp` use the typed preference. Checked
state and toggle persistence remain intact. Automatic checks retain
`checkOnStartup` gating, while forced/manual checks, skipped-version and
prompt suppression, and update-dialog behavior remain preserved.

The executor and independent tester reported a passing Debug build, focused
tests 6/6, the exact nine-target regression 9/9, and a passing launch check.
Configure/ownership passed with 737 sources; dependency checks passed for 9
production targets (`ClassMngrNext -> Qt6::Core`); resource checks passed 6
RCC packs, 7 runtime IDs, and 7 runtime references. Call-site, Qt-free,
static, and diff checks passed. Normal warnings remained non-blocking;
`clang-format` and `clang-tidy` were unavailable.

The typed automatic-update preference seam is closed. Residual settings
callers, generic settings persistence, and broader page, document, and feature
migrations remain future work; Phase 2 remains open.

## Verified typed middle-school analytics preference handoff

Against baseline commit `1c8122bb`, added the Qt-free boolean
`MiddleSchoolAnalyticsPreferencesPort` and its
`ApplicationServicesMiddleSchoolAnalyticsPreferencesPort` adapter. The adapter
preserves the exact
`classes_navigation_show_middle_school_analytics_and_evaluations` key, false
default for missing or unavailable settings, legacy `QVariant` boolean
coercion, and round-trip persistence.

Only `src/app/menu_builder.cpp` and
`src/features/classes/ui/classes_page.cpp` use the typed preference. M1-M3
Analytics and Evaluations tab behavior remains preserved; visibility and reset
preference keys are untouched.

The executor and independent tester reported a passing Debug build, focused
tests 9/9 including launch, and the exact nine-target regression 9/9.
Configure/ownership passed with 752 sources; dependency checks passed for 9
production targets (`ClassMngrNext -> Qt6::Core`); resource checks passed 6
RCC packs, 7 runtime IDs, and 7 runtime references. Qt-free, call-site,
static, and diff checks passed. Normal warnings remained non-blocking; a
transient CMake regeneration issue was resolved.

The typed middle-school analytics preference seam is closed. Residual settings
callers, generic settings persistence, and broader page, document, and feature
migrations remain future work; Phase 2 remains open.

## Verified typed class day-filter reset-policy handoff

Against baseline commit `4be0af93`, added the Qt-free
`ClassDayFilterResetPolicy`/`ClassDayFilterResetPolicyPort` contract and its
platform adapter for the exact
`classes_navigation_day_filter_reset_policy` key. It maps
`OnApplicationClose` and `OnPageLeave`, preserves round-trip persistence, and
falls back to `OnApplicationClose` for missing, invalid, or unavailable
settings.

Only the day-filter radio controls in `menu_builder.cpp` and the day-filter
branch of `ClassesPage::hideEvent` use the typed port. Menu radio persistence
is preserved; page leave clears only day-filter state when configured for
`OnPageLeave`. Class-selection reset and visibility policies remain unchanged,
and neither caller makes a direct legacy day-policy call.

The executor reported focused tests 7/7. The independent tester reported PASS
with configure/ownership, a final Debug build, focused tests 9/9, the exact
nine-target regression 9/9, resource/dependency/Qt-free/static/diff checks,
and the exact eight-file scope. Known nonblocking warnings included a
transient LNK1163 resolved by retry, existing linker warnings, unavailable
`clang-format`/`clang-tidy`, and LF/CRLF normalization warnings.

The typed class day-filter reset-policy seam is closed. Phase 2 remains open;
the next slice is not yet selected.

## Verified typed class-selection reset-policy handoff

Against baseline commit `ebc6a3f9`, added the Qt-free
`ClassSelectionResetPolicy`/`ClassSelectionResetPolicyPort` contract and its
platform adapter for the exact
`classes_navigation_class_selection_reset_policy` key. It maps
`OnApplicationClose` and `OnPageLeave`, trims and case-normalizes stored values,
preserves round-trip persistence, and falls back to `OnApplicationClose` for
missing, invalid, or unavailable settings.

The class-selection menu radio persistence is typed independently from the
completed day-filter policy. On `ClassesPage::hideEvent`, `OnPageLeave` clears
only selected/current-class state; day-filter and visibility policies remain
independent, and neither `menu_builder.cpp` nor `classes_page.cpp` directly
calls the legacy class-selection policy functions.

Verification passed configure/ownership with 758 sources; focused tests 10/10
(new adapter, ClassesPage, day-filter, launch, and navigation/analytics/
evaluation/startup visual checks); the exact nine-target regression 9/9;
resources 6 RCC packs/7 runtime IDs/7 runtime references; and dependency,
Qt-free, call-site, static, and diff checks. The exact dirty scope was eight
files. An unrelated full-solution build-directory file-lock failure occurred
after modified targets compiled; it was nonblocking, as were existing linker
warnings. `clang-format`/`clang-tidy` remained unavailable.

The typed class-selection reset-policy seam is closed. Phase 2 remains open;
the next slice is not yet selected.

## Verified typed class-navigation visibility-scope handoff

Against baseline commit `8372fd9d`, added the Qt-free
`ClassVisibilityScope`/`ClassVisibilityPreferencesPort` contract and its
platform adapter for the exact `classes_navigation_visibility_scope` key.
Stored `active_schedule` and `all_classes` map to the corresponding typed
values; missing, invalid, and unavailable settings fall back to
`ActiveSchedule`, with missing-key initialization and trimmed,
case-normalized round-trip persistence preserved.

Typed menu persistence now feeds the initial and refresh loads of
`ClassesPage` and `SpeakingEvalPage`. Existing class-tab and day-filter
semantics remain unchanged, other navigation keys are untouched, and none of
the three callers (`menu_builder.cpp`, `classes_page.cpp`, and
`speaking_eval_page.cpp`) directly accesses `ClassNavigationPreferences`.

Verification recorded a clean Debug build PASS and configure/ownership with
761 sources. Focused visibility/page/navigation checks were 11/12 PASS; the
single unrelated `ClassMngrSpeakingEvalBatchReportServiceTests`
`aiPromptPreviewCopiesAnAnonymousPrompt` case is environment-sensitive, passes
with `QT_QPA_PLATFORM=offscreen`, and its full rerun was interrupted.
Additional navigation checks passed 2/2; the exact nine-target regression
passed 9/9; resources passed 6 RCC packs/7 runtime IDs/7 runtime references;
dependency (`ClassMngrNext -> Qt6::Core`), Qt-free, static, call-site, and diff
checks passed. The exact dirty scope was nine files. Expected Vulkan/zlib and
existing linker warnings remained nonblocking.

The typed class-navigation visibility-scope seam is closed. Phase 2 remains
open; the next slice is not yet selected.

## Verified typed last-selected-campus persistence handoff

Against baseline commit `31db0c2d`, added the Qt-free
`LastSelectedCampusPort` and `SettingsManagerLastSelectedCampusPort` adapter
using `std::optional<Domain::CampusId>`. The canonical legacy key is exactly
`campus/lastSelectedJsonId` (`SettingsManager::Keys::LAST_CAMPUS_JSON_ID`);
blank, invalid, missing, and unavailable values read as no selection without
rewriting, while valid IDs preserve exact text and support set/clear round-trip
behavior.

Only `src/features/campus/ui/campus_dashboard_page.cpp` and
`src/features/campus/ui/campus_dashboard_page_data.cpp` use the typed port.
The explicit current-campus value takes precedence over the persisted fallback.
`UserPreferencesState`, `CampusDirectoryProjection`, and resource surfaces are
unchanged, and the separate legacy integer campus key remains untouched.

Verification passed configure/ownership with 764 sources, a clean Debug build,
focused tests 9/9, and the exact nine-target regression 9/9. Resource checks
passed 6 RCC packs/7 runtime IDs/7 runtime references; dependency
(`ClassMngrNext -> Qt6::Core`), Qt-free, static, call-site, and diff checks
passed. The hardened canonical-literal adapter assertion passed 1/1, and the
exact dirty scope was eight files. Only existing linker and LF/CRLF warnings
were noted.

The typed last-selected-campus persistence seam is closed. Phase 2 remains
open; the next slice is not yet selected.

## Verified typed last-database-directory persistence handoff

Against baseline commit `89068539`, added the Qt-free UTF-8 typed
`LastDatabaseDirectoryPort` and its SettingsManager adapter. The canonical
key is exactly `SettingsManager::Keys::LAST_DATABASE_DIRECTORY`,
`files/lastDirectory`. Missing or empty values read as empty, Unicode and
path values round-trip, and writes retain the legacy synchronized persistence
behavior.

`FileController::databaseDialogDirectory()` prefers the active/current
database directory, then the typed persisted directory, then the default
directory. Remembered writes use the absolute parent directory and return
early for blank paths; create, save-as, and export flows update the typed
boundary. `mostRecentDatabasePath()` remains recent-history-only, recent-file
semantics are unchanged, and `file_controller.cpp` has no direct legacy key or
getter/setter calls for this boundary.

Verification passed configure/ownership with 767 sources, a clean Debug build,
focused tests 9/9, and the exact nine-target regression 9/9. Resource checks
passed 6 RCC packs/7 runtime IDs/7 runtime references; dependency
(`ClassMngrNext -> Qt6::Core`), Qt-free, static, call-site, and diff checks
passed. The exact dirty scope was seven files. Expected Vulkan/zlib, existing
linker, and LF/CRLF warnings were the only noted nonblocking warnings.

The typed last-database-directory persistence seam is closed. Phase 2 remains
open; the next slice is not yet selected.

## Verified typed AI custom-website persistence handoff

Against baseline commit `8579ee1f`, added the Qt-free UTF-8/empty
`AiCommentCustomWebsitePort` and its SettingsManager adapter. The exact
canonical key is `OptionKeys::AiCommentCustomWebsiteUrl`,
`options/aiCommentCustomWebsiteUrl`, with typed read/write/clear behavior.

ActionRegistry, the menu presentation, and both speaking-evaluation dialogs
now use the typed boundary. Existing HTTPS trimming and validation remain
unchanged; invalid stored URLs fall back to ChatGPT, cancel leaves the prior
provider and URL untouched, valid custom URLs still open, and provider/voice
settings remain unchanged.

Verification passed configure/ownership with 770 sources and a clean Debug
build. The offscreen focused suite passed 8/8, covering the adapter, AI
options, both speaking dialog/report paths, dialog shell, launch, resources,
and startup; the additional speaking service check passed. The exact
nine-target regression passed 9/9; resource checks passed 6 RCC packs/7
runtime IDs/7 runtime references; dependency (`ClassMngrNext -> Qt6::Core`),
Qt-free, static, call-site, and diff checks passed. The exact dirty scope was
ten files. Expected Vulkan/zlib and LF/CRLF warnings remained nonblocking; the
default headless dialog mode required `QT_QPA_PLATFORM=offscreen`.

The typed AI custom-website persistence seam is closed. Phase 2 remains open;
the next slice is not yet selected.

## Verified typed AI-comment voice read bridge handoff

Against baseline commit `626501b5`, added the Qt-free typed
`AiCommentVoicePreferencesPort` and its read-only SettingsManager adapter for
the exact canonical key `OptionKeys::AiCommentVoice`,
`options/aiCommentVoice`. Stored `0` maps to `DirectToStudent`, `1` to
`ThirdPerson`, and missing, unknown, or unavailable values map to
`DirectToStudent`.

Only `src/features/speaking_eval/ui/speaking_eval_ai_batch_dialog.cpp` and
`src/features/speaking_eval/ui/speaking_eval_report_dialog.cpp` use the typed
read bridge. ActionRegistry remains the compatibility writer and the menu
remains compatible with it; provider and custom-URL paths and existing prompt
behavior are unchanged.

Verification passed configure/ownership with 773 sources and a clean Debug
build. The offscreen focused suite passed 9/9, covering the adapter, AI
options, both dialogs, speaking service, dialog shell, launch, resources, and
startup. Resource checks passed 6 RCC packs/7 runtime IDs/7 runtime references;
dependency (`ClassMngrNext -> Qt6::Core`), Qt-free, static, call-site, and diff
checks passed. The exact dirty scope was seven files. Expected Vulkan/zlib,
linker, and LF/CRLF warnings remained nonblocking.

The typed AI-comment voice read seam is closed. Phase 2 remains open; the next
slice is not yet selected.

## Verified typed AI-comment provider read bridge handoff

Against baseline commit `6a03386d`, added the Qt-free typed
`AiCommentProviderPreferencesPort` and its read-only SettingsManager adapter
for the exact canonical key `OptionKeys::AiCommentProvider`,
`options/aiCommentProvider`. Stored values map `0` to `ChatGPT`, `1` to
`Gemini`, `2` to `Claude`, `3` to `Microsoft Copilot`, and `4` to
`CustomWebsite`; missing, unknown, and unavailable values map to `ChatGPT`.

Only `src/features/speaking_eval/ui/speaking_eval_ai_batch_dialog.cpp` and
`src/features/speaking_eval/ui/speaking_eval_report_dialog.cpp` use the typed
read bridge. ActionRegistry remains the compatibility writer and the menu
remains compatible with it; voice, custom-URL, prompt, and browser behavior
are unchanged.

Verification passed configure/ownership with 776 sources and a clean Debug
build. The voice/AI/dialog suite passed 9/9, and the provider adapter was
explicitly built and passed standalone 1/1. Resource checks passed 6 RCC
packs/7 runtime IDs/7 runtime references; dependency (`ClassMngrNext ->
Qt6::Core`), Qt-free, static, call-site, and diff checks passed. The exact
dirty scope was seven files. The initial aggregate CTest omitted the provider
target, but the standalone provider test passed; expected Vulkan/zlib, linker,
and LF/CRLF warnings remained nonblocking.

The typed AI-comment provider read seam is closed. Phase 2 remains open; the
next slice is not yet selected.

## Verified typed font-size startup read bridge handoff

Against baseline commit `988db2af`, added the Qt-free typed
`FontSizePreferencesPort` and its read-only SettingsManager adapter for the
exact canonical key `OptionKeys::FontSize`, `options/fontSize`. Stored values
map `-2` to `Small`, `0` to `Normal`, `2` to `Large`, and `4` to `ExtraLarge`;
missing, unknown, and unavailable values map to `Normal`.

`main.cpp` now performs the typed startup read. Existing
`FontManager::setSizeOffset` offsets and visual-capture precedence remain
unchanged, as does the `OptionState<FontSize>` menu compatibility
reader/writer.

Verification passed configure/ownership with 779 sources and a clean Debug
build. The offscreen focused suite passed 6/6, covering the font adapter,
FontManager, startup visual behavior, AI options, launch, and resources.
Resource checks passed 6 RCC packs/7 runtime IDs/7 runtime references;
dependency (`ClassMngrNext -> Qt6::Core`), Qt-free, static, call-site, and diff
checks passed. The exact dirty scope was six files. The adapter test required a
configure refresh; expected warnings remained nonblocking.

The typed font-size startup read seam is closed. Phase 2 remains open; the next
slice is not yet selected.

## Verified typed theme startup read bridge handoff

Against baseline commit `6e67978c`, added the Qt-free typed theme startup read
bridge for the canonical `OptionKeys::Theme == "options/theme"`; this is not
the legacy `SettingsManager::Keys::THEME == "ui/theme"` key. Stored values map
`0` to `Dark`, `1` to `Light`, and `2` to `SystemDefault`; missing, unknown,
and unavailable values map to `SystemDefault`.

`main.cpp` now performs the typed read. Visual-capture precedence and the
existing `ThemeService` values remain unchanged, as do the
`ActionRegistry`/`ThemeController`/`ThemePreferencePort` and menu compatibility
owners.

Verification passed configure/ownership with 782 sources and a clean Debug
build. The offscreen focused suite passed 5/5, covering the theme adapter,
startup visual behavior, AI options, launch, and resources. Resource checks
passed 6 RCC packs/7 runtime IDs/7 runtime references; dependency
(`ClassMngrNext -> Qt6::Core`), Qt-free, static, call-site, and diff checks
passed. The exact dirty scope was six files. The corrected build-target
invocation was used; expected warnings remained nonblocking.

The typed theme startup read seam is closed. Phase 2 remains open; the next
slice is not yet selected.

## Verified typed DialogShell geometry persistence handoff

Against baseline commit `05750cda`, added the Qt-free dialog-geometry
read/write contract and its SettingsManager adapter. The dynamic canonical key
is `ui/dialogs/<normalizedDialogKey>/geometry`; dialog-key trimming and
character normalization are preserved. Missing or empty reads remain empty,
binary geometry round-trips exactly, each dialog is isolated, and an empty key
performs no write.

`DialogShell` now keeps the restore/persist lifecycle guards without singleton
access. Derived-dialog behavior remains unchanged.

Verification passed configure/ownership and a clean Debug build. The offscreen
focused suite passed 7/7, covering the geometry adapter, DialogShell lifecycle
and saved-size behavior, three next-application tests, launch, and resources.
Resource checks passed 6 RCC packs/7 runtime IDs/7 runtime references;
dependency (`ClassMngrNext -> Qt6::Core`), Qt-free, static, call-site, and diff
checks passed. The exact dirty scope was six files. The adapter target
inclusion correction was applied; expected warnings remained nonblocking.

The typed DialogShell geometry persistence seam is closed. Phase 2 remains
open; the next slice is not yet selected.

## Verified typed language-preference persistence/migration bridge handoff

Against baseline commit `e961016d`, added the Qt-free typed language
preference persistence/migration bridge for the canonical
`OptionKeys::Language == "options/language"`. Stored values map `0` to
`SystemDefault`, `1` to `English`, and `5` to `Korean`; legacy values `2`, `3`,
and `4` read as `English` and are rewritten as `1`. Unknown, malformed,
missing, and unavailable values map to `SystemDefault`.

`main.cpp` now performs the typed read. `LanguageService::savedLanguage`
persistence access and stale call sites were removed. Existing
`LanguagePreferencePort` apply behavior and the dominant visual-language
override remain unchanged.

Verification passed configure/ownership with 788 sources and an elevated clean
Debug build. The offscreen focused suite passed 8/8, covering the language
adapter, LanguageService, LanguagePreferencePort, startup visual/performance,
launch, resources, and user preferences. Resource checks passed 6 RCC
packs/7 runtime IDs/7 runtime references; dependency
(`ClassMngrNext -> Qt6::Core`), Qt-free, static, call-site, and diff checks
passed. The exact dirty scope was eight files. The non-elevated build required
a FileTracker retry before the elevated pass; expected warnings remained
nonblocking.

The typed language-preference persistence/migration seam is closed. Phase 2
remains open; the next slice is not yet selected.

## Verified typed upcoming-birthday dismissal write port handoff

Against baseline commit `958b471a`, added the Qt-free write-only typed date
contract for `SettingsManager::Keys::UPCOMING_BIRTHDAYS_DISMISSED_DATE ==
"notifications/upcomingBirthdaysDismissedDate"`. The adapter performs strict
ISO `CalendarEventDate` to legacy `QDate` conversion: valid dates are stored,
while empty or malformed dates do not overwrite the existing value.

Only the sidebar writer was cut over, and it writes only from
`dismissForToday()`. Reminder and visibility behavior remain unchanged; the
obsolete header dependency was removed safely.

Verification passed configure/ownership and elevated clean/focused builds. The
focused suite passed 4/4, covering the adapter, next application contract,
UpcomingBirthdays, and sidebar. Resource checks passed 6 RCC packs/7 runtime
IDs/7 runtime references; dependency (`ClassMngrNext -> Qt6::Core`), Qt-free,
static, call-site, and diff checks passed. The exact dirty scope was seven
files. Expected Vulkan/zlib and LF/CRLF warnings remained nonblocking; stale
MSBuild children were stopped.

The typed upcoming-birthday dismissal write seam is closed. Phase 2 remains
open; the next slice is not yet selected.

## Verified typed document-viewer-background read bridge handoff

Against baseline commit `e5bd3bce`, added the Qt-free typed read bridge for the
canonical `OptionKeys::DocumentViewerBackground ==
"options/documentViewerBackground"`. Stored values map `0` to `Default`, `1`
to `White`, and `2` to `Black`; missing, invalid, unknown, and unavailable
values map to `Default`.

`ActionRegistry` now uses the typed load. The existing `OptionState` remains
the compatibility writer and menu-persistence owner. `PageManager`/
`PdfViewer` live and theme-derived behavior remains unchanged.

Verification passed configure/ownership with 794 sources and an elevated clean
Debug build. The offscreen focused suite passed 5/5, covering the adapter, next
application contract, PageManager, startup visual behavior, and startup
performance/PDF. Resource checks passed 6 RCC packs/7 runtime IDs/7 runtime
references; dependency (`ClassMngrNext -> Qt6::Core`), Qt-free, static,
call-site, and diff checks passed. The exact dirty scope was six files.
Expected warnings remained nonblocking; stale processes were stopped.

The typed document-viewer-background read seam is closed. Phase 2 remains open;
the next slice is not yet selected.

## Verified typed document-page-spacing read bridge handoff

Against baseline commit `b73ffa1b`, added the Qt-free typed read bridge for the
canonical `OptionKeys::DocumentPageSpacing ==
"options/documentPageSpacing"`. Stored values map `0` to `None`, `1` to
`Small`, `2` to `Medium`, and `3` to `Large`; missing, unavailable, and
unknown numeric values map to `Small`. Malformed or non-numeric values retain
legacy parity through unchecked `QVariant::toInt()`, yielding `0` (`None`).

`ActionRegistry` now uses the typed load and installs typed persistence before
initial state selection through `DocumentPageSpacingPreferencesPort`. Valid
typed values write canonical `0`/`1`/`2`/`3` values, while invalid typed values
leave storage unchanged. `OptionState` remains the compatibility state and UI
owner, and `PageManager`/`PdfViewer` behavior remains unchanged.

Verification passed configure/ownership with 797 sources and an elevated clean
Debug build. The offscreen focused suite passed 5/5, covering the adapter,
application contract, PageManager, startup visual behavior, and startup
performance/PDF. Resource checks passed 6 RCC packs/7 runtime IDs/7 runtime
references; dependency (`ClassMngrNext -> Qt6::Core`), Qt-free, static,
call-site, and diff checks passed. The exact dirty scope was six files.
Expected warnings remained nonblocking; stale processes were stopped.

The typed document-page-spacing read seam is closed. Phase 2 remains open; the
next slice is not yet selected.

## Verified typed SaveMode preference read bridge handoff

Against baseline commit `294c7f08`, added the Qt-free typed read bridge for the
canonical `OptionKeys::SaveMode == "options/saveMode"`; this is not the stale
`SettingsManager::Keys::SAVE_MODE == "app/saveMode"` key. Stored values map
`0` to `Automatic` and `1` to `Manual`; missing, malformed, unknown, and
unavailable values map to `Automatic`.

`ActionRegistry` now uses the typed load. The existing `OptionState` remains
the compatibility writer and menu-persistence owner. `MainWindow`,
`PageManager`, and `FileController` behavior remains unchanged.

Verification passed configure/ownership with 800 sources and an elevated clean
Debug build. The offscreen focused suite passed 5/5, covering the adapter, next
application contract, PageManager, startup visual behavior, and startup
performance. Resource checks passed 6 RCC packs/7 runtime IDs/7 runtime
references; dependency (`ClassMngrNext -> Qt6::Core`), Qt-free, static,
call-site, and diff checks passed. The exact dirty scope was six files.
Expected warnings remained nonblocking; stale processes were stopped.

The typed SaveMode read seam is closed. Phase 2 remains open; the next slice is
not yet selected.

## Verified ActionRegistry typed AI-comment-voice read cutover handoff

Against baseline commit `9db2848f`, completed the remaining caller cutover in
`src/ui/shared/actions/action_registry.cpp` to the existing typed voice port
for the exact key `options/aiCommentVoice`. Values map `0` to
`DirectToStudent` and `1` to `ThirdPerson`; missing, unknown, and unavailable
values fall back to `DirectToStudent`. The caller has no direct raw settings
load.

The existing `OptionState` remains the compatibility writer and menu owner.
Provider, custom-URL, prompt, and dialog behavior remain unchanged.

Verification passed configure/ownership with 800 sources and a clean targeted
Debug build. The offscreen focused suite passed 6/6, covering the voice
adapter, stored-voice AI options, report widget, DialogShell, and startup
visual/performance. Resource checks passed 6 RCC packs/7 runtime IDs/7 runtime
references; dependency (`ClassMngrNext -> Qt6::Core`), Qt-free, static,
call-site, and diff checks passed. The exact dirty scope was one file. An
optional broad `ALL_BUILD` was stopped; no code failure was indicated.

The typed AI-comment-voice read cutover is closed. Phase 2 remains open; the
next slice is not yet selected.

## Verified ActionRegistry typed AI-comment-provider read cutover handoff

Against baseline commit `610e53e6`, completed the remaining ActionRegistry
caller cutover to the existing typed provider port for the exact key
`OptionKeys::AiCommentProvider == "options/aiCommentProvider"`. Values map `0`
to `ChatGPT`, `1` to `Gemini`, `2` to `Claude`, `3` to `Microsoft Copilot`,
and `4` to `CustomWebsite`; missing, unknown, and unavailable values fall back
to `ChatGPT`. There is no direct raw provider load.

The existing `OptionState` remains the compatibility writer and menu owner.
Custom URL, provider URL, prompt, voice, and dialog behavior remain unchanged.

Verification passed configure/ownership with 800 sources and a clean focused
Debug build. The offscreen focused suite passed 7/7, covering the provider
adapter, custom-URL adapter, AI-options provider/custom-URL cases, report
widget, DialogShell, and startup visual/performance. Resource checks passed 6
RCC packs/7 runtime IDs/7 runtime references; dependency
(`ClassMngrNext -> Qt6::Core`), Qt-free, static, call-site, and diff checks
passed. The exact dirty scope was one file. Expected warnings remained
nonblocking; no clipboard failure occurred.

The typed AI-comment-provider read cutover is closed. Phase 2 remains open; the
next slice is not yet selected.

## Verified ActionRegistry typed font-size read cutover handoff

Against baseline commit `bc5c90b6`, completed the remaining ActionRegistry
caller cutover to the existing typed font-size port for the exact key
`OptionKeys::FontSize == "options/fontSize"`. Values map `-2` to `Small`, `0`
to `Normal`, `2` to `Large`, and `4` to `ExtraLarge`; missing, unknown, and
unavailable values fall back to `Normal`. The caller has no direct raw load.

`ActionRegistry` now installs typed `onPersist` before startup selection and
no longer performs the direct raw write. The typed adapter writes only the
canonical offsets `-2`, `0`, `2`, and `4`, and ignores invalid values without
changing storage. `OptionState` remains the compatibility state and menu/UI
owner; `FontManager` offsets, `FontSizeController` `onChanged` behavior,
visual-capture precedence, and startup behavior remain unchanged.

Verification passed configure/ownership with 800 sources and a clean focused
Debug build. The offscreen focused suite passed 5/5, covering the font
adapter, FontManager, startup visual behavior, startup performance, and AI
options. Resource checks passed 6 RCC packs/7 runtime IDs/7 runtime references;
dependency (`ClassMngrNext -> Qt6::Core`), Qt-free, static, call-site, and diff
checks passed. The exact dirty scope was one file. Expected warnings remained
nonblocking.

The typed ActionRegistry font-size read cutover is closed. Phase 2 remains
open; the next slice is not yet selected.

## Verified ActionRegistry typed theme read cutover handoff

Against baseline commit `50de1ce4`, completed the remaining ActionRegistry
caller cutover to the existing typed theme port for canonical
`OptionKeys::Theme == "options/theme"`, distinct from legacy
`SettingsManager::Keys::THEME == "ui/theme"`. Values map `0` to `Dark`, `1`
to `Light`, and `2` to `SystemDefault`; missing, unknown, and unavailable
values fall back to `SystemDefault`. The caller has no direct raw load.

The existing `OptionState` remains the compatibility writer and menu owner.
ThemeController synchronization, palette/icon refresh, visual-capture
precedence, and user writes remain unchanged.

Verification passed configure/ownership with 800 sources and a clean focused
Debug build. The offscreen focused suite passed 4/4, covering the theme
adapter, startup visual behavior, startup performance, and AI options.
Resource, dependency (`ClassMngrNext -> Qt6::Core`), Qt-free, static,
call-site, and diff checks passed. The exact dirty scope was one file.
Expected warnings remained nonblocking.

The typed ActionRegistry theme read cutover is closed. Phase 2 remains open;
the next slice is not yet selected.

## Verified ActionRegistry typed language read cutover handoff

Against baseline commit `46ef66f1`, completed the remaining ActionRegistry
caller cutover to the existing migration-aware language port for canonical
`OptionKeys::Language == "options/language"`. Values map `0` to
`SystemDefault`, `1` to `English`, and `5` to `Korean`; legacy values `2`, `3`,
and `4` read as `English` and are written back as `1`. Unknown, malformed,
missing, and unavailable values fall back to `SystemDefault`. The caller has no
direct raw load.

The existing `OptionState` remains the compatibility writer and menu owner.
Retranslation, font refresh, visual override, controller synchronization, and
user writes remain unchanged.

Verification passed configure/ownership with 800 sources and a clean focused
Debug build. The offscreen focused suite passed 6/6, covering the language
adapter, LanguagePreferencePort, LanguageService, startup visual/performance,
and AI options. Resource, dependency (`ClassMngrNext -> Qt6::Core`), Qt-free,
static, call-site, and diff checks passed. The exact dirty scope was one file.
Expected warnings remained nonblocking.

The typed ActionRegistry language read cutover is closed. Phase 2 remains open;
the next slice is not yet selected.

## Verified typed ScheduleWidget display-mode persistence cutover handoff

Against baseline commit `41859a5f`, added the Qt-free display-mode contract for
`Regular`, `Intensive`, and `Testing`. The canonical key is
`schedule_display_mode`; first load falls back to legacy
`schedule_show_intensive` when the canonical value is absent, then migrates
that result to the canonical key. Missing or invalid values fall back to
`Regular`.

Unavailable reads return `Regular` and unavailable saves are no-ops.
`ScheduleWidget` alone now uses the typed load/save boundary. Existing
`ClassesPage` and `SpeakingEval` compatibility callers/helper remain
unchanged. Button refresh, reload, and `displayModeChanged` ordering remain
unchanged.

Verification passed configure/ownership with 803 sources and a clean focused
Debug build. The offscreen focused suite passed 6/6, covering the adapter,
ScheduleWidget, builder, print model/PDF, and existing schedule-display
adapter. Resource, dependency (`ClassMngrNext -> Qt6::Core`), Qt-free, static,
call-site, and diff checks passed. The exact dirty scope was six files.
Expected warnings remained nonblocking.

The typed ScheduleWidget display-mode persistence seam is closed. Phase 2
remains open; the next slice is not yet selected.

## Verified ClassesPage typed schedule-display-mode caller cutover handoff

Against baseline commit `83cca837`, completed the ClassesPage caller cutover
to the typed schedule-display-mode boundary. The canonical key is
`schedule_display_mode`, with legacy fallback key `schedule_show_intensive`.
Values are trimmed and case-normalized; invalid modern values do not overwrite
the canonical setting, while a missing modern key performs the legacy-to-
canonical migration. An unavailable service resolves to `Regular`.

`Testing` preserves the existing `Regular` schedule-source behavior.
`SpeakingEvalPage` remains the compatibility caller. The exact current
production/test-target scope is `src/features/classes/ui/classes_page.cpp` and
`cmake/tests/pages_and_output.cmake`.

Verification passed configure/build and ownership validation with 803 sources;
the focused suite passed 7/7. Resource, Qt-free, static, call-site, and diff
checks passed. Known warnings were missing Vulkan headers, existing
`/FORCE`/duplicate-stub linker warnings, and LF-to-CRLF normalization.

This ClassesPage caller-cutover slice is closed. Phase 2 remains open; the next
boundary is not yet selected.

## Verified final SpeakingEvalPage schedule-display-mode seam handoff

Against baseline commit `01092f57`, completed the final schedule-display-mode
compatibility caller cutover. `SpeakingEvalPage` now uses the existing typed
`ApplicationServicesScheduleDisplayModePreferencesPort`, mapping typed
`Regular`/`Intensive`/`Testing` to the legacy UI enum while preserving page
lifecycle, evaluation loading, visibility, and `scheduleSourceForMode`
behavior. `ScheduleWidget` remains the write owner.

The canonical key is `schedule_display_mode`, with legacy fallback/migration
from `schedule_show_intensive`. Reads trim and case-normalize; an invalid
modern value does not overwrite the canonical setting, a missing modern key
migrates the legacy value, and an unavailable service resolves to `Regular`.
No legacy callers or source-list references remain. The exact seven-file
implementation scope is:

- `cmake/production_sources.cmake`
- `cmake/tests/features.cmake`
- `cmake/tests/pages_and_output.cmake`
- `src/features/speaking_eval/ui/speaking_eval_page.cpp`
- `src/features/speaking_eval/ui/speaking_eval_page_p.h`
- deleted `src/features/schedule/schedule_display_mode_preferences.cpp`
- deleted `src/features/schedule/schedule_display_mode_preferences.h`

Verification passed ownership validation with 801 handwritten sources, a clean
Debug rebuild, focused CTest 8/8, and an offscreen launch smoke test. Resource
checks passed 6 RCC packs/7 runtime IDs/7 runtime references; dependency and
Qt-free, static, and call-site checks passed; `git diff --check` passed.
Warnings were limited to the nonfatal MSB8064 generated-autogen notice for
stale deleted-helper paths, existing `/FORCE`/duplicate-stub linker warnings,
missing Vulkan headers, and LF-to-CRLF normalization.

This final schedule-display-mode seam is closed. Phase 2 remains open; the
next boundary is not yet selected.

## Verified typed two-key calendar event-display preferences boundary handoff

Against baseline commit `db8c7cf1`, completed the Qt-free typed boundary for
`calendar/showEventsAtAllCampuses` and `calendar/hideStartOfTermEvents`.
Missing and unavailable values default to `false`; exact-key round trips,
legacy `QVariant` boolean coercion, atomic `saveAll` failure/rollback, and
unrelated-setting preservation are covered.

`CalendarPreferencesPanel` now uses the typed port for load/save while
preserving warning behavior. `CalendarPageUpcomingEvents` uses the typed port
for display reads while preserving filtering. New sources are registered once,
the contract remains Qt-free, and existing production ownership is clean.

The exact seven-file implementation scope is:

- `cmake/next.cmake`
- `cmake/tests/next.cmake`
- `src/features/calendar/ui/calendar_page_upcoming_events.cpp`
- `src/features/calendar/ui/calendar_preferences_panel.cpp`
- `src/next/application/calendar_event_display_preferences.h`
- `src/next/platform/application_services_calendar_event_display_preferences_port.h`
- `tests/next_platform_application_services_calendar_event_display_preferences_port_tests.cpp`

Verification passed ownership validation with 804 handwritten sources and a
clean Debug rebuild. Focused tests passed 7/7, including adapter,
calendar/page/ScheduleWidget coverage; the offscreen launch smoke passed.
Resource checks passed 6 RCC packs/7 runtime IDs/7 runtime references;
dependency, Qt-free, static, call-site, and `git diff --check` checks passed.
There is no standalone CalendarPreferencesPanel or UpcomingEvents test target;
coverage is through compilation, smoke/surrounding tests, and static review.
Warnings were missing Vulkan headers, the Qt bundled-zlib fallback, existing
`/FORCE`/duplicate-symbol linker warnings, and LF-to-CRLF normalization.

This calendar event-display preferences boundary is closed. Phase 2 remains
open; the next boundary is not yet selected.

## Verified typed AcademicCalendarProvider first-day-of-week preferences boundary handoff

Against baseline commit `6a33df6d`, completed the typed first-day-of-week
preferences boundary owned by `AcademicCalendarProvider` for the exact key
`calendar/firstDayOfWeek`. Persisted values `0..6` read directly; missing or
invalid values use the locale fallback, while an unavailable service uses the
same fallback and makes saves no-ops. Exact-key round trips are preserved.

`setFirstDayOfWeek` continues to normalize to Sunday/Monday (`0`/`1`). Provider
revision and signal ordering and save-warning behavior remain unchanged.
`AcademicCalendarProvider` has no direct raw key access. The
`calendar/academicSchedule/v1` JSON, QML, panel, and compatibility key owners
remain unchanged.

The exact six-file implementation scope is:

- `cmake/next.cmake`
- `cmake/tests/next.cmake`
- `src/features/calendar/ui/academic_calendar_provider.cpp`
- `src/next/application/calendar_first_day_of_week_preferences.h`
- `src/next/platform/application_services_calendar_first_day_of_week_preferences_port.h`
- `tests/next_platform_application_services_calendar_first_day_of_week_preferences_port_tests.cpp`

Verification passed ownership validation with 807 handwritten sources and a
clean Debug rebuild. Focused tests passed 8/8, including the new adapter,
`ClassMngrAcademicCalendarTests`, and calendar/page/ScheduleWidget coverage;
the offscreen launch smoke passed. Resource checks passed 6 RCC packs/7
runtime IDs/7 runtime references; dependency, Qt-free, static, call-site, and
diff checks passed. Warnings were missing Vulkan headers, the Qt bundled-zlib
fallback, existing `/FORCE`/duplicate-stub linker warnings, and LF-to-CRLF
normalization.

This narrow calendar first-day-of-week boundary is closed. Phase 2 remains
open; the next boundary is not yet selected.

## Verified typed AcademicCalendarProvider schedule-persistence boundary handoff

Against baseline commit `7a50b3e9`, completed the typed persistence boundary
for the exact key `calendar/academicSchedule/v1`. The Qt-free contract carries
opaque `std::string` JSON, with the adapter preserving exact-key UTF-8/
`QVariant` round trips. Missing or unavailable services return an empty read
and make writes no-ops; payload and unrelated settings are preserved, and save
failure retains warning behavior.

`AcademicCalendarProvider::reload()` and `persist()` now use only the typed
port. `AcademicCalendarSchedule::toJson()`/`fromJson()`, schema/version 1,
malformed-load clearing, defaults, provider API, revision/signal ordering,
panel, QML, and production ownership remain unchanged.

The exact six-file implementation scope is:

- `cmake/next.cmake`
- `cmake/tests/next.cmake`
- `src/features/calendar/ui/academic_calendar_provider.cpp`
- `src/next/application/academic_calendar_schedule_preferences.h`
- `src/next/platform/application_services_academic_calendar_schedule_preferences_port.h`
- `tests/next_platform_application_services_academic_calendar_schedule_preferences_port_tests.cpp`

Verification passed ownership validation with 810 handwritten sources and a
clean Debug rebuild. Focused tests passed 8/8, including the new adapter,
`ClassMngrAcademicCalendarTests` JSON round-trip/malformed fallback coverage,
and calendar/page/ScheduleWidget coverage; the offscreen launch smoke passed.
Resource checks passed 6 RCC packs/7 runtime IDs/7 runtime references;
dependency, Qt-free, static, call-site, and diff checks passed. Warnings were
missing Vulkan headers, the Qt bundled-zlib fallback, existing
`/FORCE`/duplicate-stub linker warnings, and LF-to-CRLF normalization.

This remaining narrow calendar persistence boundary is closed. Phase 2 remains
open; the next boundary is not yet selected.

## Verified typed CalendarPage event-type color persistence boundary handoff

Against baseline commit `c74b8a9c`, completed the typed CalendarPage
event-type color persistence boundary. Dynamic keys are
`calendar/eventTypeColor/<normalized-event-type>`, with the exact normalized
event types `Vacation`, `Holiday`, `Workshop`, `CM`, `Meeting`, and `Other`.
Event-type normalization and `QColor` conversion remain in the UI; the adapter
owns only typed key construction and persistence.

Valid `QColor::HexRgb` values round-trip and store exactly. Missing or
unavailable values use the UI default; invalid stored colors pass through the
adapter and fall back in the UI. Invalid colors are no-op saves, save failures
retain their warning, and unrelated settings are preserved. `myInfo`, campus,
`custom_colors`, and generic settings remain separate.

`CalendarPage` no longer raw-accesses dynamic color keys; defaults, warning
paths, and repaint/filter-refresh ordering remain unchanged. The exact
six-file implementation scope is:

- `cmake/next.cmake`
- `cmake/tests/next.cmake`
- `src/features/calendar/ui/calendar_page_upcoming_events.cpp`
- `src/next/application/calendar_event_type_color_preferences.h`
- `src/next/platform/application_services_calendar_event_type_color_preferences_port.h`
- `tests/next_platform_application_services_calendar_event_type_color_preferences_port_tests.cpp`

Verification passed ownership validation with 813 handwritten sources and an
elevated Debug focused build. Focused tests passed 7/7, including the adapter
and calendar/page-adjacent coverage; the offscreen launch smoke passed.
Resource checks passed 6 RCC packs/7 runtime IDs/7 runtime references;
dependency, Qt-free, static, dynamic-key call-site, and diff checks passed.
The initial stale CTest listing was resolved by target regeneration. Existing
`/FORCE`/duplicate-symbol linker warnings, missing Vulkan headers, and
LF-to-CRLF normalization remained nonblocking; idle MSBuild nodes were also
nonblocking.

This event-type color boundary is closed. Phase 2 remains open; the next
boundary is not yet selected.

## Verified typed CalendarPage current-campus read boundary handoff

Against baseline commit `9413fc1d`, completed the typed CalendarPage
current-campus read boundary for the exact key `myInfo/campus`. The adapter is
read-only and performs no writes: it reads the exact key, returns an empty
value when the setting is missing or unavailable, preserves verbatim
`QVariant::toString()` behavior, and leaves unrelated settings untouched.

CalendarPage preserves empty handling, current/all-campus code construction,
case-insensitive matching, duplicate removal, filtering, and refresh behavior.
This boundary is separate from `campus/lastSelectedJsonId`, and
`PersonalDetailsRepository::saveCampus` remains the writer.

The exact six-file implementation scope is:

- `cmake/next.cmake`
- `cmake/tests/next.cmake`
- `src/features/calendar/ui/calendar_page_upcoming_events.cpp`
- `src/next/application/current_campus_preferences.h`
- `src/next/platform/application_services_current_campus_preferences_port.h`
- `tests/next_platform_application_services_current_campus_preferences_port_tests.cpp`

Verification passed ownership validation with 816 handwritten sources and an
elevated Debug build. Focused tests passed 7/7; the adapter passed 4/4, and
last-selected-campus plus CampusDashboard regressions passed 2/2. The
offscreen launch smoke passed. Resource checks passed 6 RCC packs/7 runtime
IDs/7 runtime references; dependency, Qt-free, static, key-separation,
call-site, and diff checks passed. Warnings were missing Vulkan headers, the
Qt bundled-zlib fallback, existing `/FORCE`/duplicate-symbol linker warnings,
and LF-to-CRLF normalization.

This current-campus boundary is closed. Phase 2 remains open; the next
boundary is not yet selected.

## Verified typed custom-color palette persistence boundary handoff

Against baseline commit `84e5cf70`, completed the typed custom-color palette
persistence boundary for the exact key `custom_colors`. The typed port owns a
fixed 16-entry palette with canonical uppercase defaults, missing/unavailable
fallbacks, compact JSON round-trip, legacy `QStringList`/JSON/separator
payload compatibility, invalid-entry fallback, fixed 16-entry normalization,
canonicalization, save-failure warning, and unrelated-setting preservation.
`QColorDialog` behavior remains unchanged.

The exact six-file implementation scope is:

- `cmake/next.cmake`
- `cmake/tests/next.cmake`
- `src/core/utils/colorutils.cpp`
- `src/next/application/custom_color_palette_preferences.h`
- `src/next/platform/application_services_custom_color_palette_preferences_port.h`
- `tests/next_platform_application_services_custom_color_palette_preferences_port_tests.cpp`

`ColorUtils` now uses the typed port with no raw settings calls. `myInfo/name`,
Sub Prep, `OptionState`, and unrelated settings remain separate. Compatibility
diagnosis confirmed that valid stored colors are Qt-canonicalized lowercase on
read, while default fallback entries remain uppercase on write. SQLite coerces
`QStringList` settings to empty `TEXT`, so the `QStringList` parser path is
covered directly through the adapter helper.

Verification passed ownership validation with 819 handwritten sources and an
elevated Debug build. Focused tests passed 5/5 (adapter, TestingClassesPage,
ClassesPage, ScheduleImportDialog, and SubPrepPage); adapter slots passed 6/6,
and the offscreen launch smoke passed. Resource checks passed 6 RCC packs/7
runtime IDs/7 runtime references; dependency, Qt-free, static, call-site, and
diff checks passed. Warnings were missing Vulkan headers/Qt zlib fallback,
existing `/FORCE`/duplicate-symbol linker warnings, and LF-to-CRLF
normalization; no standalone ColorUtils target exists.

This custom-color boundary is closed. Phase 2 remains open; the next boundary
is not yet selected.

## Verified typed personal display-name read bridge handoff

Against baseline commit `4c0f891d`, completed the typed, read-only personal
display-name bridge for the exact key `myInfo/name`. The adapter performs an
exact-key read, returns an empty value when the setting is missing or
unavailable, preserves UTF-8 and whitespace round-trip behavior, and leaves
unrelated settings untouched. It performs no writes, exposes no direct reader
key access to callers, and does not use `SettingsManager`.

Schedule output consumes the raw value; ScheduleImportDialog and
SubPrepPage retain their existing trimmed-value policies. `PersonalDetailsRepository`
and other writers remain unchanged, as does the direct schedule-import SQL.
The exact eight-file implementation scope is:

- `cmake/next.cmake`
- `cmake/tests/next.cmake`
- `src/features/schedule/services/schedule_output_controller.cpp`
- `src/features/schedule/ui/schedule_import_dialog.cpp`
- `src/features/sub_prep/ui/sub_prep_print_dialog.cpp`
- `src/next/application/personal_display_name_preferences.h`
- `src/next/platform/application_services_personal_display_name_preferences_port.h`
- `tests/next_platform_application_services_personal_display_name_preferences_port_tests.cpp`

Verification passed ownership validation with 822 handwritten sources and a
Debug build. Focused tests passed 3/3 (adapter, ScheduleImportDialog, and
SubPrepPage); adapter slots passed 3/3, and schedule-output coverage passed
3/3 for ScheduleWidget, PrintModel, and PrintPdf. The offscreen launch smoke
passed. Resource checks passed 6 RCC packs/7 runtime IDs/7 runtime references;
dependency, Qt-free, static, call-site, writer-preservation, and diff checks
passed. Warnings were Vulkan/zlib configure notices, existing forced-link and
duplicate-symbol warnings, and LF-to-CRLF normalization.

This personal display-name boundary is closed. Phase 2 remains open; the next
boundary is not yet selected.

## Verified typed Sub Prep saved-content settings bundle handoff

Against baseline commit `d4dc18e0`, completed the typed Sub Prep saved-content
settings boundary for the exact keys `subPrep/classMaterials`,
`subPrep/bookReportGrading`, `subPrep/bookReportSpecialInstructions`, and
`subPrep/subComments`. Typed load/save replaces only these raw settings calls,
with one atomic `saveAll`, missing-versus-present grading and special-instruction
defaults, unavailable-service no-op load and failed-save behavior, unrelated
setting preservation, and existing dirty/autosave/failure behavior retained.

The exact seven-file implementation scope is:

- `cmake/next.cmake`
- `cmake/tests/next.cmake`
- `src/features/sub_prep/ui/sub_prep_page_p.h`
- `src/features/sub_prep/ui/sub_prep_page_settings.cpp`
- `src/next/application/sub_prep_preferences.h`
- `src/next/platform/application_services_sub_prep_preferences_port.h`
- `tests/next_platform_application_services_sub_prep_preferences_port_tests.cpp`

Zoom, campus, name/PersonalDetails, `OptionState`, and the broader Sub Prep
migration remain separate. A compatibility correction is recorded: missing
`loadSetting` returns a successful result containing an invalid `QVariant`; the
adapter test asserts both `has_value` and `!value.isValid()`.

Verification passed ownership validation with 825 handwritten sources and a
Debug build. Focused tests passed 2/2; adapter slots passed 5/5, including the
required SubPrep regressions
`freshAndExistingGradingSettingsResolveWithoutDataLoss` and
`clearDatabaseStateStopsAutosaveAndRemovesLoadedContent`. The offscreen launch
smoke passed. Resource checks passed 6 RCC packs/7 runtime IDs/7 runtime
references; dependency, Qt-free, static, call-site, and diff checks passed.
The initial non-elevated FileTracker access denial was resolved by elevated
retry. Other warnings were Vulkan/zlib notices, existing `/FORCE`/duplicate-
symbol warnings, LF-to-CRLF normalization, and resident MSBuild nodes.

This Sub Prep saved-content boundary is closed. Phase 2 remains open; the next
boundary is not yet selected.

## Verified typed Sub Prep current-campus read cutover handoff

Against baseline commit `ffd1f769`, completed the Sub Prep current-campus read
cutover by reusing the existing `CurrentCampusPreferencesPort`. The exact
three-file implementation scope is:

- `src/features/sub_prep/ui/sub_prep_page_p.h`
- `src/features/sub_prep/ui/sub_prep_page_settings.cpp`
- `tests/sub_prep_page_tests.cpp`

The cutover removes `SettingsKeys::MyInfoCampus`, retains the settings
availability gate, and leaves no raw `myInfo/campus` read in Sub Prep. Existing
UTF-8 conversion, trimming, case-insensitive ID/display-name matching,
first-campus fallback, campus-field population, and unavailable-service no-op
behavior are preserved. `PersonalDetailsRepository::saveCampus` remains the
writer, and other Sub Prep settings remain untouched.

Verification passed ownership validation with 825 handwritten sources and a
Debug build. CTest passed 2/2; current-campus adapter slots passed 4/4, and
the individual Sub Prep tests passed 3/3:
`savedCampusSelectionUsesTypedRead`,
`freshAndExistingGradingSettingsResolveWithoutDataLoss`, and
`clearDatabaseStateStopsAutosaveAndRemovesLoadedContent`. The offscreen launch
smoke passed. Resource checks passed 6 RCC packs/7 runtime IDs/7 runtime
references; dependency, Qt-free, static, exact-key, call-site,
writer-preservation, and diff checks passed. Warnings were known Vulkan/zlib
notices, existing `/FORCE`/duplicate-symbol warnings, and LF-to-CRLF
normalization; initial guessed adapter slot names were corrected from the
executable listing.

This Sub Prep current-campus boundary is closed. Phase 2 remains open; the
next boundary is not yet selected.

## Verified typed Sub Prep personal-Zoom read/migration boundary handoff

Against baseline commit `2ae382db`, completed the typed Sub Prep personal-Zoom
read/migration boundary. Primary keys are `myInfo/zoomLoginId`,
`myInfo/zoomPassword`, and `myInfo/zoomNotAvailable`; legacy fallbacks are
`subPrep/personalZoomEmail`, `subPrep/personalZoomPassword`, and
`subPrep/personalZoomNotAvailable`.

Primary values take precedence. Legacy reads are best-effort migrated to the
primary keys; if migration save fails, the legacy values remain readable.
Missing values retain N/A credentials and the default unavailable state, and
unavailable-service behavior is preserved. QVariant/UTF-8 coercion,
credential hiding, and `valueOrNa` display behavior remain unchanged. There
are no writer changes to `PersonalDetailsRepository`, personal-details
aggregation, `OptionState`, or unrelated Sub Prep settings.

The exact seven-file implementation scope is:

- `cmake/next.cmake`
- `cmake/tests/next.cmake`
- `src/features/sub_prep/ui/sub_prep_page_p.h`
- `src/features/sub_prep/ui/sub_prep_page_settings.cpp`
- `src/next/application/sub_prep_personal_zoom_preferences.h`
- `src/next/platform/application_services_sub_prep_personal_zoom_preferences_port.h`
- `tests/next_platform_application_services_sub_prep_personal_zoom_preferences_port_tests.cpp`

Verification passed ownership validation with 828 handwritten sources and a
Debug build. CTest passed 2/2; adapter slots passed 5/5, and Sub Prep
regressions passed, including `zoomUnavailableHidesStoredCredentials`, the
grading regression, and the database-state/autosave regression. The offscreen
launch smoke passed. Resource checks passed 6 RCC packs/7 runtime IDs/7 runtime
references; dependency, Qt-free, static, key-ownership, call-site, and diff
checks passed. Warnings were Vulkan/zlib notices, existing
`/FORCE`/duplicate-symbol warnings, and LF-to-CRLF normalization.

This personal-Zoom boundary is closed. Phase 2 remains open; the next boundary
is not yet selected.

## Verified typed personal-display-name writer extension handoff

Against baseline commit `f4480483`, extended the existing typed `myInfo/name`
bridge with a typed save. Successful saves persist the trimmed name; existing
names are not overwritten, disabled folder creation performs no write,
unavailable writes are silent no-ops, and save failures map to technical
errors while retaining the warning. UTF-8/whitespace behavior and unrelated
settings remain preserved.

The exact four-file implementation scope is:

- `src/next/application/personal_display_name_preferences.h`
- `src/next/platform/application_services_personal_display_name_preferences_port.h`
- `src/features/sub_prep/ui/sub_prep_print_dialog.cpp`
- `tests/next_platform_application_services_personal_display_name_preferences_port_tests.cpp`

`SubPrepPrintDialog` has no raw key or direct settings access.
`PersonalDetailsRepository::save` remains the aggregate writer; no
`OptionState` or full personal-details migration is included.

Verification passed ownership validation with 828 handwritten sources and a
Debug build. CTest passed 2/2; adapter slots passed 5/5, including
`printDialogRequiresAndSavesMissingUserName`. The offscreen launch smoke
passed. Resource checks passed 6 RCC packs/7 runtime IDs/7 runtime references;
dependency, Qt-free, static, call-site, writer-preservation, and diff checks
passed. Warnings were Vulkan/zlib notices, existing `/FORCE`/duplicate-symbol
warnings, and LF-to-CRLF normalization.

This personal-display-name writer boundary is closed. Phase 2 remains open; the
next boundary is not yet selected.

## Verified typed personal signature-image read port handoff

Against baseline commit `fa8b5193`, completed the typed personal signature-image
read boundary for the exact key `myInfo/signatureImage`. The port Base64-decodes
the stored value, invokes the existing `SignatureImage::prepareForEmbedding`
exactly once, and returns opaque bytes. Missing, invalid, unavailable, or
corrupt-Base64 values become empty; the port performs no writes.

The two page-actions `PersonalDetailsRepository` calls were removed while
report generation, PowerPoint output, and class-service guards remain
unchanged. The exact six-file implementation scope is:

- `cmake/next.cmake`
- `cmake/tests/next.cmake`
- `src/features/speaking_eval/ui/speaking_eval_page_actions.cpp`
- `src/next/application/personal_signature_image.h`
- `src/next/platform/application_services_personal_signature_image_port.h`
- `tests/next_platform_application_services_personal_signature_image_port_tests.cpp`

Verification passed ownership validation with 831 handwritten sources and a
clean-first full Debug build. CTest passed 3/3; adapter slots passed 4/4, and
the offscreen launch smoke passed. Resource, Qt-free, static, exact-key,
one-preparation-call, call-site, writer-preservation, dependency, and diff
checks all passed. Expected notices were missing Vulkan headers, the bundled
zlib fallback, MSVC/build warnings, and LF-to-CRLF normalization.

This personal signature-image boundary is closed. Phase 2 remains open; the
next boundary is not yet selected.

## Verified typed InitialSetupWizard signature-image reads handoff

Against baseline commit `039a618c`, completed the InitialSetupWizard slice:
both remaining personal signature-image reads now use the existing typed
`PersonalSignatureImagePort` and
`ApplicationServicesPersonalSignatureImagePort`. The adapter retains the
exact-key, Base64-decoding, exactly-once preparation, opaque-byte, and
read-only semantics; no new contract, adapter, or CMake change was needed.

The exact implementation/test scope is:

- `src/features/setup/ui/initial_setup_wizard.cpp`
- `tests/initial_setup_wizard_tests.cpp`

PersonalDetailsRepository aggregate loading for the name and other fields,
aggregate save, existing image file-selection replacement/write behavior,
preview behavior, and guards remain unchanged. Temporary-database coverage
verified valid image use, missing/unavailable/corrupt values as empty, and
preservation of the existing image when no replacement is selected.

Verification passed ownership validation with 831 handwritten sources and a
clean-first serial Debug build. CTest passed 4/4; wizard slots passed 2/2 and
signature-port slots passed 4/4. Resource, dependency, static, source,
offscreen, and diff checks passed. Expected notices were missing Vulkan
headers, the Qt bundled-zlib fallback, and LF-to-CRLF normalization. Four
leftover MSBuild processes were stopped cleanly; no command remains active.

This InitialSetupWizard boundary is closed. Phase 2 remains open; the next
boundary is not yet selected.

## Verified typed InitialSetupWizard display-name prefill handoff

Against baseline commit `bd1e0fec`, completed the remaining InitialSetupWizard
`myInfo/name` prefill cutover using the existing typed
`PersonalDisplayNamePreferencesPort` and
`ApplicationServicesPersonalDisplayNamePreferencesPort`. UTF-8 and whitespace
behavior matches legacy `QVariant::toString()` semantics; missing or
unavailable values are empty, and the read path performs no writes.

The exact implementation/test scope is:

- `src/features/setup/ui/initial_setup_wizard.cpp`
- `tests/initial_setup_wizard_tests.cpp`

PersonalDetailsRepository aggregate loading remains for `validatePage` aggregate
save and other fields. Trimming/validation, aggregate save, the typed
signature-image path, and guards remain preserved; no new contract, adapter,
CMake change, or unrelated migration was introduced.

Verification passed ownership validation with 831 handwritten sources and a
clean-first Debug build. CTest passed 5/5; new wizard name slots passed 2/2,
the display-name adapter passed 5/5, and the signature adapter passed 4/4.
Resource, dependency, offscreen, static, source, and diff checks passed. A
transient stale-AUTOGEN omission was resolved by forced regeneration/rebuild.
Expected notices were missing Vulkan headers, the bundled-zlib fallback, and
LF-to-CRLF normalization. Three leftover MSBuild processes were stopped; no
command remains active.

This InitialSetupWizard display-name boundary is closed. Phase 2 remains open;
the next boundary is not yet selected.

## Verified typed current-campus writer boundary handoff

Against baseline commit `b3a90622`, extended the existing current-campus port
with the Qt-free `write(std::string) -> Domain::Result<void>` contract. The
adapter writes the exact key `myInfo/campus` with UTF-8 conversion and maps
unavailable services and save failures. The PersonalDetailsPage corrective
write now uses this port.

The exact implementation/test scope is:

- `src/features/my_info/ui/personal_details_page_sections.cpp`
- `src/next/application/current_campus_preferences.h`
- `src/next/platform/application_services_current_campus_preferences_port.h`
- `tests/next_platform_application_services_current_campus_preferences_port_tests.cpp`

Aggregate `PersonalDetailsRepository::save()`, current-campus read behavior,
matching/filtering/fallback/refresh/guards, other callers, and CMake remain
unchanged. Tests covered exact-key UTF-8 write, unrelated-setting
preservation, unavailable no-op, QVariant coercion, and save failure.

Verification passed ownership validation with 831 handwritten sources and a
clean-first serial Debug build. Focused tests passed 4/4, including
current-campus, MyWorkspace, CampusDashboard, and last-selected-campus;
adapter slots passed 6/6. Resource, dependency, offscreen, static, source,
and diff checks passed. Three leftover MSBuild processes were stopped; no
command remains active. Expected notices were missing Vulkan headers, the Qt
bundled-zlib fallback, and LF-to-CRLF normalization.

This current-campus writer boundary is closed. Phase 2 remains open; the next
boundary is not yet selected.

## Verified typed PersonalDetailsPage signature-image read handoff

Against baseline commit `f434d5b3`, completed the remaining
PersonalDetailsPage `myInfo/signatureImage` read using the existing typed
`PersonalSignatureImagePort` and
`ApplicationServicesPersonalSignatureImagePort`. Prepared opaque bytes are
converted at the Qt boundary.

The exact implementation/test scope is:

- `src/features/my_info/ui/personal_details_page_sections.cpp`
- `tests/my_workspace_page_tests.cpp`

Aggregate `PersonalDetailsRepository` load/save, image replacement/removal
write behavior, no-write load semantics, name/campus/Zoom/typed signature
fields, and guards remain preserved. No new contract, adapter, CMake change,
or unrelated migration was introduced. Valid prepared previews work; missing,
corrupt, or unavailable values retain the default behavior. Existing adapter
coverage verifies exact Base64 decoding, preparation, and no-write semantics.

Verification passed ownership validation with 831 handwritten sources and a
clean-first Debug build. Focused tests passed 4/4 (MyWorkspace, signature
adapter, current-campus adapter, and CampusDashboard); new MyWorkspace slots
passed 2/2 and signature-adapter slots passed 4/4. Resource, dependency,
offscreen, static, source, and diff checks passed. The page test checks a
non-null preview; exact prepared bytes remain covered by adapter tests.
Expected notices were Vulkan/zlib and LF-to-CRLF normalization. Three leftover
MSBuild processes were stopped; no command remains active.

This PersonalDetailsPage signature-image boundary is closed. Phase 2 remains
open; the next boundary is not yet selected.

## Verified typed PersonalDetailsPage current-campus read handoff

Against baseline commit `9cec8483`, completed the remaining
`PersonalDetailsPage::loadStoredSettings()` current-campus read using the
existing typed `CurrentCampusPreferencesPort` and
`ApplicationServicesCurrentCampusPreferencesPort`. Exact UTF-8/verbatim
semantics are preserved. Repository loading remains for name, Zoom, and typed
signature fields.

The exact implementation/test scope is:

- `src/features/my_info/ui/personal_details_page_sections.cpp`
- `tests/my_workspace_page_tests.cpp`

Missing/unavailable behavior, combo matching, first-campus fallback,
case-insensitive comparison, corrective typed write, refresh, guards, and
aggregate save remain preserved. No new contract, adapter, CMake change, or
unrelated migration was introduced.

Verification passed ownership validation with 831 handwritten sources and a
clean-first serial Debug build. Focused tests passed 4/4 (MyWorkspace,
current-campus adapter, CampusDashboard, and last-selected-campus); MyWorkspace
slots passed 3/3 and current-campus adapter slots passed 6/6. Resource,
dependency, offscreen, static, source, and diff checks passed. Page
missing/unavailable cases are covered through adapter tests and source review;
stored-campus matching and correction are directly tested. Expected notices
were Vulkan/zlib and LF-to-CRLF normalization. Three leftover MSBuild
processes were stopped; no command remains active.

This PersonalDetailsPage current-campus boundary is closed. Phase 2 remains
open; the next boundary is not yet selected.

## Verified typed PersonalDetailsPage display-name read handoff

Against baseline commit `2b58a9c0`, completed the remaining
`PersonalDetailsPage::loadStoredSettings()` display-name read using the
existing typed `PersonalDisplayNamePreferencesPort` and
`ApplicationServicesPersonalDisplayNamePreferencesPort`. UTF-8 is converted
to `QString` with legacy `QVariant::toString()` and whitespace behavior;
missing or unavailable values are empty, and the read path performs no writes.

The exact implementation/test scope is:

- `src/features/my_info/ui/personal_details_page_sections.cpp`
- `tests/my_workspace_page_tests.cpp`

Repository loading remains for Zoom and typed-signature fields, with aggregate
save ownership preserved. Save trimming/validation, campus/signature UI state,
guards, and unrelated writers remain unchanged. No new contract, adapter, CMake
change, or unrelated migration was introduced.

Verification passed ownership validation with 831 handwritten sources and a
clean-first serial Debug build. CTest passed 4/4; 17/17 individual slots
passed. Application, MyWorkspace, display-name, campus, and signature targets
were built. Static, source, resource, dependency, offscreen, and diff checks
all passed. Expected notices were missing Vulkan headers/zlib fallback and
LF-to-CRLF normalization; no concrete failures or active commands remained.

This PersonalDetailsPage display-name boundary is closed. Phase 2 remains open;
the next boundary is not yet selected.

## Verified typed PersonalDetailsPage Zoom-read reuse handoff

Against baseline commit `da4f8c74`, completed the three PersonalDetailsPage
reads for `myInfo/zoomLoginId`, `myInfo/zoomPassword`, and
`myInfo/zoomNotAvailable` using the existing typed
`SubPrepPersonalZoomPreferencesPort` and
`ApplicationServicesSubPrepPersonalZoomPreferencesPort`. Qt-free values,
primary-key precedence, legacy fallback/migration, typed defaults, UTF-8
credentials, N/A masking, unavailable state/field enablement, and
save/normalization behavior remain preserved.

The exact implementation/test scope is:

- `src/features/my_info/ui/personal_details_page_sections.cpp`
- `tests/my_workspace_page_tests.cpp`

Aggregate loading remains for typed-signature fields and aggregate save remains
the owner; unrelated behavior is unchanged. No Zoom writer, new contract,
adapter, CMake change, or unrelated migration was introduced.

Verification passed with a serial clean-first Debug build. MyWorkspace passed
17/17; the typed Zoom adapter passed 7/7 and the SubPrep page passed 14/14.
Resource checks passed 6 RCC packs/7 runtime references; the dependency JSON
was valid at 1042 bytes, and diff checks passed for the exact two-file scope.
No commands remain active. The umbrella focused CTest/offscreen runner was
stopped after stalling; bounded individual gates passed. Expected notices were
offscreen `propagateSizeHints`, pre-existing `/FORCE`/duplicate-symbol linker
warnings, and Vulkan/zlib/LF-to-CRLF notices.

This PersonalDetailsPage Zoom-read boundary is closed. Phase 2 remains open;
the next boundary is not yet selected.

## Verified typed personal-signature-preferences bundle handoff

Against baseline commit `554e106b`, added the Qt-free read-only personal
signature-preferences bundle for exact keys `myInfo/signatureMode`,
`myInfo/typedSignatureText`, and `myInfo/typedSignatureFont`. Defaults are
`Image`, empty text, and font `0`; invalid QVariant mode/font values normalize
to those defaults, text preserves UTF-8/whitespace, unavailable settings return
a failure, and the bundle performs no writes.

The exact seven-file scope is:

- `cmake/next.cmake`
- `cmake/tests/next.cmake`
- `src/next/application/personal_signature_preferences.h`
- `src/next/platform/application_services_personal_signature_preferences_port.h`
- `src/features/my_info/ui/personal_details_page_sections.cpp`
- `tests/next_platform_application_services_personal_signature_preferences_port_tests.cpp`
- `tests/my_workspace_page_tests.cpp`

PersonalDetailsPage maps the typed mode, text, and font to the existing UI
types and removes only the aggregate load from `loadStoredSettings()`. Aggregate
save and all name/campus/Zoom/signature-image/write/guard behavior remain
preserved. CMake registration is included; no unrelated migration was made.

Verification passed ownership validation with 834 handwritten sources and a
serial clean-first Debug build. The new adapter passed 7/7, MyWorkspace passed
18/18, the signature-image adapter passed 6/6, the current-campus adapter
passed 8/8, and the display-name and Zoom adapters passed 7/7 each. Resource
checks passed 6 RCC packs/7 runtime references; dependency JSON was valid at
1042 bytes and the final diff passed. No TIMEOUT or FAIL occurred and no
commands remain active. Executables ran offscreen, so a separate offscreen
launch step was skipped. Expected notices were Qt/Vulkan/zlib and LF-to-CRLF;
stub-test linker notices were also nonblocking.

This personal-signature-preferences boundary is closed. Phase 2 remains open;
the next boundary is not yet selected.

## Verified typed PersonalDetailsPage aggregate atomic writer handoff

Against baseline commit `36773c0b`, added the Qt-free
`PersonalDetailsSaveRequest`/`Result<void>` contract and Qt adapter for exactly
nine keys: `myInfo/name`, `myInfo/campus`, `myInfo/zoomLoginId`,
`myInfo/zoomPassword`, `myInfo/zoomNotAvailable`, `myInfo/signatureImage`,
`myInfo/signatureMode`, `myInfo/typedSignatureText`, and
`myInfo/typedSignatureFont`. The boundary preserves exact UTF-8/whitespace,
image preparation/Base64 encoding, mode/font normalization, one atomic
`saveAll`, unrelated-setting preservation, unavailable-service handling, and
rollback with no partial writes.

The exact seven-file implementation/test scope is:

- `cmake/next.cmake`
- `cmake/tests/next.cmake`
- `src/features/my_info/ui/personal_details_page_sections.cpp`
- `tests/my_workspace_page_tests.cpp`
- `src/next/application/personal_details_save.h`
- `src/next/platform/application_services_personal_details_save_port.h`
- `tests/next_platform_application_services_personal_details_save_port_tests.cpp`

PersonalDetailsPage replaces only the aggregate save. InitialSetupWizard and
repository compatibility ownership, all typed read callers and writers, and
guards remain unchanged. CMake registration is included; no unrelated
migration was made.

Verification recorded the executor focused Debug build and an independent
incremental serial Debug build exiting 0. The save adapter passed 6/6,
MyWorkspace 20/20, signature-image 6/6, current-campus 8/8, display-name 7/7,
and Zoom 7/7. Ownership validation counted 837 handwritten sources; resource
validation passed 6 RCC packs/7 IDs/7 references, dependency JSON was valid at
1042 bytes, and diff checks passed. The independent serial clean-first build
was stopped while compiling without compiler failure; incremental build/tests
passed, and no commands remain active. Expected notices were Vulkan/zlib,
LF-to-CRLF, and stub-linker warnings; no unrelated changes were found.

This PersonalDetailsPage aggregate-writer boundary is closed. Phase 2 remains
open; the next boundary is not yet selected.

## Verified typed InitialSetupWizard aggregate-writer cutover handoff

Against baseline commit `d4dc9e52`,
`InitialSetupWizard::PersonalDetailsWizardPage::validatePage()` now uses the
existing typed `PersonalDetailsSavePort` for all nine keys and values in one
atomic `saveAll`. Repository loading remains for legacy Zoom fallback/migration
and untouched-field preservation.

The exact implementation/test scope is:

- `src/features/setup/ui/initial_setup_wizard.cpp`
- `tests/initial_setup_wizard_tests.cpp`

Trimmed-name validation and warnings, selected/retained signature image,
campus and Zoom fallback/availability, mode/text/font, unrelated settings, and
the existing guards remain preserved. The wizard has no direct settings access;
no new contract, adapter, CMake change, or unrelated migration was introduced.

Configure/ownership checks passed. The executor Debug build passed, and bounded
independent gates passed: InitialSetupWizard 10/10, MyWorkspace 20/20,
PersonalDetailsSavePort 6/6, signature-image 6/6, current-campus 8/8,
display-name 7/7, and Zoom 7/7. Resource validation passed 6 RCC packs/7 IDs/7
references; dependency JSON was valid at 1042 bytes, diff checks passed, and no
test failure or timeout occurred. No commands remain active. Clean-first
encountered environmental MSBuild FileTracker `E_ACCESSDENIED`; bounded gates
passed and no unrelated changes were found. Expected notices were offscreen
Qt/font/plugin, Vulkan/zlib, LF-to-CRLF, and FileTracker notices.

This InitialSetupWizard aggregate-writer boundary is closed. Phase 2 remains
open; the next boundary is not yet selected.

## Verified typed InitialSetupWizard read composition handoff

Against baseline commit `5cf3d9e1`, InitialSetupWizard no longer references
`PersonalDetailsRepository` or accesses settings directly. It composes the
existing typed campus, SubPrep Zoom, signature-image, and
signature-preferences reads into the existing nine-key
`PersonalDetailsSaveRequest` and atomic writer. The name remains trimmed and
UI-owned.

The exact implementation/test scope is:

- `src/features/setup/ui/initial_setup_wizard.cpp`
- `tests/initial_setup_wizard_tests.cpp`

Zoom primary/legacy fallback, migration, and defaults; campus; signature-image
retention/replacement; mode/text/font; unavailable behavior; failure warning;
and unrelated-setting preservation remain unchanged. No new contract, adapter,
CMake change, or unrelated migration was introduced.

Verification passed with an incremental elevated Debug build exiting 0.
InitialSetupWizard passed 11/11, MyWorkspace 20/20, PersonalDetailsSavePort
6/6, campus 8/8, Zoom 7/7, signature-image 6/6, signature-preferences 7/7,
and display-name 7/7. Ownership validation counted 837 handwritten sources;
resource validation passed 6 RCC packs/7 IDs/7 references, dependency JSON was
valid at 1042 bytes, formal Qt-free and call-site checks passed, and the final
diff passed. No test failure, timeout, or active command remained. Expected
notices were `/FORCE`/duplicate-symbol, Vulkan/zlib, and LF-to-CRLF; the
FileTracker environmental qualification remained nonblocking.

This InitialSetupWizard typed-read composition boundary is closed. Phase 2
remains open; the next boundary is not yet selected.

## Verified typed language persistence cutover handoff

Against baseline commit `641f7936` (`Phase2 - Remove InitialSetupWizard
repository reads`), completed the typed language persistence cutover. The
current uncommitted scope is:

- `src/ui/shared/actions/action_registry.cpp`
- `src/ui/shared/state/option_state.h`
- `tests/language_preference_port_tests.cpp`

`OptionState` now exposes the `onPersist` seam. `ActionRegistry` maps legacy
Language `SystemDefault`/`English`/`Korean` to typed
`LanguagePreference` through `SettingsManagerLanguagePreferencesPort`.
Canonical persisted values remain 0/1/5; reads migrate legacy 2/3/4 to
English. Startup selection synchronizes without reapplying presentation
language, and other `OptionState` writers retain compatibility behavior.

Acceptance passed after rebuilding runtime/test targets: an isolated run with a
unique `CLASSMNGR_SETTINGS_ROOT` passed the full `LanguagePreferencePortTests`
8/8, each of the three focused cases passed 3/3, typed adapter,
`LanguageService`, and AI-options regressions passed, and static Qt-free and
call-site checks passed. Earlier shared/alternate-harness 0-vs-1/5/2 failures
were environment/order-dependent; the isolated rerun passed.

## Verified typed SaveMode persistence cutover handoff

Against baseline commit `9ad0fddb` (`Phase2 - Cut ActionRegistry language
persistence over`), completed the typed SaveMode persistence cutover. The
current scope is:

- `src/next/application/save_mode_preferences.h`
- `src/next/platform/settings_manager_save_mode_preferences_port.h`
- `src/ui/shared/actions/action_registry.cpp`
- `tests/next_platform_settings_manager_save_mode_preferences_port_tests.cpp`
- `tests/ai_comment_options_tests.cpp`

The `SaveModePreferencesPort` write contract uses canonical `options/saveMode`
values 0=`Automatic` and 1=`Manual`. `ActionRegistry` installs its `onPersist`
cutover before startup selection, preserving `onChanged`/autosave behavior;
malformed or unknown reads fall back to `Automatic`, and other `OptionState`
writers remain unaffected.

An independent elevated Debug rebuild passed after a non-elevated VS FileTracker
access-denied retry. Configure/ownership passed with 837 handwritten sources;
the adapter passed 6/6, AI/ActionRegistry 10/10, and language 8/8. Static
Qt-free, call-site, ownership, and diff checks passed, with each test using a
unique temporary `CLASSMNGR_SETTINGS_ROOT`.

## Verified typed AI-comment voice persistence slice handoff

Against baseline commit `fe29d1cb` (`Phase2 - Cut ActionRegistry SaveMode
persistence over`), added the typed AI-comment voice write contract. The
current five-file scope is:

- `src/next/application/ai_comment_voice_preferences.h`
- `src/next/platform/settings_manager_ai_comment_voice_preferences_port.h`
- `src/ui/shared/actions/action_registry.cpp`
- `tests/next_platform_settings_manager_ai_comment_voice_preferences_port_tests.cpp`
- `tests/ai_comment_options_tests.cpp`

Canonical `options/aiCommentVoice` values are 0=`DirectToStudent` and
1=`ThirdPerson`. `ActionRegistry` installs `onPersist` before startup
selection; unknown or missing reads fall back to `DirectToStudent`, while
reload and direct persistence retain canonical values. Other option writers and
UI behavior remain unchanged; no CMake changes were made.

An independent elevated Debug rebuild passed. The voice adapter passed 7/7,
AI/ActionRegistry 10/10, language adapter and regression 8/8 each, and
SaveMode adapter 6/6. Static Qt-free, call-site, ownership, and diff checks
passed; every test used a fresh temporary `CLASSMNGR_SETTINGS_ROOT`.

## Verified typed AI-comment provider persistence slice handoff

Against baseline commit `c0281217` (`Phase2 - Cut ActionRegistry AI voice
persistence over`), added the typed AI-comment provider write contract. The
current five-file scope is:

- `src/next/application/ai_comment_provider_preferences.h`
- `src/next/platform/settings_manager_ai_comment_provider_preferences_port.h`
- `src/ui/shared/actions/action_registry.cpp`
- `tests/next_platform_settings_manager_ai_comment_provider_preferences_port_tests.cpp`
- `tests/ai_comment_options_tests.cpp`

Canonical `options/aiCommentProvider` values are 0=`ChatGPT`, 1=`Gemini`,
2=`Claude`, 3=`MicrosoftCopilot`, and 4=`CustomWebsite`. `ActionRegistry`
installs `onPersist` before startup selection; unknown or missing reads fall
back to `ChatGPT`, while reload and direct persistence retain canonical values.
Custom URL behavior and other option writers remain unchanged; no CMake changes
were made.

An independent elevated Debug rebuild passed. The provider adapter passed 7/7,
AI/ActionRegistry 10/10, voice 7/7, language adapter and regression 8/8 each,
and SaveMode 6/6. Static Qt-free, call-site, ownership, and diff checks passed;
every test used a fresh temporary `CLASSMNGR_SETTINGS_ROOT`.

## Verified typed document-viewer background persistence handoff

Against baseline commit `2d7d4a29` (`Phase2 - Cut ActionRegistry AI provider
persistence over`), added the typed document-viewer background write contract.
The five-file scope is:

- `src/next/application/document_viewer_background_preferences.h`
- `src/next/platform/settings_manager_document_viewer_background_preferences_port.h`
- `src/ui/shared/actions/action_registry.cpp`
- `tests/next_platform_settings_manager_document_viewer_background_preferences_port_tests.cpp`
- `tests/ai_comment_options_tests.cpp`

Canonical `options/documentViewerBackground` values are 0=`Default`,
1=`White`, and 2=`Black`. `ActionRegistry` installs `onPersist` before startup
selection; malformed or unknown reads fall back to `Default`. MainWindow
`onChanged` propagation and PDF viewer behavior remain intact; no CMake changes
were needed.

The focused Debug rebuild passed after retrying a Visual Studio FileTracker
access-denied failure with elevated access. A serial CTest run passed all 9
selected targets, including the background adapter, AI/ActionRegistry,
PageManager, StartupVisualSettings, and provider, voice, language, and SaveMode
regressions. CMake ownership validation passed with 837 handwritten sources;
Qt-free, call-site, source-path, and diff checks passed. Tests used an isolated
`CLASSMNGR_SETTINGS_ROOT`.

## Verified typed document-page-spacing persistence handoff

Against baseline commit `790082c4` (`Phase2 - Cut ActionRegistry viewer
background persistence over`), completed the typed document-page-spacing
persistence cutover for `OptionKeys::DocumentPageSpacing ==
"options/documentPageSpacing"`. The Qt-free
`DocumentPageSpacingPreferencesPort` now exposes `write()`; the
SettingsManager adapter stores valid `None`, `Small`, `Medium`, and `Large`
values as `0`, `1`, `2`, and `3`, and rejects invalid typed values without
changing storage. Existing reads retain compatibility: missing or unknown
values map to `Small`, while malformed text uses unchecked
`QVariant::toInt()` and maps to `None`.

`ActionRegistry` installs typed persistence before initial state selection.
MainWindow/PageManager update propagation remains unchanged. The source/test
scope is limited to:

- `src/next/application/document_page_spacing_preferences.h`
- `src/next/platform/settings_manager_document_page_spacing_preferences_port.h`
- `src/ui/shared/actions/action_registry.cpp`
- `tests/next_platform_settings_manager_document_page_spacing_preferences_port_tests.cpp`
- `tests/ai_comment_options_tests.cpp`

No CMake changes were made.

Independent review, build, and serial CTest passed for
`ClassMngrNextPlatformSettingsManagerDocumentPageSpacingPreferencesPortTests`,
`ClassMngrAiCommentOptionsTests`, `ClassMngrPageManagerTests`, and
`ClassMngrStartupVisualSettingsTests`; `git diff --check` was clean. The typed
document-page-spacing persistence seam is closed; generic settings and other
Phase 2 migrations remain open.

## Verified typed font-size persistence cutover handoff

Completed the typed `FontSize` persistence cutover for
`OptionKeys::FontSize == "options/fontSize"` in the current Phase 2 working
tree. The Qt-free `FontSizePreferencesPort` now exposes `write()`; the
SettingsManager adapter writes only `Small=-2`, `Normal=0`, `Large=2`, and
`ExtraLarge=4`, and ignores invalid typed values without changing storage.
Missing, unknown, and malformed reads continue to map to `Normal`.

`ActionRegistry` installs typed `onPersist` before startup selection and the
direct raw write is removed. `FontSizeController` `onChanged` behavior remains
unchanged. The exact production/test scope is:

- `src/next/application/font_size_preferences.h`
- `src/next/platform/settings_manager_font_size_preferences_port.h`
- `src/ui/shared/actions/action_registry.cpp`
- `tests/next_platform_settings_manager_font_size_preferences_port_tests.cpp`
- `tests/ai_comment_options_tests.cpp`

Independent source review passed. The targeted Debug rebuild succeeded after
the normal MSBuild FileTracker `E_ACCESSDENIED` retry with elevated access;
registered CTest targets passed: `ClassMngrNextPlatformSettingsManagerFontSizePreferencesPortTests`,
`ClassMngrAiCommentOptionsTests`, `ClassMngrStartupVisualSettingsTests`, and
`ClassMngrFontManagerTests`. No CMake changes were made; `git diff --check`
was clean. The typed FontSize persistence seam is closed; Phase 2 remains
open.

## Verified Phase 2 Sub Prep schedule-summary query contract

The current legacy summary read is
[`SubPrepPage::buildClassInformation(schedule)`](../../src/features/sub_prep/ui/sub_prep_page_class_information.cpp#L688): it reads all classes, then calls `classInfo(id)` and `studentCount(id)` per class and `teacher(id)` per class with a teacher, before `SubPrepClassInformation::build` filters to visible class IDs, days, and schedule mode. The equivalent v2 Application boundary is `SubPrepScheduleScopeRequest` → `SubPrepScheduleSummaryQuery` → `ClassSummaryProjection`, through the injected `SubPrepScheduleSummaryReadPort` in [`sub_prep_schedule_summary_query.h`](../../src/next/application/sub_prep_schedule_summary_query.h). The read adapter must apply the typed scope and return compact copied summary inputs; the query validates a complete projection and sorts deterministically.

This is a contract mapping only. The legacy page and package path still use the current services; no adapter, page cutover, SQL batching, or output migration is implemented. The app-less test is [`next_application_sub_prep_schedule_summary_query_tests.cpp`](../../tests/next_application_sub_prep_schedule_summary_query_tests.cpp), registered in [`next.cmake`](../../cmake/tests/next.cmake). Release ownership validation covered 701 handwritten sources, `ClassMngrNext` built, and `ctest -R ClassMngrNextApplicationSubPrepScheduleSummaryQueryTests --output-on-failure` passed 1/1. The full Sub Prep feature and memory gate remain open; see the [Phase 2 plan](03-Phase-2-Domain-Model-and-Application-Contracts.md) and [Sub Prep memory plan](sub-prep-class-information-memory-plan.md).

## Verified Sub Prep selected-details and selection-state contracts

Commit `389d90a6a433ae6c5c7ce7263daba02f4a27a5ce` adds
[`SubPrepClassDetailsQuery`](../../src/next/application/sub_prep_class_details_query.h)
for one selected-class detail value, with an injected read port, structured
failures, returned-class validation, and support for a missing teacher.
`SubPrepClassInformationState`, added in commit `7959eb07`, owns the value
lifecycle for scope refresh, selection, details, and clear: it retains only a
visible selection and accepts details only when class and teacher identities
match. Details are cleared on each successful refresh and on selection change.

The focused app-less targets
[`ClassMngrNextApplicationSubPrepClassDetailsQueryTests`](../../tests/next_application_sub_prep_class_details_query_tests.cpp)
and
[`ClassMngrNextApplicationSubPrepClassInformationStateTests`](../../tests/next_application_sub_prep_class_information_state_tests.cpp)
passed 1/1 each; both are registered in [`next.cmake`](../../cmake/tests/next.cmake).
These contracts are not connected to legacy UI or persistence. No batching,
package/PDF migration, or memory reduction is claimed. The next boundary is
the operation-scoped print-source contract; package/roster/PDF and Release
memory gates remain later work.

## Verified Phase 2 Sub Prep operation-scoped print-source contract

[`SubPrepPrintSourceRequest`, `SubPrepPrintSourceReadPort`, and
`SubPrepPrintSourceQuery`](../../src/next/application/sub_prep_print_source_query.h)
define the Qt-free information-sheet source boundary for selected class IDs,
weekdays, and regular/intensive mode. The injected port returns copied values
in stable order; the query validates request scope and bounded source records,
propagates structured read errors, and returns one owned source or an error.
Teacher records are stored once and classes refer to them by typed ID. An
empty class/day scope performs no read; returned classes may be a subset and
may omit a teacher.

The app-less target
[`ClassMngrNextApplicationSubPrepPrintSourceQueryTests`](../../tests/next_application_sub_prep_print_source_query_tests.cpp)
is registered in [`next.cmake`](../../cmake/tests/next.cmake); its focused
target build passed and CTest passed 1/1. This contract is not wired to a Sub
Prep adapter, page, or PDF renderer. SQL batching, source-release or memory
improvement, roster/package migration, and output parity remain unverified;
Phase 2 remains open.

## Verified custom-color palette caller boundary

Commit `83163b0a` moves [`ColorUtils`](../../src/core/utils/colorutils.h)
palette load/save behind the existing
[`CustomColorPalettePreferencesPort`](../../src/next/application/custom_color_palette_preferences.h);
the utility no longer depends on `SettingsService` or Platform. Each of the
seven UI picker call sites passes the existing adapter from
[`application_services_custom_color_palette_preferences_port.h`](../../src/next/platform/application_services_custom_color_palette_preferences_port.h).

The two focused Windows x64 Ninja targets built and CTest passed 2/2. The
offscreen QtTest [`ColorUtilsCustomColorPaletteTests`](../../tests/colorutils_custom_color_palette_tests.cpp)
covers all 16 Qt dialog slots, canonical writes, and slot restoration; the
[`NextPlatformApplicationServicesCustomColorPalettePreferencesPortTests`](../../tests/next_platform_application_services_custom_color_palette_preferences_port_tests.cpp)
covers null-service defaults and no-op writes. This closes only the caller
seam. Generic settings persistence, other UI-service boundaries, Phase 3
persistence work, and Phase 7 feature migration remain open; no stored-format
change is claimed.

## Verified calendar import planning contract

Commit `d5a5cab9` extracts duplicate planning from
[`CalendarEventImportService::handleFinished`](../../src/features/calendar/calendar_event_import_service.cpp)
into the Qt-free
[`CalendarEventImportPlan`](../../src/next/application/calendar_event_import_plan.h).
After the service's legacy range read, it supplies exact UTF-16 keys derived
from the existing six-field signature. The planner skips keys already present
or repeated among candidates, returns accepted indices in stable input order,
and carries forward the parser's skipped count. The service maps those indices
back to event values and retains its existing batch save, error, metric, and
signal paths.

The app-less
[`NextApplicationCalendarEventImportPlanTests`](../../tests/next_application_calendar_event_import_plan_tests.cpp)
and [`CalendarImportTests`](../../tests/calendar_import_tests.cpp) cover
duplicate planning, ordering/counts, signature fields, normalization, and
exact UTF-16 comparison, including distinct lone-surrogate keys. Both Windows
x64 Ninja targets built and focused CTest passed 2/2. Range retrieval and
batch persistence remain legacy responsibilities; no broader calendar-import
migration is claimed. The existing-signature read cutover is recorded below;
batch persistence remains a legacy responsibility. The general projection's
4,096-row cap and stricter metadata checks would change this import caller's
legacy behavior, so its read port returns exact signature keys.

## Historical calendar import concrete existing-signature read

Commit `d14155c1` adds
[`ApplicationServicesCalendarEventPort::importSignatureKeysInRange`](../../src/next/platform/application_services_calendar_event_port.h)
and uses it for existing-event duplicate detection. It reads the importer's
same date range through `CalendarService::eventsInRange`, preserves event
order, and returns UTF-16 keys matching
`CalendarImport::calendarEventImportSignature`. The dedicated read bypasses
the general 4,096-row projection and its stricter metadata checks. Candidate
parsing and batch save remain in the legacy import service.

`ClassMngr` and the focused calendar-event port target built on Windows x64
Ninja; focused CTest passed 1/1 and `git diff --check` passed. This was the
initial concrete Platform read seam. The current Application query boundary
supersedes it; see the entry below. Sub Prep print-source adapter/query
integration was the next slice at that time; its production adapter, page, and
PDF wiring were still unverified then.

## Verified Sub Prep print-source Platform read adapter

Commit `2daae4ef` adds
[`ApplicationServicesSubPrepPrintSourcePort`](../../src/next/platform/application_services_sub_prep_print_source_port.h)
and routes its selected class/day/mode read through
`ClassService::classInfosForScheduleScope` to
`ClassInfoRepository::loadClassInfosForScheduleScope`. The repository filters
the requested IDs, selected days, and one schedule mode in SQL before
materializing records. It validates positive unique integer IDs, uses decimal
integer literals for the at-most-4,096-ID scope to stay below SQLite's bind
limit, and uses per-class and aggregate limit-plus-one sentinels to detect
overflow. This scoped class-info read requires the active repository session;
it has no `DataService` fallback.

The Platform adapter emits owning values in request order and stores each
referenced teacher once. Classes lacking a selected meeting, class-info row,
assigned teacher, or existing teacher are omitted, matching the legacy
print-source builder. Roster lookup errors preserve its zero-count fallback.
The adapter test also covers selected-mode-only reads, noncanonical typed-ID
aliases, per-class and aggregate overflow, repeated and orphan teacher
assignments, and roster failure.

`ClassMngr`,
`ClassMngrNextPlatformApplicationServicesSubPrepPrintSourcePortTests`, and
`ClassMngrNextApplicationSubPrepPrintSourceQueryTests` built on Windows x64
Ninja. Focused CTest passed both test targets and `git diff --check` passed.
This closes only the selected-scope source-read seam: there is no page/PDF
wiring, output-parity result, teacher/roster batching, Release memory
acceptance, or full Sub Prep completion. A selected-class details Platform
read adapter is now available; see the next section. It too is unconnected to
the page.

## Verified Sub Prep selected-class details Platform read adapter

[`ApplicationServicesSubPrepClassDetailsPort`](../../src/next/platform/application_services_sub_prep_class_details_port.h)
implements the injected read for
[`SubPrepClassDetailsQuery`](../../src/next/application/sub_prep_class_details_query.h).
The bounded Application value carries class notes, the preferred teacher
display name, teacher notes, and separate bounded room, WiFi name, WiFi
password, internet type, Zoom ID, Zoom password, and projection type fields.

The adapter reads one class through the active `ClassService` repository
session. It does not query regular or intensive schedule tables and has no
`DataService` fallback. Existing classes without a `class_info` row return
blank details; an absent class returns `NotFound`. Unassigned, missing, and
stale teacher references return empty teacher values. The repository read is
scoped to the selected class and projects only the details required by this
contract.

The Windows x64 Debug build succeeded. Focused
`NextApplicationClassSummary`, `NextApplicationSubPrepClassDetailsQuery`, and
`NextPlatformApplicationServicesSubPrepPrintSourcePort` suites passed. This is
a read-adapter seam only: it is not wired to a page or output path, and it
establishes no full legacy-output parity, memory improvement, or broader
acceptance. The scoped schedule-summary persistence read was completed in
commit `dd429838`; see the following section. Page/output integration, parity,
and large-workspace Release memory evidence remain open. Phase 2 remains In
Progress.

## Verified Sub Prep scoped schedule-summary Platform read

Commit `dd429838` adds
[`ApplicationServicesSubPrepScheduleSummaryPort`](../../src/next/platform/application_services_sub_prep_schedule_summary_port.h)
for `SubPrepScheduleSummaryQuery`. The read is scoped by visible class IDs,
selected days, and regular or intensive mode. It uses the active `ClassService`
repository session and returns bounded owning class and teacher summary values
in request order. Teacher values are shared by ID, and roster counts are read
in a batch; roster-query failure retains the zero-count fallback. Empty class
or day scopes return successfully without reading, and there is no
`DataService` fallback. The intensive-mode test succeeds with `class_times`
dropped, confirming isolation from the regular schedule table.

The Windows Debug build of
`ClassMngrNextPlatformApplicationServicesSubPrepPrintSourcePortTests`
succeeded. Focused CTest passed
`ClassMngrNextApplicationClassSummaryTests`,
`ClassMngrNextApplicationSubPrepScheduleSummaryQueryTests`, and
`ClassMngrNextPlatformApplicationServicesSubPrepPrintSourcePortTests` 3/3 in
13.39 seconds. Configure validated 857 handwritten files and
`git diff --check` passed. This completes the scoped Application read seam,
not Sub Prep or Phase 2 acceptance. Work Package D is the next Sub Prep
increment: model-backed class list/navigation and a reusable selected-class
detail view. Page/output wiring, behavior parity, and 96-class Release memory
evidence remain open.

## Verified Sub Prep model-backed class-information view

The live class-information view now consumes the Application schedule-summary
and selected-class details queries through the corresponding Platform read
ports. A `QAbstractListModel` owns the compact projection and supplies grade,
configured-level, teacher, meeting, and student-count roles to one `QListView`.
One reusable details card is updated from `SubPrepClassInformationState`; its
selected typed class ID is not recovered from per-class widgets. Schedule
display-mode changes refresh the projection in place, and refresh retains the
selection only while the class remains in the current schedule scope.

Windows x64 Debug Ninja built `ClassMngr`, the list-model and Sub Prep page
test targets, and the startup-performance test target. Focused CTest passed
the list-model and page suites 2/2; CMake validated 860 handwritten files and
`git diff --check` passed. The packaged 96-class Release route was not run.
This closes the live summary/details view wiring only. Page-leave release,
package/PDF output migration and parity, and Release memory acceptance remain
open; Phase 2 remains in progress. Work Package E covers explicit lifecycle
release and invalidation.

## Verified Sub Prep page-leave lifecycle release

`SubPrepPage::releaseFeatureResources()` runs through
`BasePage::deactivate()`, which `PageManager` calls when leaving the page. It
clears the selected detail state and summary projection while preserving the
bounded static view skeleton, then marks the page stale. The next activation
reloads the schedule projection and the current selected detail.

Windows x64 Debug Ninja built `ClassMngr` and `ClassMngrSubPrepPageTests`;
focused CTest passed 1/1. The lifecycle test verifies release before
navigation returns and a fresh details read after reactivation. Package/PDF
output migration, parity, and packaged Release memory acceptance remain open;
Work Package F connects the print-source query to package generation.

## Verified Sub Prep information-sheet print-source integration

`SubPrepPage::generateSubPrep()` now sends the accepted dialog's selected
class IDs, weekdays, and current schedule mode through
`SubPrepPrintSourceQuery`. `SubPrepPrintSourceMapper` converts the owning
result into the existing renderer model. The Application teacher value
preserves English name, Korean name, preferred name, and preferred
romanization, retaining the legacy display-name fallback order and all facts
used by the information sheet. The former all-class/class-info/teacher/roster
count loader has been removed from this output path.

Windows x64 Debug Ninja built the application and focused mapper, page,
Application query, Platform adapter, PDF, and package targets. CTest passed
6/6. At this milestone the separate roster-PDF stage still loaded legacy
class, teacher, and full roster records; Work Package F7 later replaced those
direct package-service reads with the typed source below. The mapper and
existing output tests establish this boundary, not full generated package
parity or a memory improvement; those Phase 2 and Sub Prep gates remain open.

## Verified Sub Prep information-sheet renderer lifetime

`SubPrepDocumentModel::Document` now borrows `Request::classInformation` by
const reference wrapper instead of copying the rich `TeacherGroup` list.
The page moves the print request into `SubPrepPackageService::Request`,
avoiding another copy at the ownership handoff.
`SubPrepPrintService::saveSubPrepPdf()` keeps the request alive through the
synchronous renderer call, and the PDF test verifies that both values refer
to the same list. Windows x64 Debug Ninja built the application, PDF, package,
and page targets; CTest passed the page, PDF, and package suites 3/3.

At this point the package request still retained the information model after
the main sheet finished, and roster output still used full legacy class,
teacher, and roster records. The subsequent package-stage release dropped the
information model before roster materialization; Work Package F7 later moved
the roster source read behind the typed boundary below. This renderer change
was one copy reduction, not full output parity or memory evidence; Phase 2 and
the Release memory gate remain open.

## Verified Sub Prep package stage release

`SubPrepPackageService::generate()` now takes ownership of its request by
value; `SubPrepPage` moves the operation request into the service. The package
service writes `Sub Prep.pdf` before loading package classes and full roster
data, then clears `request.subPrep` before the roster output stage. This drops
the schedule and rich information-sheet model before roster materialization.

Windows x64 Debug Ninja built the application, package service tests, and page
tests; CTest passed the page, PDF, and package suites 3/3. At this stage the
package service still read legacy class, teacher, and full roster records
directly. Work Package F7 later replaced those direct reads with the typed
roster source below. This closes the main-sheet lifetime boundary only; output
parity and Phase 2 remain open.

## Current Sub Prep roster-output source boundary - 2026-09-24

Commit `bc930be919a47cdeefa4b131cad3df92e96534b2` integrates
`SubPrepRosterOutputSourceQuery` and
`ApplicationServicesSubPrepRosterOutputSourcePort` into the package path.
`SubPrepPage` owns the session-backed Platform adapter for the synchronous
generation call; `SubPrepPackageService` maps the bounded Application result
to the renderer model and no longer reads classes, teachers, or rosters through
legacy services directly. The request carries typed class IDs, dates, schedule
mode, and extra columns. Existing ordering, package tree, folder names, and
output selection remain preserved.

Focused CTest passed 5/5 across package, page, PDF, Application query, and
Platform source suites. Full PDF/package output parity and the packaged
96-class Release memory gate remain open; Phase 2 is not complete.


## Current calendar import persistence boundary - 2026-09-24

Accepted candidates from the existing-signature planner now pass through the
Qt-free `CalendarEventImportSaveRequest` and
`ApplicationServicesCalendarEventImportSavePort`. The adapter preserves one
ordered `CalendarService::saveEvents()` transaction. F36 verifies the live
service path through this batch-save boundary; see the handoff below and the
Phase 2 plan's
[typed calendar import batch-save update](03-Phase-2-Domain-Model-and-Application-Contracts.md#progress-update---2026-09-24-typed-calendar-import-batch-save-boundary)
for adapter details. Workbook parsing and campus-directory lookup remain
legacy.


## Current calendar reset mutation boundary - 2026-09-24

`CalendarPreferencesPanel::resetCalendarEvents()` now checks
`Application::CalendarEventDeleteAllPort::isAvailable()` before opening its
destructive prompt and routes the confirmed delete through
`Platform::ApplicationServicesCalendarEventDeleteAllPort`. The panel no
longer retains `CalendarService`; it still owns the existing prompt text,
warning, success status, and `calendarPreferencesChanged(true)` notification.
Focused Application and Platform tests cover the Qt-free result contract,
successful reset, unavailable service, and a database delete failure.
Workbook parsing, campus-directory lookup, and other feature-service calls
remain mapped for later Phase 2 slices.


## Calendar availability boundary - 2026-09-24

Calendar import start and calendar dialog opening now use
`ApplicationServicesCalendarEventPort::isAvailable()`. The Platform adapter
owns the service lookup and converts exceptions to the existing unavailable
path. Neither `CalendarEventImportService` nor the calendar page helper stores
or dereferences `CalendarService`; a source search found no direct
`calendarService()` call below `src/features/calendar/`. The calendar import
query and save operations remain on their structured Platform result paths.


## Calendar page edit-draft boundary - 2026-09-24

Calendar event activation already obtains a typed `CalendarEventSummary` from
`ApplicationServicesCalendarEventPort`; the page now copies it directly to
`Application::CalendarEventEditDraft`. New day activation builds that same
draft shape. The dialog and typed mutation ports consume the draft without an
intermediate legacy `CalendarEvent` record. The legacy list-based upcoming
filter overload was unused and has been removed; calendar summary filtering
continues on the typed projection values.


## Calendar display-preference caller boundary - 2026-09-24

`CalendarPreferencesPanel` keeps `ApplicationServices*` and now passes it to
`ApplicationServicesCalendarEventDisplayPreferencesPort`. It no longer
captures `SettingsService*` for the event-display setting pair. The Platform
adapter retains the exact keys, default-false reads, unavailable-save no-op,
and atomic save semantics; unrelated preferences remain untouched.


## Academic calendar preference-port ownership - 2026-09-24

AcademicCalendarProvider now owns injected Application schedule and
first-day preference ports. CalendarPage and evaluation-default selection
construct their Platform adapters at the boundary. Platform retains
SettingsService access; the provider no longer stores it or constructs
adapters during reads and writes. Upcoming-events preference access and
broader settings persistence remain open.


## Calendar event-type color preference caller - 2026-09-24

Calendar event-type color reads and writes construct the Platform adapter
from ApplicationServices*. The adapter owns missing or unavailable settings
behavior: the caller receives no stored color and applies its current default,
while a save safely does nothing. Current-campus options and remaining
upcoming-events preferences remain separate legacy-access slices.


## Current calendar import signature-query and identity boundary - 2026-09-25

Existing-event signature lookup uses the Qt-free
`Application::CalendarEventImportSignatureQueryPort` and dedicated
`Platform::ApplicationServicesCalendarEventImportSignatureQueryPort`; the
former `ApplicationServicesCalendarEventPort` method has been removed. F50
adds the shared Qt-free `Application::CalendarEventImportSignature` value used
by the parser and query port. It preserves the six-field legacy key's order,
delimiters, exact UTF-16 code units, and all-day flag; title simplification,
type/time-status normalization, and ISO date conversion remain in Qt adapters.
The key excludes times, database ID, and repeat-series ID. The read port retains
result order, availability/failure behavior, and access beyond the general
projection cap. See the Phase 2 plan's
[F50 signature-identity update](03-Phase-2-Domain-Model-and-Application-Contracts.md#progress-update---2026-09-25-f50-calendar-import-signature-identity).


## Personal display-name caller boundary - 2026-09-24

Schedule output, schedule import, and Sub Prep print-dialog callers now create
`ApplicationServicesPersonalDisplayNamePreferencesPort` from
`ApplicationServices&`. Schedule output reads after acceptance and preserves
stored whitespace; the import and Sub Prep callers trim. In Sub Prep, the name
write follows folder selection and replacement confirmation but precedes
dialog acceptance. Package generation later calls `QDir::mkpath`, so a later
filesystem failure does not undo that preference write; the write does not
depend on successful directory creation. Null services retain empty/default
names, unavailable reads are empty, and unavailable writes are no-ops. My
Information and Initial Setup now also construct the adapter from
`ApplicationServices&`; the `SettingsService*` constructor and its
constructor-only test are removed. My Information retains its availability
guard and aggregate personal-details save. Initial Setup retains its
availability guard, fills only a blank name field, and uses the aggregate save
path. The Setup prefilled-name reinitialization case has no direct assertion,
though the fill-only-if-blank source condition remains. See the [verified F20
migration and verification limits](03-Phase-2-Domain-Model-and-Application-Contracts.md#progress-update---2026-09-24-f20-my-information-and-initial-setup-migration).

## Verified F21 unused class-navigation preferences cleanup

The unused `ClassNavigationPreferences` header and implementation are removed,
along with their production source manifest entry, the Classes Page test source
entry, and stale includes. `speaking_eval_page_p.h` includes
`class_tab_navigation_model.h` directly for `ClassTabNavigation`. The typed
Application contracts, Platform adapters, and preference tests remain active.

Independent fresh Windows x64 Ninja/MSVC verification configured and built
376+8 steps. CTest passed 8/8 across ClassesPage, ClassTabNavigation, evaluation
defaults, and the five typed preference suites. CMake validated 872 source
owners; searches found no deleted API or file references in `src`, `tests`,
`cmake`, `CMakeLists.txt`, or `compile_commands`; `git diff --check` passed.

## Verified F22 calendar-import campus-code query

Calendar import obtains campus codes through the Qt-free
`Application::CalendarEventImportCampusCodeQueryPort`, implemented by the
Platform `CalendarEventImportCampusCodeQueryAdapter`. The adapter owns
`ResourcePaths` and `CampusJsonRepository` access and supports injected test
directories; `CalendarEventImportService` has no direct resource-path or
repository lookup. The Application result is
`std::vector<std::string>`.

The adapter retains repository campus-name ordering, trims values, removes
blanks, and preserves the first exact duplicate. It skips default,
malformed, and unreadable records and returns no codes for a missing or empty
directory. A fixture verifies actual Korean UTF-8. Workbook parsing, parser
behavior, and CalendarPage behavior remain unchanged.

Independent fresh Windows x64 Ninja/MSVC Debug configure and full 356-step
build passed. CMake validated 875 source owners; parser and adapter CTest
passed 2/2; source/dependency checks and `git diff --check` passed. This is
only the importer's code-list read seam.

## Verified F23 CalendarPage campus-directory query

CalendarPage campus metadata now uses the Qt-free
`Application::CalendarPageCampusDirectoryQueryPort` and a Platform adapter
backed by `CampusJsonRepository` with an injected directory for tests. The
query returns owning UTF-8 IDs and names with an optional code. CalendarPage no
longer reads `CampusJsonRepository` or `ResourcePaths` directly; F22's
importer-specific campus-code-list port remains separate.

Source comparison confirmed the existing availability branch and timing,
repository order, ID/name/code alias order, case-insensitive ID/name matching,
trimmed display-name fallback, whitespace-only codes, exact-empty removal,
and deduplication. Adapter tests cover Unicode and missing, empty, whitespace,
blank, malformed, and default records, plus empty and missing directories.
Executor and independent fresh Windows x64 Ninja/MSVC configure/builds
validated 878 handwritten owners and each passed focused CTest 3/3 for the
F23 adapter, F22 adapter, and CalendarEventCache. No dedicated CalendarPage
test exists. This closes only the campus-directory migration seam; it does not
complete calendar/workbook migration or Phase 2.

## Verified F24 personal signature-image caller cutover

`ApplicationServicesPersonalSignatureImagePort` retains its
`ApplicationServices&` constructor, adds a nullable `ApplicationServices*`
constructor, and removes the `SettingsService*` constructor. The five reads in
Initial Setup (two), My Information (one), and Speaking Eval (two) now pass
`setup->services()` or `m_services` through the Application adapter. The
`myInfo/signatureImage` key, Base64 conversion, one-time
`SignatureImage::prepareForEmbedding`, read-only behavior, empty results, and
existing availability guards remain unchanged.

Executor verification reused the Ninja/MSVC x64 configure, validated 878
handwritten owners, built `ClassMngr` and the adapter, Initial Setup, and
MyWorkspace targets, and passed focused CTest 3/3; source scan and
`git diff --check` were clean. Independent fresh configure/build also validated
878 owners and compiled all targets. Its focused CTest passed 2/3: adapter and
Initial Setup passed, while three MyWorkspace PageManager cases failed because
the `documents` and `campuses` resource packs were unavailable. Running all 18
MyWorkspace functions individually confirmed the F24 image preview,
missing/corrupt/unavailable image, display-name, and aggregate-save cases
passed; only those same three resource-dependent cases failed. This is a
fresh-tree limitation outside the F24 migration scope.

## Verified F25 custom-color adapter constructor cleanup

The custom-color adapter retains its `ApplicationServices&` constructor, adds
a nullable `ApplicationServices*` constructor, and removes `SettingsService*`.
Seven callers in five UI files now pass their `ApplicationServices*`:
`src/features/schedule/ui/schedule_editor_dialog.cpp` (two),
`src/features/classes/ui/testing_classes_page.cpp` (two),
`src/features/schedule/ui/schedule_import_review_dialog.cpp`,
`src/ui/shared/widgets/sections/class_details_section.cpp`, and
`src/features/setup/ui/initial_setup_wizard.cpp`. The `custom_colors` key, 16
palette slots, stored payload formats, defaults, unrelated settings, and getColor
load-before/save-after/cancel behavior remain unchanged. No ColorUtils logic
changed.

Executor fresh Ninja/MSVC x64 configure validated 878 handwritten owners; its
382-step build covered `ClassMngr`, adapter and ColorUtils tests, Initial Setup,
Testing Classes, Schedule Import Review, and ScheduleWidget targets. Focused
CTest passed 6/6. Independent fresh configure validated 878 owners; the repeat
Ninja build returned exit 0 with no work, and the same six focused suites passed
6/6. Source scan found exactly seven callers and no `SettingsService*`
constructor or call; `git diff --check HEAD` passed. Schedule Editor and Class
Details have no picker-specific tests, though their translation units compiled
through `ClassMngr`.

## Verified F26 Sub Prep typed settings-gate removal

Removed `openSettingsService` from Sub Prep. Saved-content and Zoom preference
paths now use their existing typed ports with `ApplicationServices`; the
nullable current-campus port checks availability before campus loading or
mutation. The save path returns before stopping autosave or restoring grading
when settings are unavailable. Four original preference paths and keys,
atomic save, grading default, Zoom primary/legacy fallback and best-effort
migration, and campus match/fallback are preserved. The full campus-detail
lookup and all-years calendar read were not changed.

Page tests verify that unavailable loading preserves sentinel fields and
unavailable save preserves page values, dirty state, timer, blank grading, and
stored settings. The test stub defaults to database-open; tests explicitly set
it false before constructing an unavailable fixture and never close its fake
service. Executor and independent fresh Ninja/MSVC x64 configures each
validated 878 handwritten owners, built `ClassMngr`, the page, and three
adapter targets, and passed focused CTest 4/4. The independent repeat build
returned exit 0 with no work; diff checks were clean. There are no remaining
verification gaps in this slice.

## Verified F27 My Information campus-directory query

My Information campus chooser metadata now crosses the Qt-free
`MyInfoCampusDirectoryQueryPort`; a Platform adapter reads
`CampusJsonRepository` and returns owning UTF-8 IDs and display names. It
preserves repository order, trimmed name/ID fallback, and raw IDs. The page no
longer reads `CampusJsonRepository` or `ResourcePaths` directly; stored
ID/name matching and correction writes remain unchanged.

Two new Application/Platform suites and the existing MyWorkspace behavior test
cover the cutover. Executor and independent fresh Ninja/MSVC x64 configures
validated 882 handwritten owners, built `ClassMngr`, MyWorkspace, and both new
suites, and passed focused CTest 3/3. The independent repeat build returned
exit 0 with no work; diff check passed. `CampusJsonCodec` normalizes blank
ID/name to `campus`, so an empty-display fixture cannot be reached through the
repository; the empty-display filter remains in place. Phase 2 remains open.

## Verified F28 Sub Prep campus-detail directory query

Sub Prep campus details now cross the Qt-free
`SubPrepCampusDirectoryQueryPort`; its Platform adapter wraps
`CampusJsonRepository` and accepts an injected fixture directory. The page no
longer references `CampusJsonRepository`, `ResourcePaths::Campuses`, or
`CampusInfo`. It preserves repository order and omission, trimmed
case-insensitive saved ID/name matching, first-campus fallback, availability
checks before lookup or state mutation, raw selected IDs, and `N/A` for empty
detail fields. Settings behavior and the all-years calendar query remain
outside this slice.

New Application and Platform suites cover ordering, UTF-8 fields, fallback,
malformed/default records, and missing or empty directories; the page suite
checks selected details and `N/A`. Executor and independent fresh Ninja/MSVC
x64 configures validated 886 handwritten owners and built `ClassMngr`, the
SubPrepPage suite, and both new suites. Executor CTest passed 3/3; independent
CTest passed 6/6, including three existing preference suites. Resource
generation passed, and the independent repeat build had no work. Phase 2
remains open.

## Verified F29 Personal Details atomic-save caller cutover

`ApplicationServicesPersonalDetailsSavePort` retains its
`ApplicationServices&` constructor, adds a nullable `ApplicationServices*`
constructor, and removes the `SettingsService*` constructor. Initial Setup and
My Information now pass their existing `ApplicationServices*`. The adapter
retains its availability check before mutation, UTF-8 and signature-image
preparation, request normalization, and one atomic `saveAll` for all nine
personal-details keys. Initial Setup's failure warning, My Information's
availability return before autosave cancellation or field normalization, and
rollback behavior remain preserved.

Adapter null-pointer coverage and a MyWorkspace unavailable-save regression
verify the new constructor and preservation of whitespace Zoom fields and
dirty state. Executor and independent fresh Ninja/MSVC x64 configs validated
886 handwritten owners, built `ClassMngr`, the adapter, InitialSetupWizard,
and MyWorkspace targets, and passed focused CTest 3/3. Diff check and the
caller/constructor scan passed; there was no resource limitation. Phase 2
remains open.

## Verified F30 personal-signature-preferences caller cutover

`ApplicationServicesPersonalSignaturePreferencesPort` retains its
`ApplicationServices&` constructor, adds a nullable `ApplicationServices*`
constructor, and removes `SettingsService*`. My Information and Initial Setup
pass their existing `ApplicationServices` owners. Availability guards, exact
read-only keys and defaults, UTF-8 typed text, mode/font normalization,
unavailable failure behavior, and the no-write contract are preserved.

Executor and independent fresh Ninja/MSVC x64 configs each validated 886
handwritten owners, built `ClassMngr`, the adapter, InitialSetupWizard, and
MyWorkspace targets, and passed focused CTest 3/3. Diff and source scans
passed; there was no resource limitation. Phase 2 remains open.

## Verified F31 current-campus-preferences caller cutover

`ApplicationServicesCurrentCampusPreferencesPort` removes its raw
`SettingsService*` constructor while retaining its `ApplicationServices&` and
nullable `ApplicationServices*` constructors. My Information's campus read
and correction writes, plus Initial Setup's campus read, pass their existing
`ApplicationServices` owners. The `myInfo/campus` key, UTF-8/QVariant string
conversion, unavailable empty-read/no-op-write behavior, mapped save failure,
My Information availability guard, and saved-campus correction timing remain
preserved.

Executor and independent fresh Ninja/MSVC x64 configures each validated 886
handwritten owners, built `ClassMngr`, the adapter, InitialSetupWizard, and
MyWorkspace, and passed focused CTest 3/3. Adapter/test/source scans and diff
check passed; there was no resource limitation. Phase 2 remains open.

## Verified F32 Sub Prep personal-Zoom preference caller cutover

`ApplicationServicesSubPrepPersonalZoomPreferencesPort` retains its
`ApplicationServices&` constructor, adds a nullable `ApplicationServices*`
constructor, and removes the `SettingsService*` constructor. My Information
and Initial Setup pass their existing services. Primary `myInfo/zoom*` values
take precedence; legacy `subPrep/personalZoom*` values are fallback inputs and
are best-effort migrated only when the primary key is absent. Migration failure
still returns the legacy values. UTF-8 conversion, defaults, unavailable
behavior, and page display remain unchanged. Null-constructor coverage was
updated.

The executor built `ClassMngr`, the adapter, MyWorkspace, and InitialSetupWizard;
focused CTest passed 1/1. An independent fresh Ninja/MSVC x64 configure
validated 886 handwritten owners, built all four targets, and passed focused
CTest 3/3. Adapter/source audits and diff check passed; there was no resource
limitation. Phase 2 remains open.

## Next candidate: F33 settings-availability query for My Information and Initial Setup

Remove the remaining direct settings-availability checks in My Information
(`openSettingsService` helper and its load/save gates) and Initial Setup
(`settingsService()` getter and its personal-details initialization/validation
gates) behind a narrow typed availability query. Preserve unavailable early
returns and no-mutation behavior, including My Information's save return
before autosave cancellation or Zoom normalization and Initial Setup's no-op
initialization/validation. Add direct My Information unavailable-load
coverage. Use the existing typed
`CurrentCampusPreferencesPort::isAvailable()` availability boundary and
Platform adapter. This reuses the tested narrow contract and adapter; no
generic availability contract is needed. Workbook
decoding, generic settings, other feature services, broader document work, and
the formal Phase 2 exit gate remain open.

## Verified F36 Calendar import parity handoff

The production `CalendarEventImportService` is exercised with a required
checked-in XLSX fixture over loopback transport and a temporary database. The
test covers parsing, typed existing-signature lookup and planning, then ordered
batch save; it asserts three inserted events, two skipped rows, and the exact
persisted event set. Independent fresh Windows x64 configure/build and all
five focused suites passed.

Parser-level signature deduplication and planner duplicate candidates are
covered by the separate planner suite, not by the end-to-end service test.
This verifies one Calendar import parity path; broader baseline parity and the
Phase 2 exit gate remain open. Workbook decoding and campus-directory lookup
remain legacy boundaries.

## Verified F37 Schedule import state-validation cutover

[`Application::validateScheduleImportState`](../../src/next/application/schedule_import_state_validation.h)
is a Qt-free Application contract; only this contract is established as
Qt-free here, not the entire `src/next` tree. After structural plan checks and
teacher/class snapshot reads, [`ScheduleImportRepository`](../../src/data/repositories/schedule_import_repository.cpp)
adapts those values to the request and calls the contract before writes in the
current transaction.
This replaces the duplicate legacy state validator. F38 later moved matching
and preview into a Qt-free Application contract; F39 moved overlap/conflict
projection into an Application contract shared with review presentation.
Persistence remains at the legacy edge.

The contract validates teacher and class target availability/identity,
teacher action targets and room selection, unique exact-match skip targets,
Normal/Intensive projected membership, projected day/time validity, and
cross-class overlaps. Normal imports project selected skipped classes but
drop absent classes; intensive update preserves absent existing classes,
whereas intensive replacement does not.

The repository regression in [`schedule_import_tests.cpp`](../../tests/schedule_import_tests.cpp)
adds a SQLite `BEFORE UPDATE` trigger that aborts
if an early teacher write is reached. A stale class target is rejected before
that trigger can fire, and teacher, class metadata, schedule times, and a
settings sentinel remain unchanged. This checks the pre-write boundary for
that stale-target path; other validation rules are exercised by the separate
app-less Application suite. Executor and independent Tester each configured
a fresh Windows x64 build with 895 handwritten source owners; both focused
suites passed 2/2. F38 adds matching/preview and F39 adds shared review/apply
conflict projection with required fixture coverage. Workbook decoding, wider
baseline parity, broader Domain completeness, and the formal Phase 2 gate
remain open. Phase 2 remains in progress.

## Verified F38 Schedule Import matching and preview - commit `bc6ac011504e0a499a8cdfd4b1533b49ea3f4bcb`

A Qt-free Application contract now owns Schedule Import candidate matching
and preview projection; [`ScheduleImportRepository::preview`](../../src/data/repositories/schedule_import_repository.cpp)
adapts repository records through
[`ScheduleImportMatchingProjection`](../../src/next/application/schedule_import_matching_projection.h),
and the legacy `ScheduleImportMatcher` is removed. The Qt edge creates
simplified, case-folded grade/level/room matching keys and keeps raw room text
for display. The app-less
[`matching-projection suite`](../../tests/next_application_schedule_import_matching_projection_tests.cpp)
covers all seven ranking buckets, stable ties, no match, inventory, and
Normal/Intensive fallback.

The required checked-in [`schedule_review.xlsx`](../../tests/fixtures/imports/schedule_review.xlsx)
exercises production repository preview without a skip or external path. The
[`schedule_import_tests.cpp`](../../tests/schedule_import_tests.cpp) fixture
verifies exact and weaker IDs `[43,42]`, suggestion `43`, exact/confident
status, two regular and no intensive inventory candidates, initially absent
IDs, and a whitespace room seed (`' 416 '`) matching workbook room `416`.
Executor and independent fresh x64 Ninja/MSVC runs each validated 895 source
owners, built the two focused targets, and passed CTest 2/2. QtTest reported
Schedule 24/0/1 (the sole skip is the optional external sample, which was
unset) and matching 5/0/0.

This adds required fixture-backed matching/preview evidence. F39 subsequently
adds shared review/apply conflict projection; see the verified F39 handoff
below. Wider baseline parity, workbook decoding, and other Phase 2 gaps remain
open. F38 is committed as `bc6ac011504e0a499a8cdfd4b1533b49ea3f4bcb`.
Phase 2 remains open.

## Verified F39 Schedule Import conflict projection - commit `3121d90c2db6af8e225048f016eec6f0843c1c18`

The Qt-free standard-C++
[`schedule_import_overlap_projection.h`](../../src/next/application/schedule_import_overlap_projection.h)
is shared by Schedule Import review presentation and apply-time state
validation. It covers half-open overlap versus adjacency, matching days,
deterministic conflict ordering, Normal/Intensive projection, skipped classes,
and retained intensive schedules. Translation remains at the UI edge.

The required
[`schedule_overlap_conflict.xlsx`](../../tests/fixtures/imports/schedule_overlap_conflict.xlsx)
drives production preview and review, shows the expected conflict warning, and
disables import in [`schedule_import_dialog_tests.cpp`](../../tests/schedule_import_dialog_tests.cpp).
Apply rejects the conflicting state; [`schedule_import_tests.cpp`](../../tests/schedule_import_tests.cpp)
verifies zero teachers, classes, or `class_times` persisted, and the F37
pre-write trigger sentinel still passes. App-less rule coverage is in
[`next_application_schedule_import_state_validation_tests.cpp`](../../tests/next_application_schedule_import_state_validation_tests.cpp).
Independent Tester `PH2-F39-INDEPENDENT-VERIFY`
configured fresh Windows x64 Ninja/MSVC with 896 handwritten source owners,
built three focused targets, and passed CTest 3/3. QtTest passed ScheduleImport
25/0/1 (pass/fail/skip), ScheduleImportDialog 21/0/0, and Application state
validation 12/0/0 (58 passed, 0 failed, one existing optional external-workbook
skip because `CLASSMNGR_SCHEDULE_IMPORT_SAMPLE` was unset). `git diff --check`
passed.

F39 closes the review-time conflict-projection gap, not the Phase 2 gate.
Wider baseline parity and the remaining gate-audit items remain open.

## Verified F40 Domain schedule-time value - commit `2ab23fb1796dfb1761a4c48644869a9ae6e1060d`

Standard-C++ [`Domain::Weekday` and validated `Domain::ScheduleTime`](../../src/next/domain/schedule_time.h)
provide a weekday/minute value with half-open overlap behavior. Raw Application
inputs remain available for diagnostics. After validation,
`ScheduleImportProjectedTime` carries the Domain value with Application-owned
labels through overlap projection; both apply validation and F39 UI review use
the typed time. Invalid raw times retain `InvalidProjectedTime` labels.

The independent `PH2-F40-SCHEDULE-TIME-VERIFY` recheck configured fresh
Windows x64 Ninja/MSVC Debug with 897 handwritten sources, each with one
explicit owner; four focused CTest suites passed 4/4. QtTest passed Domain 8,
state validation 13, Schedule Import repository 25, and review dialog 21 (67
passed, 0 failed), with one
existing optional external-workbook skip because
`CLASSMNGR_SCHEDULE_IMPORT_SAMPLE` was unset. The F39 fixture and review
warning/disabled-action/zero-write rejection passed, as did the F37 pre-write
trigger sentinel; `git diff --check` passed. See
[`next_domain_contract_tests.cpp`](../../tests/next_domain_contract_tests.cpp),
[`next_application_schedule_import_state_validation_tests.cpp`](../../tests/next_application_schedule_import_state_validation_tests.cpp),
[`schedule_import_tests.cpp`](../../tests/schedule_import_tests.cpp), and
[`schedule_import_dialog_tests.cpp`](../../tests/schedule_import_dialog_tests.cpp).

F40 is committed as `2ab23fb1796dfb1761a4c48644869a9ae6e1060d`. It adds Domain
value coverage but no parity scope; the Phase 2 gate remains open. Sub Prep's
interval span remains limited to the current and following calendar years at
most.

## Verified F41 Schedule Import review decisions - commit `30ec8d7512a8847a5b1d32addabf25f252b0eabb`

Qt-free [`schedule_import_review_decisions.h`](../../src/next/application/schedule_import_review_decisions.h)
is shared authority for teacher/class decision checks in dialog readiness and
the PlanValidator adapter. It validates complete, unique resolutions, actions
and targets, duplicate targets, required/foreign rooms, and skipped
teacher/class consistency. Workbook content/color/meeting validation stays at
the feature edge; SQL and stale/current-state checks remain repository/F37
responsibilities.

Required [`schedule_review.xlsx`](../../tests/fixtures/imports/schedule_review.xlsx)
now traverses production parse, preview, explicit choices, and repository
apply, asserting result summary, persisted teacher/class/color/time state, and
retained unrelated class metadata. F39 conflict-warning/disabled-action and
zero-write apply coverage and the F37 trigger sentinel still pass. Independent
verification `PH2-F41-SCHEDULE-IMPORT-VERIFY` configured fresh Windows x64
Debug/Ninja/MSVC with 899 handwritten owners; four focused CTests passed 4/4.
QtTest totals are decision contract 26/0/0, repository 25/0/1, review dialog
21/0/0, and state validation 13/0/0 (85 passed, 0 failed, one optional
`CLASSMNGR_SCHEDULE_IMPORT_SAMPLE` skip). `git diff --check` passed.
Evidence suites: [`review-decision contract`](../../tests/next_application_schedule_import_review_decisions_tests.cpp),
[`repository`](../../tests/schedule_import_tests.cpp),
[`dialog`](../../tests/schedule_import_dialog_tests.cpp), and
[`state validation`](../../tests/next_application_schedule_import_state_validation_tests.cpp).

F41 is committed as `30ec8d7512a8847a5b1d32addabf25f252b0eabb`. Fixture-backed
Schedule review/apply parity improves, but wider baseline parity and the
Phase 2 gate remain open. Sub Prep's current-plus-following-calendar-year
maximum is unchanged.

## Verified F42 workspace replacement failure handling - commit `8b2eb8a2a7a0dae6a22ce8a4163b35d5d9dd24ee`

[`FileController` replacement-open coverage](../../tests/file_controller_workspace_lifecycle_tests.cpp)
verifies that an invalid replacement profile leaves the active database,
settings sentinel, recent/last-file entries, and file-action availability
unchanged. Interactive create also aborts before target preparation when
coordinator close fails. F42 adds no new `src/next` production file; the formal
`WorkspaceGateway::createWorkspace`/`WorkspaceCoordinator` criterion is
satisfied by the dirty-rejection, successful-state-transition, and
failure-atomicity cases in the focused workspace suites.

Independent fresh x64 Ninja/MSVC verification validated 899 handwritten
owners; the four focused CTest targets passed 17/17, 33/33, 25/25, and 11/11.
`git diff --check` was clean. FileController integration remains a separate,
non-gating caveat: dirty-page approval still comes from `MainWindow`, and
normal new/initial-setup creation closes the active session before target
preparation and create succeed. Same-path replacement and the complete live
MainWindow snapshot have no direct coverage. The formal Phase 2 gate remains
open.

## Verified F43 Class Transfer review decisions - commit `c0e03e55aa5f5cc1897ccf97a25901a5e119c8e5`

The Qt-free
[`class_transfer_projection.h`](../../src/next/application/class_transfer_projection.h)
provides shared review-decision validation to
[`ClassImportDialog`](../../src/features/classes/ui/class_import_dialog.cpp)
readiness and
[`ClassTransferRepository`](../../src/data/repositories/class_transfer_repository.cpp)
apply validation. The application contract has no Qt or legacy Application
dependencies; persistence remains at the repository edge.

Required checked-in
[`success_source.json`](../../tests/fixtures/transfers/success_source.json)
drives production fixture-backed apply and verifies persisted class, teacher,
schedule including end time, and roster state.
[`conflict_source.json`](../../tests/fixtures/transfers/conflict_source.json)
verifies a conflicting transfer causes no partial writes. The
[app-less contract suite](../../tests/next_application_class_transfer_tests.cpp)
and [repository integration suite](../../tests/class_transfer_tests.cpp)
passed 19 and 15 QtTest cases respectively.

Independent fresh Windows x64 Ninja/MSVC verification validated 899 handwritten
source owners; the two focused CTest targets passed 2/2, and `git diff --check`
was clean. F43 advances app-less Domain/Application behavior and fixture-backed
parity, while both gates remain Partial pending broader Domain records and
baseline coverage. The formal `WorkspaceGateway::createWorkspace`/
`WorkspaceCoordinator` criterion remains Satisfied, as does audited `src/next`
dependency isolation. FileController dirty approval, New Profile/Initial Setup
replacement preservation, and same-path/full MainWindow integration coverage
remain separate non-gating caveats. Phase 2 remains In Progress and the formal
exit gate remains open. Non-gating FileController gaps remain: dirty approval
comes from MainWindow, and New Profile/Initial Setup can close the active
session before replacement succeeds; same-path replacement and full MainWindow
snapshot coverage remain absent. Sub Prep remains bounded to the current and
following calendar years at most; `cmake/sources.cmake` is unchanged.

## Verified F47 Course weekly meeting-day rule - commit `7cba8abf952b5b32f90391844beec68eac2c3f69`

`Domain::Course::WeeklyMeetingDayRule` in
[`course.h`](../../src/next/domain/course.h) owns typed weekly meeting-day
policy over `Domain::Weekday` patterns, including invalid, out-of-range, and
duplicate-day rejection and order-insensitive matching. Schedule Import's
production parser/partitioning and plan/apply validation use that policy;
QString weekday parsing and translated diagnostics stay at the feature edge.
Legacy grade trim/uppercase, trimmed case-insensitive Athena/Song's categories,
allowed/forbidden patterns, and E5/Zeus Tuesday-only Skip behavior are
preserved. Unsupported `M3 Zeus` has no pattern error but remains rejected by
separate Course validation.

The required [`schedule_review.xlsx`](../../tests/fixtures/imports/schedule_review.xlsx)
persists accepted rows. A fixture-derived one-day E4/Theseus prohibited pattern
is rejected before writes, preserving snapshots for teachers, classes,
class_info, class_times, and app_settings. Existing invalid-course, overlap
no-write, and Skip-with-prohibited-pattern coverage remains.

Executor and independent fresh x64 MSVC 19.51/Ninja 1.13.2/CMake 4.4.2/Qt 6.12.0
builds each passed focused CTest 2/2 across `ClassMngrNextDomainContractTests`
and `ClassMngrScheduleImportTests`. Direct fixture persistence, invalid-pattern and
invalid-course rejection, overlap, and Domain policy cases passed. No full
suite was run. `git diff --check` passed. Gate 1 and Gate 2 advance but remain
Partial; Workspace boundary and audited dependency isolation remain Satisfied.
Phase 2 remains In Progress with its exit gate Open. Sub Prep remains capped at
the current and following calendar years at most.

## Verified F48 Domain schedule-entry persistence - commit `2055bbb5f74842e4f146a48e211df58e65908b6b`

Qt-free [`Domain::ScheduleEntry`](../../src/next/domain/schedule_entry.h)
pairs typed `Domain::ClassId` and validated `Domain::ScheduleTime`, with value
accessors/equality/order and no presentation fields. The app-less Domain test
checks class-versus-teacher ID type separation, value semantics, and typed time
values. The header is registered in `cmake/next.cmake` only.

Schedule Import constructs entries after resolving real IDs for created or
updated targets. Entries derive from existing `finalTimes`, so retained and
skipped rows are included, and the persistence writer consumes them. The SQL
adapter preserves the original day/time text and checks it against typed
values. Existing write order, transaction, rollback, Skip, and intensive
branches are unchanged. The required `schedule_review.xlsx` production path
compares persisted SQL facts with typed entries after ID resolution. The
`schedule_overlap_conflict.xlsx` rejection test seeds teacher, class,
class_info, class_times, and app_settings snapshots and confirms all remain
unchanged; invalid-course/pattern, Skip, intensive, and write-failure rollback
coverage also passes.

Executor and independent Tester used fresh x64 MSVC 19.51.36257/Ninja
1.13.2/CMake 4.4.2/Qt 6.12.0 builds; focused CTests passed 2/2 in both runs.
Direct QtTest results: Domain 17/0/0 and Schedule Import 26/0/1; the sole skip
is the optional external-workbook sample. The Tester reran workbook apply,
overlap rejection, Skip, and Intensive cases successfully. CMake ownership
covered 902 handwritten files; `git diff --check` passed and the user-owned
`cmake/sources.cmake` SHA-256 remained
`9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF`.
Gate 1 and Gate 2 advance but remain Partial; the Workspace boundary and
audited v2 dependency isolation remain Satisfied. Broader parity and Domain
gaps remain open. Phase 2 remains In Progress and its exit gate Open. Sub Prep
coverage stays capped at the current and following calendar years at most.

## Verified F49 Calendar Import use case - commit `6a41e958671b7fa93c301d8b25c9c4381178fd7f`

Qt-free [`CalendarEventImportUseCase`](../../src/next/application/calendar_event_import_use_case.h)
composes the existing signature-query port, duplicate planner, and batch-save
port. `CalendarEventImportService::handleFinished` delegates signature lookup,
deduplication, ordered signature/request pairing, batch save, and imported and
skipped counts to this use case. Pairing signatures with requests prevents
index mismatch. Workbook/network/campus handling, Qt signals, and localized
error translation remain at the feature edge; the injected observer preserves
profiler timing at the prior boundaries.

Six app-less fake-port tests cover ordered mapping, existing and in-batch
duplicates, parser skip counts, empty and duplicate-only input, exact UTF-16
signature identity including a lone surrogate, and query/save failures. The
required [`calendar_import_parity_2026.xlsx`](../../tests/fixtures/imports/calendar_import_parity_2026.xlsx)
production fixture exercises the modified service and verifies persisted rows
and counts. Executor and independent Tester fresh builds each passed the
focused CTest 2/2; no full suite was run. Gate 1 and Gate 2 advance but remain
Partial; Workspace boundary and audited v2 dependency isolation remain
Satisfied. Phase 2 remains In Progress and the exit gate Open. Sub Prep stays
capped at the current and following calendar years at most.

## Verified F50 Calendar Import signature identity - commit `92d001db11d8c8eb973de5f238444abe855ea5c5`

Qt-free [`Application::CalendarEventImportSignature`](../../src/next/application/calendar_event_import_signature.h)
is the shared legacy six-field key value used by
[`academic_calendar_event_parser.cpp`](../../src/features/calendar/academic_calendar_event_parser.cpp)
and [`application_services_calendar_event_import_signature_query_port.h`](../../src/next/platform/application_services_calendar_event_import_signature_query_port.h).
It preserves field order, delimiters, exact UTF-16 code units, and the all-day
`1/0` value; it excludes time fields, database ID, and repeat-series ID. The
Qt adapters continue to own simplified title, normalized type/time status, and
ISO date conversion. App-less tests cover key formatting, each field, UTF-16,
placeholder-like `%2` title text, and type members excluding metadata;
existing `CalendarImportTests` covers normalization and excluded metadata. A
Qt 6.12 probe confirms legacy six-argument `QString::arg` does not rescan
inserted `%2` title text.

Executor and independent Tester each freshly configured Windows x64 MSVC/Ninja,
validated 906 source owners, built three focused targets, and passed CTest 3/3,
including required `calendar_import_parity_2026.xlsx` production parity. No
full suite was run. Gate 1 and Gate 2 remain Partial; Workspace boundary and
audited `src/next` dependency isolation remain Satisfied. Phase 2 remains In
Progress with its exit gate Open. Sub Prep remains capped at the current and
following calendar years at most.

## Verified F51 typed Calendar Import signature flow - commit e940f0c0ed8a63e740a3c2375631a22c08875f84

Application::CalendarEventImportSignature now flows through the
[CalendarEventImportSignatureQueryPort](../../src/next/application/calendar_event_import_signature_query_port.h),
[CalendarEventImportPlan](../../src/next/application/calendar_event_import_plan.h),
and [CalendarEventImportUseCase](../../src/next/application/calendar_event_import_use_case.h)
as a typed value. The parser and
[ApplicationServicesCalendarEventImportSignatureQueryPort](../../src/next/platform/application_services_calendar_event_import_signature_query_port.h)
produce it directly; candidate deduplication uses the value's exact UTF-16
payload. Query results, planner inputs, and candidates no longer convert the
signature through raw std::u16string values. Qt-side normalization and ISO
date conversion remain at the adapters.

A fresh Windows x64 MSVC/Ninja configure validated 906 source owners. Executor
and independent Tester each verified seven focused CTest targets, each passing
1/1: ClassMngrNextApplicationCalendarEventImportSignatureTests,
ClassMngrNextApplicationCalendarEventImportSignatureQueryPortTests,
ClassMngrNextApplicationCalendarEventImportPlanTests,
ClassMngrNextApplicationCalendarEventImportUseCaseTests,
ClassMngrCalendarImportTests,
ClassMngrNextPlatformApplicationServicesCalendarEventPortTests, and
ClassMngrCalendarEventImportParityTests. The production parity test used
[calendar_import_parity_2026.xlsx](../../tests/fixtures/imports/calendar_import_parity_2026.xlsx).
With QCOMPARE diagnostics restored, ClassMngrCalendarImportTests was rebuilt
and rerun, passing 1/1. No full suite was run. Gate 1 and baseline parity Gate 2
remain Partial; workspace create and audited v2 dependency isolation remain
Satisfied. Phase 2 remains In Progress with its exit gate Open.

## Verified F52 shared calendar timing validation - commit `9cd9a2a4469482bc803cdc172d18a072c0fb3949`

Qt-free [`Domain::CalendarEventTiming`](../../src/next/domain/calendar_event_timing.h)
now supplies timing validation to [`CalendarEventEditDraft`](../../src/next/application/calendar_event_edit_draft.h),
[`CalendarEventSavePort`](../../src/next/application/calendar_event_save_port.h),
and [`CalendarEventSeriesEditPort`](../../src/next/application/calendar_event_series_edit_port.h).
The feature-facing application boundaries retain their specific error messages
and validation order, including the existing cross-day clock rule. F52 changes
no legacy service-call mapping; the save and series-edit adapters continue to
map their typed requests to the existing `CalendarService` operations.

## Verified F53 Roster Score Import parity

[`roster_editor_widget_import_tests.cpp`](../../tests/roster_editor_widget_import_tests.cpp)
exercises the production `RosterEditorWidget::importScores` slot through
`ClassMngrRuntime`. The legacy workflow reads saved speaking evaluations from
production services and maps their grades into the four roster evaluation
columns; it does not parse a workbook. Temporary-database setup seeds the
evaluations through production services. Tests cover English/Korean name-pair
matching including collisions, unmatched/empty/English-only partial-row
preservation, autosave and persistence through a fresh roster-service read,
idempotent re-import, and missing English/Korean column warnings without
persisted changes. A mixed Winter score totaling 16/6 is asserted as B+.

The registered CTest is `ClassMngrRosterEditorWidgetImportTests`. Independent
fresh Ninja/MSVC 19.51/Qt 6.12 verification validated 908 source owners, built
that target plus `ClassMngrRosterModelTests` and
`ClassMngrSpeakingEvaluationServiceTests`, and passed CTest 3/3. No full suite
was run. F53 adds Gate 2 parity evidence only; Gate 1 and Gate 2 remain
Partial, the workspace boundary and audited `src/next` dependency isolation
remain Satisfied, and the Phase 2 exit gate remains Open. Sub Prep remains
limited to the current and following calendar years at most.

## Verified F54 Calendar event vocabulary - commit `3739f2aaca23587c732dc77d6e77eddb16d92d88`

Qt-free Domain classifiers in
[`calendar_event_timing.h`](../../src/next/domain/calendar_event_timing.h)
define the six Calendar event type names and three time statuses. The calendar
edit-draft, single-save, and repeat-series-edit validators reuse them while
preserving request strings and raw fields, trimming at each Application
boundary, 64-character limits, validation order, feature-specific errors, and
projection behavior.

Domain and Application tests cover known, unknown, mis-cased, and untrimmed
values, padded inputs with raw-field preservation, length boundary/overflow,
and per-request errors. Fresh independent Ninja/MSVC 19.51/Qt 6.12
configure/build ran `ClassMngrNextDomainContractTests` and
`ClassMngrNextApplicationCalendarEventTests`; exact CTest passed 2/2. No full
suite was run. F54 adds Gate 1 evidence only: Gate 1 and Gate 2 remain Partial,
Workspace boundary and audited `src/next` dependency isolation remain
Satisfied, and the Phase 2 exit gate remains Open. Sub Prep remains capped at
the current and following calendar years at most.

## Verified F55 shared Course grade-band classification - commit `87b7bfff66bf13cc5b79180cd1142101875be3cf`

Qt-free [`Course::gradeBandForName`](../../src/next/domain/course.h) is the
shared grade-only classifier for the Classes page, Evaluation Default
Selection, and Schedule. Each feature keeps its existing `trimmed().toUpper()`
boundary and policy. Classes preference behavior, Evaluation's
Middle-for-M1–M3/Elementary-otherwise rule, and Schedule's M2/M3 suppression
and conditional M1 suppression remain unchanged. Classification does not
require a valid grade/level pair.

Coverage spans all six grade bands and Other/invalid/case/whitespace inputs,
invalid-pair independence, class preference behavior, Evaluation's
grade/school-level policy helper, and Schedule suppression without assignments
including the M1 toggle. The Evaluation test calls the same private policy
helper used by `forClass`, not the complete `ApplicationServices` integration.

Independent fresh Ninja/MSVC 19.51/Qt 6.12 verification validated 908
handwritten source owners, built `ClassMngrNextDomainContractTests`,
`ClassMngrClassesPageTests`, `ClassMngrSchedulePrintModelTests`, and
`ClassMngrEvaluationDefaultSelectionTests`; exact CTest passed 4/4. No full
suite was run. F55 adds Gate 1 evidence only: Gate 1 and Gate 2 remain Partial,
the Workspace boundary and audited `src/next` dependency isolation remain
Satisfied, and the Phase 2 exit gate remains Open. Sub Prep remains capped at
the current and following calendar years at most.

## Verified F56 Speaking Evaluation grade mapping - commit `c73e896fe34e186a045d73b653aa8ec9dfa89e83`

Qt-free [`Domain::SpeakingEvaluationGrade`](../../src/next/domain/speaking_evaluation_grade.h)
is shared by repository roster import, the speaking-evaluation report data
assembler, and the report widget for the six criteria and C/B/B+/A/A+ values.
It owns exact-label parsing and legacy aggregate behavior, including >=0.4
rounding, invalid/missing outcomes, and clamping. Input normalization remains
at the legacy boundaries: repository import trims saved labels, while report
paths require exact labels.

The real roster widget-import test verifies trimming of a padded saved label
and an incomplete evaluation producing N/A; it retains the mixed 16/6-to-B+
result, persistence, and idempotence assertions. The report-widget path checks
B+ and N/A through assembly and rendering. Domain tests cover all 15,625 valid
six-criterion combinations plus labels, invalid/missing inputs, and rounding.

Independent fresh Ninja/MSVC 19.51/Qt 6.12 verification validated 909 source
owners, built `ClassMngrNextDomainContractTests`,
`ClassMngrRosterEditorWidgetImportTests`,
`ClassMngrSpeakingEvaluationServiceTests`, and
`ClassMngrSpeakingEvalReportWidgetTests`, and passed exact CTest 4/4. No full
suite was run. F56 adds Gate 1 and Gate 2 evidence, but both remain Partial;
the Workspace boundary and audited `src/next` dependency isolation remain
Satisfied, and the Phase 2 exit gate remains Open. F55 Evaluation Default
Selection coverage remains limited to the policy helper rather than full
`ApplicationServices::forClass` integration. Sub Prep remains capped at the
current and following calendar years at most.

## Verified F58 typed Schedule Import matching identities - commit `9b9183818fc2163d625a8ffb088a492a4aa631a9`

[`ScheduleImportMatchingProjection`](../../src/next/application/schedule_import_matching_projection.h)
now accepts `Domain::TeacherId`/`Domain::ClassId` identity values and represents
a suggested class with `std::optional`; the legacy
[`ScheduleImportRepository`](../../src/data/repositories/schedule_import_repository.cpp)
adapter converts numeric IDs to the existing integer preview representation.
Compatibility coverage preserves ordering, ranks, confidence, explanations,
intensive fallback, empty teacher-key behavior, unfiltered teacher IDs (including
0/-1), and nonpositive class IDs excluded from matching/suggestion but retained
in `initiallyAbsentClassIds`.

Independent fresh x64 Ninja/MSVC 19.51.36257/Qt 6.12 verification validated
912 handwritten source owners, built the matching projection and
`ClassMngrScheduleImportTests`, and passed exact CTest 2/2. The production test
`previewsAndAppliesCheckedInWorkbookAgainstSeededDatabase` uses the checked-in
[`schedule_review.xlsx`](../../tests/fixtures/imports/schedule_review.xlsx).
Executor focused CTest passed 1/1; no full suite was run. A direct production
assertion for the adapter's no-suggestion `-1` sentinel remains uncovered; the
existing data-model default remains `-1`, while app-less contract tests assert
no suggested class.
