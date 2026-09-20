# Phase 2 legacy application mapping

This document maps the current legacy application boundary to the committed
v2 application contracts. The typed Sidebar/MainWindow catalog cutover follows
baseline commit `662e5f2`; the earlier mapping, resolver, and document-folder
handoffs are retained below. The runtime worker bridge, limited document route
slice, bounded resource/platform document resolver, typed catalog ownership,
calendar read-projection adapter, theme and language preference bridges are implemented. A partial content-session
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
| `calendarService()` | Menu, calendar pages, and sub-prep consume calendar events; representative calls are [`menu_builder.cpp`](../../src/app/menu_builder.cpp#L391) and [`calendar_page_events.cpp`](../../src/features/calendar/ui/calendar_page_events.cpp#L40). | `Platform::ApplicationServicesCalendarEventPort` maps `CalendarService::eventsInRange` into an owned typed `Application::CalendarEventProjection`, with bounded copied metadata, typed IDs, explicit all-day/unknown-time policy, and validation of range, service, technical, ID/metadata, partial-time, and capacity failures. The read-projection boundary is complete; cache/UI cutover remains future because the projection omits `eventType`, `timeStatus`, and `repeatSeriesId`, while `CalendarEventCache` directly owns the database worker. | Platform calendar read adapter; future calendar cache/UI migration. |
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
| 4. Feature slices | Migrate generic settings persistence, teachers, classes, schedule, calendar, roster, speaking evaluation, and the remaining document ownership/content boundaries as separate typed contracts. The accepted preference work includes the theme, language, and schedule-output explicit-theme boundaries; accepted document work includes metadata projection, typed Sidebar/MainWindow catalog ownership, content-reference propagation, the bounded resource/platform resolver, and the partial `PdfViewerPage` session lifecycle. | Each slice has its own owner, adapter, parity tests, and release boundary; no v2 contract exposes a legacy service pointer. The theme, language, and schedule-output explicit-theme boundaries and typed `MainWindow`/`Sidebar` catalog ownership are complete; generic settings persistence, full document-service migration, and other unstarted services remain future slices, with legacy accessors retained until their own cutovers. |

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

The calendar read-projection boundary is complete. Calendar cache/UI cutover
is future because `CalendarEventProjection` currently omits `eventType`,
`timeStatus`, and `repeatSeriesId`, and `CalendarEventCache` directly owns the
database worker. Generic settings and other feature migrations remain open;
Phase 2 remains in progress and is not complete.
