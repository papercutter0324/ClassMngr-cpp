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
theme
and language preference bridges are implemented.
A partial content-session
integration covers referenced
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
| `openDatabase(QString)` | [`FileController::loadDatabase`](../../src/app/controllers/file_controller.cpp#L452), new-database setup, and initial setup open the selected path; the facade forwards to `DataService::openDatabase`. | `WorkspaceGateway::openWorkspace` -> `WorkspaceUseCase::openWorkspace` -> `WorkspaceCoordinator::openWorkspace`; successful commit uses `WorkspaceState::open` and clears `SelectionState`. Contract destination exists; legacy adapter and cutover are pending. | Qt/file outer adapter; v2 application use case, coordinator, and state. |
| `closeDatabase()` | [`FileController::closeActiveDatabase`](../../src/app/controllers/file_controller.cpp#L854) closes the current database and then clears its UI file state. | `WorkspaceGateway::closeWorkspace` -> `WorkspaceUseCase::closeWorkspace` -> `WorkspaceCoordinator::closeWorkspace`; successful close uses `WorkspaceState::close` and clears `SelectionState`. Contract destination exists; adapter pending. | Legacy facade now; v2 coordinator/state after migration. |
| `hasOpenDatabase()` | Guards in [`FileController`](../../src/app/controllers/file_controller.cpp#L528) and [`MainWindow`](../../src/app/mainwindow.cpp#L112), plus navigation guards, prevent actions without an open database. | Read `WorkspaceState::snapshot()` and its optional session/lifecycle; do not add a v2 boolean wrapper around `ApplicationServices`. State projection exists; runtime read migration is pending. | v2 application state; UI adapter consumes a copy. |
| `currentDatabasePath()` | [`FileController`](../../src/app/controllers/file_controller.cpp#L991) uses it for dialog location and current-file fallback; sidebar transfer and calendar UI also read it. | Read the current `WorkspaceSession::location()` from `WorkspaceStateSnapshot`; convert the adapter-neutral location back to a UI path at the outer boundary. Projection exists; runtime read migration is pending. | v2 workspace state; Qt/file adapter owns conversion. |
| `saveDatabase()` | [`FileController::saveDatabase`](../../src/app/controllers/file_controller.cpp#L771) is used by manual save and autosave; the legacy facade calls `DataService::save()`, which is void. | `WorkspaceGateway::saveWorkspace` -> `WorkspaceUseCase::saveWorkspace` -> `WorkspaceCoordinator::saveWorkspace` -> `WorkspaceState::markSaved`. Contract destination exists; the adapter still needs a verified structured result for the legacy void call. | Legacy persistence adapter; v2 coordinator/state. |
| `saveDatabaseAs(QString)` | [`FileController::saveDatabaseAs`](../../src/app/controllers/file_controller.cpp#L785) normalizes the output path, calls the facade, then reopens it through `loadDatabase` on success. | `WorkspaceGateway::saveWorkspaceAs` -> `WorkspaceUseCase::saveWorkspaceAs` -> `WorkspaceCoordinator::saveWorkspaceAs` -> `WorkspaceState::markSavedAs`; the returned location replaces the session location atomically. Contract destination exists; adapter pending. | Qt/file outer adapter; v2 use case, coordinator, and state. |
| `exportDatabaseAs(QString)` | [`FileController::exportDatabaseAs`](../../src/app/controllers/file_controller.cpp#L817) normalizes the output path, calls the facade, and remembers the directory on success without replacing the open workspace. | `WorkspaceGateway::exportWorkspace` -> `WorkspaceUseCase::exportWorkspace` -> `WorkspaceCoordinator::exportWorkspace`; the coordinator does not mutate workspace or selection state. Contract destination exists; adapter pending. | Qt/file outer adapter; v2 use case/coordinator. |

The v2 coordinator guards dirty replacement and dirty close before gateway
calls, commits state only after a successful result, and clears selection only
after a successful open/create/close. Closed-workspace save, save-as, and
export return structured `NotFound` without calling the use case. These are
the committed v2 semantics in [`workspace_coordinator.h`](../../src/next/application/workspace_coordinator.h), not behavior currently provided by
`ApplicationServices`.

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
| `teacherService()` | Navigation, setup/import, roster, and teacher-facing UI; representative call sites are [`navigation_controller.cpp`](../../src/app/controllers/navigation_controller.cpp#L118) and [`initial_setup_wizard.cpp`](../../src/features/setup/ui/initial_setup_wizard.cpp#L154). | Future teacher use cases/projections; no v2 wrapper. | Legacy feature service; future teacher slice. |
| `classService()` | Navigation, classes, roster, schedule, speaking evaluation, and setup use class CRUD/import data; representative calls are [`classes_page.cpp`](../../src/features/classes/ui/classes_page.cpp#L179) and [`navigation_controller.cpp`](../../src/app/controllers/navigation_controller.cpp#L234). | Future class use cases/projections; no v2 wrapper. | Legacy feature service; future class slice. |
| `scheduleService()` | Menu, schedule UI, testing-class UI, and roster output consume schedule/testing operations; representative calls are [`menu_builder.cpp`](../../src/app/menu_builder.cpp#L195) and [`schedule_widget.cpp`](../../src/features/schedule/ui/schedule_widget.cpp#L307). | Future schedule/testing contracts; no v2 wrapper. | Legacy feature service; future schedule slice. |
| `calendarService()` | Menu, calendar pages, and sub-prep consume calendar events; representative calls are [`menu_builder.cpp`](../../src/app/menu_builder.cpp#L391) and [`calendar_page_events.cpp`](../../src/features/calendar/ui/calendar_page_events.cpp#L40). | `Platform::ApplicationServicesCalendarEventPort` maps `CalendarService::eventsInRange` into an owned typed `Application::CalendarEventProjection`, with bounded copied metadata including `eventType`, `timeStatus`, and optional `repeatSeriesId`, typed IDs, explicit all-day/unknown-time policy, and validation of range, service, technical, ID/metadata, partial-time, and capacity failures. The port also exposes typed `projectionById(int)` for activation reads, while `ApplicationServicesCalendarEventDeletePort` maps typed `CalendarEventId` deletion to legacy `CalendarService::deleteEvent` and returns typed failure results for its boundary cases. `CalendarEventDialog::eventData()` returns a typed `CalendarEventEditDraft`; private `legacyEventData()` retains the dialog-local Qt/legacy conversion used for validation, and the constructor/member now use the draft by value. `ApplicationServicesCalendarEventSavePort` maps typed draft-derived requests to `CalendarService::saveEvents({event})` and returns `Result<CalendarEventId>`; `ApplicationServicesCalendarEventSeriesEditPort` maps a typed `CalendarEventSeriesEditRequest` by loading `repeatSeriesFromDate(repeatSeriesId, startDate)`, applying the date offset, duration, and edited-field propagation, then saving through `saveEvents(updatedEvents)` with `Result<void>`; `ApplicationServicesCalendarEventSeriesCreatePort` maps a typed Qt-free `CalendarEventSeriesCreateRequest` to one ordered legacy batch and one atomic `saveEvents(events)` call, returning typed occurrence IDs; `ApplicationServicesCalendarEventSeriesDeletePort` maps a bounded typed repeat-series suffix-delete request to legacy `CalendarService::deleteRepeatSeriesFromDate` and returns `Result<void>`. It trims only `repeatSeriesId`, preserves existing mapped-field whitespace, and returns structured failures for blank, over-bounds, or malformed fields. `CalendarEventCache` retains typed summaries and exposes date-scoped and range-scoped typed projections; `CalendarEventModel` consumes typed rows and converts to QML types only at the UI boundary. Legacy `eventsForDate`/`eventsInRange` compatibility remains for other callers; `calendar_page_upcoming_events.cpp` uses `CalendarEventSummary` for its narrow retrieval/filtering/formatting/row-rendering path, and `CalendarPage::ensureNextTenEvents` uses the typed range projection with the typed `filterUpcomingEvents` overload. `calendar_page_events.cpp` maps legacy activation/new-event values to `CalendarEventEditDraft` before constructing the dialog and consumes drafts for all typed save, series-create, and series-edit requests; existing typed edit/save/delete/dialog paths, defaults, validation, inline errors, warnings, repeat/delete/mutation behavior, `schedule_use_24h`, routing, and invalidation/refresh remain preserved. | Platform calendar read adapter, dialog edit-draft and constructor/input ownership boundaries, cache/model, upcoming-read, next-ten-prefetch, activation-read, non-repeat save/delete, repeat-occurrence save, this-and-following repeat-series edit/save, new-repeat series-create/batch-save, and repeat-series suffix-delete boundaries, plus worker-boundary adapter; broader typed calendar UI/page migration remains future. |
| `rosterService()` | Roster editors/printing, class pages, sub-prep, and speaking evaluation use roster operations; representative calls are [`roster_editor_widget.cpp`](../../src/features/roster/ui/roster_editor_widget.cpp#L64) and [`roster_print_dialog.cpp`](../../src/features/roster/ui/roster_print_dialog.cpp#L445). | Future roster use cases/projections; no v2 wrapper. | Legacy feature service; future roster slice. |
| `speakingEvaluationService()` | Speaking-evaluation pages, analytics, and roster score import use it; representative calls are [`speaking_eval_page.cpp`](../../src/features/speaking_eval/ui/speaking_eval_page.cpp#L198) and [`class_analytics_page.cpp`](../../src/features/classes/ui/class_analytics_page.cpp#L526). | Future evaluation/analytics contracts; no v2 wrapper. | Legacy feature service; future evaluation slice. |
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
| 1. Workspace gateway adapter | Add one outer adapter for `WorkspaceGateway::createWorkspace` plus the listed open/close/save/save-as/export methods around `ApplicationServices`; keep the separate new/initial-setup creation decision explicit and `FileController` on its existing calls. | Existing app-less `NextApplicationContractTests` and `NextApplicationWorkspaceCoordinatorTests` remain green for create/open/close/save/save-as/export, including create validation, dirty-replacement rejection, successful state/selection commit, and failure preservation. Adapter tests cover path conversion, legacy error text, void-save result source, explicit create mapping, and failure atomicity. The adapter is removable without changing v2 headers. |
| 2. File-controller integration | Route create/open/close/save/save-as/export one workspace action at a time through the adapter. Keep dialogs, recent files, warnings, and window/action updates in the Qt/controller layer. | Create preserves the current coordinator contract: `WorkspaceGateway::createWorkspace` is called only after guards, a successful session opens `WorkspaceState` and clears `SelectionState`, and gateway or invalid-session failures leave snapshots unchanged. The other listed actions preserve verified legacy outcomes and v2 snapshots; dirty/conflict and failure cases leave snapshots unchanged. Roll back the action entry point to the existing `ApplicationServices` call if a gate fails. |
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

`ActionRegistry` now uses the typed load. The existing `OptionState` remains
the compatibility writer and menu-persistence owner, and `PageManager`/
`PdfViewer` behavior remains unchanged.

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

The existing `OptionState` remains the compatibility writer and menu owner.
`FontManager` offsets, visual-capture precedence, and startup behavior remain
unchanged.

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
