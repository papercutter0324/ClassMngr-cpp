# Phase 2 legacy application mapping

This document maps the current legacy application boundary to the committed
v2 application contracts. It is a mapping and handoff, not a claim that the
legacy adapter or runtime cutover exists.

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
window/action state around those calls. No v2 application header currently
includes the legacy facade or provides its adapter implementation.

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
| `calendarService()` | Menu, calendar pages, and sub-prep consume calendar events; representative calls are [`menu_builder.cpp`](../../src/app/menu_builder.cpp#L391) and [`calendar_page_events.cpp`](../../src/features/calendar/ui/calendar_page_events.cpp#L40). | Future calendar use cases/projections; no v2 wrapper. | Legacy feature service; future calendar slice. |
| `rosterService()` | Roster editors/printing, class pages, sub-prep, and speaking evaluation use roster operations; representative calls are [`roster_editor_widget.cpp`](../../src/features/roster/ui/roster_editor_widget.cpp#L64) and [`roster_print_dialog.cpp`](../../src/features/roster/ui/roster_print_dialog.cpp#L445). | Future roster use cases/projections; no v2 wrapper. | Legacy feature service; future roster slice. |
| `speakingEvaluationService()` | Speaking-evaluation pages, analytics, and roster score import use it; representative calls are [`speaking_eval_page.cpp`](../../src/features/speaking_eval/ui/speaking_eval_page.cpp#L198) and [`class_analytics_page.cpp`](../../src/features/classes/ui/class_analytics_page.cpp#L526). | Future evaluation/analytics contracts; no v2 wrapper. | Legacy feature service; future evaluation slice. |
| `themeService()` | [`MainWindow`](../../src/app/mainwindow.cpp#L480) injects it into the theme controller; schedule output also reads the current theme. | Future theme/platform contract; no v2 wrapper. | Legacy core service; future platform/UI slice. |
| `documentCatalog()` | [`MainWindow`](../../src/app/mainwindow.cpp#L433) passes it to the sidebar and [`NavigationController`](../../src/app/controllers/navigation_controller.cpp#L357) reads it for document navigation. | Future document metadata/content contract; no v2 wrapper. | Legacy catalog owned by `ApplicationServices`; future document slice. |

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
   The runtime Qt thread bridge is not yet implemented.
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
| 0. Mapping | Keep this mapping and the legacy path unchanged. | Source/link checks pass; no production behavior changes. Revert only this documentation slice if the boundary evidence changes. |
| 1. Workspace gateway adapter | Add one outer adapter for `WorkspaceGateway::createWorkspace` plus the listed open/close/save/save-as/export methods around `ApplicationServices`; keep the separate new/initial-setup creation decision explicit and `FileController` on its existing calls. | Existing app-less `NextApplicationContractTests` and `NextApplicationWorkspaceCoordinatorTests` remain green for create/open/close/save/save-as/export, including create validation, dirty-replacement rejection, successful state/selection commit, and failure preservation. Adapter tests cover path conversion, legacy error text, void-save result source, explicit create mapping, and failure atomicity. The adapter is removable without changing v2 headers. |
| 2. File-controller integration | Route create/open/close/save/save-as/export one workspace action at a time through the adapter. Keep dialogs, recent files, warnings, and window/action updates in the Qt/controller layer. | Create preserves the current coordinator contract: `WorkspaceGateway::createWorkspace` is called only after guards, a successful session opens `WorkspaceState` and clears `SelectionState`, and gateway or invalid-session failures leave snapshots unchanged. The other listed actions preserve verified legacy outcomes and v2 snapshots; dirty/conflict and failure cases leave snapshots unchanged. Roll back the action entry point to the existing `ApplicationServices` call if a gate fails. |
| 3. Worker bridge | Connect Qt worker delivery to the existing import/report sinks and application-owner `pump()`; keep workers behind their ports. | Bounded FIFO, generation isolation, cancellation request/acknowledgement, and terminal release tests pass with no worker/widget mutation. Remove the bridge without changing coordinator/state contracts if a gate fails. |
| 4. Feature slices | Migrate settings, teachers, classes, schedule, calendar, roster, speaking evaluation, theme, and document services as separate typed contracts. | Each slice has its own owner, adapter, parity tests, and release boundary; no v2 contract exposes a legacy service pointer. Leave unstarted services on legacy accessors until their slice is accepted. |

The Phase 2 [deliverables](03-Phase-2-Domain-Model-and-Application-Contracts.md#deliverables)
require this mapping, but the Phase 2 exit gate is not met by documentation
alone: the outer adapter, runtime bridge, and feature slices remain work.
