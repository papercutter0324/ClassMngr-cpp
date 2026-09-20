# Phase 2 — Domain Model and Application Contracts

## Status

- Status: In progress
- Default route: Heavy
- Depends on: Phase 1
- Blocks: Persistence, bootstrap, shared UI, and feature migration
- Owner: Unassigned
- Last updated: 2026-09-20
- Current note: Replace implicit behavior and UI-coupled service calls with explicit contracts. The typed Domain slice, workspace persistence/state contracts, current-selection state owner, import-job lifecycle contract, report/export-job lifecycle contract, document-content session contract, legacy application mapping document, Qt-free legacy workspace gateway seam, concrete ApplicationServices workspace port, FileController open/close/create/initial-setup/save/save-as/export integration, and Qt runtime worker/cancellation bridge are implemented. Feature-service migration remains. Invalid-UTF-8 boundary coverage is non-blocking and remains untested.

#### Progress update - 2026-09-19 (initial domain-contract slice)

`ClassMngrNext::Domain` now owns header-only, Qt-free contracts for typed
workspace, teacher, class, campus, and calendar-event identifiers, together
with `Result<T>`, `Result<void>`, `OperationError`, and explicit error codes.
The contracts reject empty identifiers, keep identifier categories distinct at
compile time, and separate successful values from recoverable or technical
operation failures.

`ClassMngrNextDomainContractTests` exercises these rules without constructing a
`QApplication`; CMake source ownership records the new headers explicitly.
The Windows Debug target and `ClassMngrNextLaunch` both passed locally. The
next slice is to define the first application use-case input/output contract
and map it to the legacy service boundary without adding widget or singleton
dependencies.

#### Progress update - 2026-09-19 (workspace application-contract slice)

`ClassMngrNext::Application` now owns a Qt-free, header-only workspace
create/open/close contract. Requests carry an adapter-neutral workspace
location, successful create/open operations return a copyable session
projection containing the typed `Domain::WorkspaceId` and location, and close
accepts that caller-owned session explicitly. `WorkspaceUseCase` validates
empty locations before invoking an injected `WorkspaceGateway`, which keeps
the boundary stateless and makes gateway call counts deterministic in
`NextApplicationContractTests`.

The adapter-facing legacy mapping is reserved for a later outer adapter:
v2 `openWorkspace` maps to `ApplicationServices::openDatabase(QString)` and
v2 `closeWorkspace` maps to `ApplicationServices::closeDatabase()`; QString
conversion is deferred to that adapter, and legacy QString errors are mapped
to structured `Domain::OperationError`. No Qt/DataService adapter is part of
this slice.

#### Progress update - 2026-09-19 (workspace persistence application-contract slice)

The stateless workspace contract now exposes explicit save, save-as, and export
requests, with validation of the caller-owned session and destination before
the gateway is called. The outer adapter mapping is concise and exact:

- v2 `saveWorkspace` -> `ApplicationServices::saveDatabase` (outer adapter
  turns legacy void/postcondition into a structured result)
- v2 `saveWorkspaceAs` -> `saveDatabaseAs(QString)`
- v2 `exportWorkspace` -> `exportDatabaseAs(QString)`

#### Progress update - 2026-09-19 (workspace application-state slice)

`ClassMngrNext::Application` now owns a Qt-free `WorkspaceStateSnapshot` and
`WorkspaceState`. A snapshot is a copyable value containing an optional
caller-visible `WorkspaceSession`; its lifecycle is closed or open based on
that optional session, and its unsaved state is clean or dirty. `WorkspaceState`
owns its snapshot and returns copies, so no caller, widget, page, or singleton
retains hidden current-workspace state.

The transition policy is deterministic: opening a valid session from closed or
replacing a clean session succeeds and resets the new session to clean;
invalid session input returns `InvalidInput`; dirty replacement and dirty
close return recoverable `Conflict`; dirty/saved/close operations without an
open session return `NotFound`. Marking dirty or saved is idempotent while a
session is open, and a successful close resets the snapshot to closed/clean.
`NextApplicationStateTests` covers the value and transition contract without
constructing a `QApplication`. Legacy service adapters and migration remain a
separate outer-boundary slice.

#### Progress update - 2026-09-19 (current-selection application-state slice)

`ClassMngrNext::Application` now owns a Qt-free `SelectionStateSnapshot` and
`SelectionState`. A snapshot is a copyable value backed by a variant of no
selection, `TeacherId`, `ClassId`, `CampusId`, or `CalendarEventId`. Its
`SelectionKind` and typed accessors are derived from the same variant, so one
selection cannot carry a category/value mismatch. Typed domain identifiers
remain the only accepted identifier inputs; empty raw identifiers cannot enter
this contract.

`SelectionState` is the sole owner of the current application selection and
returns copies from `snapshot()`. `setSelection` replaces the owned value and
`clear` releases it immediately. Selection is not persisted with a workspace;
the later application coordinator must clear it at workspace close or
replacement boundaries so an identifier from one workspace cannot leak into
another. This owner remains independent from workspace state and has no hidden
external lifetime or object identity dependency. `NextApplicationSelectionTests`
covers no selection, all supported categories, replacement, clearing, value
copy/equality, and compile-time category distinction without a `QApplication`.

#### Progress update - 2026-09-19 (import-job state and cancellation slice)

`ClassMngrNext::Application` now owns a Qt-free, copyable
`ImportJobSnapshot`/`ImportJobState` contract. The snapshot exposes the compact
`Idle`, `Running`, `Completed`, `Failed`, and `Canceled` lifecycle, total and
completed item counts, a cancellation-requested flag, and an optional
structured failure. `ImportJobState` accepts a new job from any non-running
phase, rejects duplicate starts with recoverable `Conflict`, rejects progress
above the total with `InvalidInput`, and only completes after all items are
reported. Terminal snapshots do not change in response to late worker events;
`start` is the explicit reset boundary for a later job.

Cancellation requests are idempotent while running. A worker cancellation
acknowledgement (or the equivalent `cancel()` event) transitions the running
job to `Canceled` and preserves its counts; a completion event wins if final
progress arrives before cancellation is acknowledged. One application owner
serializes worker events and owns this state; worker code emits events and does
not mutate the state object directly. `NextApplicationImportJobTests` covers
the lifecycle, structured failure, cancellation, terminal immutability, and
copy/equality behavior without constructing a `QApplication`.

#### Progress update - 2026-09-19 (Phase 2.6 import-review projection slice)

`ClassMngrNext::Application` now owns a Qt-free, copyable
`ImportReviewSession` projection. It contains typed teacher/class matching
indexes, category-specific decisions with explicit `Unresolved`, `Create`,
`UpdateOrReplace`, and `Skip` actions, and separate warning, unmatched-value,
and conflict collections. Teacher decisions can target only
`Domain::TeacherId`; class decisions can target only `Domain::ClassId`.
Decision and session factories reject empty or oversized source text and
contradictory action/target combinations with `Domain::ErrorCode::InvalidInput`;
explicit compact caps of 4,096 entries per index/decision/diagnostic
collection and 256 candidates per match index preserve the large-import
scenarios without allowing an unbounded projection. Adapters must paginate or
stage inputs above those caps before creating a review session.

The session is an operation-scoped value snapshot. The import/parser owner
releases raw workbook bytes, decoded worksheet buffers, and other broad
compatibility representations before or when this projection becomes
authoritative. UI code materializes only bounded review rows from the
snapshot, and releases its session copy when apply or cancel completes. The
projection retains no workbook, cells, Qt types, widgets, repositories, or
page pointers. `NextApplicationImportReviewTests` verifies these boundaries,
typed categories, readiness/conflict behavior, invalid decisions, and
copy/equality semantics without constructing a `QApplication`.

#### Progress update - 2026-09-19 (report/export-job state and output contract slice)

`ClassMngrNext::Application` now owns a Qt-free, copyable
`ReportJobSnapshot`/`ReportJobState` contract for report, PDF, and export
operations. The lifecycle is explicit (`Idle`, `Running`, `Completed`,
`Failed`, and `Canceled`) with bounded total/completed unit counts,
cancellation-requested state, optional structured failure, and a bounded
output reference populated only by successful completion. Duplicate starts,
invalid progress, incomplete completion, and blank or oversized output
references return structured errors without mutating the snapshot. Terminal
snapshots remain immutable until an explicit restart.

The state owner releases the operation-scoped render source and output
buffers after completion, cancellation, or failure; the snapshot retains only
the bounded successful output reference. Workers emit lifecycle events to the
owner instead of mutating state. The actual PDF renderer and its platform or
filesystem adapter remain outside this contract. `NextApplicationReportJobTests`
covers the lifecycle, validation, cancellation, output bound, terminal
immutability, copy/equality, and worker-boundary behavior without constructing
a `QApplication`.

#### Progress update - 2026-09-20 (report-job event bridge/coordinator slice)

`ClassMngrNext::Application` now owns a Qt-free `ReportJobCoordinator` with a
generation-tagged, bounded thread-safe FIFO sink and adapter-neutral worker
port. The coordinator alone applies progress, output-bearing completion,
cancellation acknowledgement, and failure events to `ReportJobState`; stale
generations, invalid/incomplete completions, and late terminal events cannot
mutate the active snapshot. Worker start/cancellation failures and exceptions
remain structured, while renderer and legacy adapters stay outside the slice.
`NextApplicationReportJobCoordinatorTests` covers the sink boundary, ordering,
overflow, restart isolation, cancellation races, output validation, and
terminal immutability without constructing a `QApplication`.

#### Progress update - 2026-09-19 (document-content session contract slice)

`ClassMngrNext::Application` now owns a Qt-free, copyable
`DocumentContentReference`, `DocumentContentSnapshot`, and
`DocumentContentSession` contract. References contain only bounded token, path,
or URI text; the snapshot retains the explicit `Idle`, `Requested`, `Loading`,
`Ready`, `Failed`, and `Released` phases, optional bounded reference metadata,
and an optional structured failure. Blank and oversized references are rejected
without mutating the current snapshot.

Requests replace only an idle, failed, or released session. Replacement while
requested, loading, or ready returns a recoverable conflict until the caller
releases the active session. Loading, ready, and failure events are accepted
only from their exact predecessor phases. Failure retains only the bounded
reference metadata and structured error. `release()` is the explicit boundary
at which the viewer/platform adapter must release its document object and
content bytes; the application projection then clears its reference and error.
Requests after release are supported, while late events after released or
other terminal phases are rejected without snapshot mutation.

`NextApplicationDocumentContentTests` covers lifecycle transitions, validation,
structured failure, release/re-request, late-event immutability, copy/equality,
and the no-content-bytes/no-QtPdf boundary without constructing a
`QApplication`. The remaining Phase 2 work is the application-to-legacy mapping
and coordinator integration for worker-thread ownership plus end-to-end
cancellation request/acknowledgement; the current job contracts define those
event boundaries but do not yet provide the runtime thread bridge.

#### Progress update - 2026-09-19 (import-job event bridge/coordinator slice)

`ClassMngrNext::Application` now owns a synchronous, Qt-free
`ImportJobCoordinator` with an adapter-neutral worker port and a bounded,
thread-safe FIFO event sink. Generation-tagged progress, completion,
failure, and cancellation-acknowledgement events are applied only when pumped;
stale generations and late terminal events cannot mutate a restarted/current
snapshot. Queue overflow and worker-start failures retain structured errors,
and focused app-less tests cover FIFO ordering and completion-vs-cancellation
semantics. Actual worker implementations and legacy adapters remain outside
this slice.

#### Progress update - 2026-09-19 (workspace lifecycle coordinator slice)

`ClassMngrNext::Application` now owns a synchronous, Qt-free
`WorkspaceCoordinator` that composes the workspace use case, workspace state,
and current-selection state. Create/open calls guard dirty replacement before
the gateway, commit only valid gateway sessions, and clear selection after a
successful transition. Close uses the current session, guards dirty or closed
state before the gateway, and clears selection only after a successful close;
gateway failures and invalid returned sessions leave both snapshots unchanged.
`NextApplicationWorkspaceCoordinatorTests` covers these transitions with a fake
gateway and `QTEST_APPLESS_MAIN`, without legacy service references or a
`QApplication`.

#### Progress update - 2026-09-19 (workspace persistence coordinator slice)

`WorkspaceCoordinator` now exposes synchronous save, save-as, and export
operations. Each operation requires an open session and returns structured
`NotFound` without calling the use case when the workspace is closed. Save
commits the clean state only after a successful gateway result. Save-as passes
validation through `WorkspaceUseCase`, then atomically replaces only the
current session's location and marks it clean after a valid gateway-returned
location; gateway failures and invalid returned locations leave the workspace
and selection snapshots unchanged. Export propagates the use-case result and
does not mutate either snapshot.

`WorkspaceState::markSavedAs` validates before assignment, preserving the
workspace id and keeping the replacement/clean transition atomic. The
app-less coordinator and state tests cover success, failure, closed/no-call,
validation, invalid returned locations, selection preservation, and the
existing lifecycle contract. No Qt, widget, singleton, legacy, thread, or
adapter code is part of this slice.

Windows x64 Debug verification passed with `cmake --preset
windows-x64-debug`; configure-time source ownership validated 673 handwritten
files, the focused coordinator/state CTest run passed 2/2, and the full
`ClassMngrNext*` selection passed 10/10. The resource manifest checker also
passed for six RCC packs, seven runtime IDs, and seven runtime references;
the generated Qt-link manifest keeps `ClassMngrNext` limited to `Qt6::Core`.

#### Progress update - 2026-09-20 (legacy application mapping slice)

[`phase2-legacy-application-mapping.md`](phase2-legacy-application-mapping.md)
records the verified `ApplicationServices`/`FileController` boundary, maps
workspace lifecycle and persistence to the existing v2 gateway/use-case,
coordinator, workspace-state, and selection-state contracts, and assigns the
outer-adapter duties and reversible migration gates. No legacy adapter,
FileController cutover, runtime Qt worker bridge, or feature-service migration
was added. Remaining Phase 2 work is to implement and verify those outer
boundaries, then migrate feature services as separate future slices.

#### Progress update - 2026-09-20 (document-catalog metadata projection slice)

`ClassMngrNext::Application` now owns a Qt-free, copyable
`DocumentCatalogProjection` containing bounded folder metadata and document
entries. Folder and document identifiers are distinct domain types; the
factory rejects blank or oversized metadata and references, invalid typed ids,
unknown folder links, duplicate ids, collection overflow, negative ordering,
and inconsistent exportability. Optional export references preserve absent vs
present state and use the existing `DocumentContentReference` metadata
boundary; no content bytes, viewer objects, or legacy ownership cross it.

The projection owns copied strings, identifiers, flags, and references only.
Adapters may release parser/catalog source after construction, UI consumers may
copy and release snapshots, and the adapter remains responsible for document
and export bytes plus viewer release through the content-session boundary.
`NextApplicationDocumentCatalogTests` covers valid metadata, category
distinction, validation/duplicates, optional export state, copy/equality, and
the adapter-neutral surface without constructing a `QApplication`.

#### Progress update - 2026-09-20 (document-catalog use-case slice)

`ClassMngrNext::Application` now owns a Qt-free `DocumentCatalogUseCase` that
composes a caller-owned const `DocumentCatalogProjection` with a mutable
`DocumentContentSession`. It returns copied metadata or structured `NotFound`,
delegates primary and optional-export requests to the existing content-session
contract, and returns its generation token without retaining content bytes,
viewer objects, Qt types, or mutable projection state. App-less tests cover
lookup/copy, no-mutation not-found paths, exact reference selection, token and
conflict/release behavior, error propagation, and copyable metadata results.

#### Progress update - 2026-09-20 (legacy workspace gateway seam slice)

`ClassMngrNext::Platform` now owns an injectable, Qt-free
`LegacyWorkspacePort` and stateless `LegacyWorkspaceGateway`. The gateway maps
create/open/close/save/save-as/export requests and legacy text/status results
to the existing `Application::WorkspaceGateway`, validates returned handle and
location postconditions, and preserves typed workspace identity without
retaining mutable sessions or service pointers. The explicit create-or-open
hook documents that concrete outer adapters own `QString`/`QFile` conversion
and file preparation/removal. App-less fake-port tests cover the mapping,
failure/no-mutation behavior, and the platform dependency boundary; concrete
legacy Qt conversion and FileController cutover remain later work.

Windows x64 Debug verification passed: configure-time ownership/dependency
checks validated 695 handwritten sources with `ClassMngrNextPlatform` limited
to `ClassMngrNext::Application`; the focused gateway test passed 1/1 and the
full `ClassMngrNext*` plus launch selection passed 21/21. The resource-pack
manifest check passed for six RCC packs, seven runtime IDs, and seven runtime
references; the generated Qt-link report keeps `ClassMngrNext` at
`Qt6::Core`.

## Objective

Create a stable, testable application core that is independent of widget construction, page visibility, and the legacy data facade.

## Work packages

### 2.1 Domain value types

Create explicit value types for:

- Workspace identifiers.
- Teacher and staff records.
- Class and course records.
- Class times and schedule entries.
- Students and rosters.
- Campuses and locations.
- Calendar events.
- Speaking evaluations and criteria.
- Document catalog entries, document-content references, and templates.
- Import matches and conflicts.
- User preferences.

Use typed identifiers and enums instead of unrelated strings that happen to contain IDs or state values.

### 2.2 Structured results

Replace loosely typed return values with structured results:

- Success values.
- Recoverable warnings.
- User-facing errors.
- Technical errors.
- Validation failures.
- Import conflicts.
- Cancellation state.

An import result, for example, should separately expose imported records, warnings, unmatched values, and conflicts.

### 2.3 Application use cases

Create use cases for:

- Creating, opening, closing, saving, and exporting a workspace.
- Importing a legacy database.
- Importing teachers, schedules, calendars, rosters, and class transfers.
- Editing teachers, classes, schedules, calendar events, rosters, and evaluations.
- Generating reports and substitute documents.
- Listing campus and document metadata.
- Opening document content on demand for a viewer or output operation.
- Performing backups and recovery.
- Checking for application updates after startup.

The Sub Prep slice also requires explicit summary, selected-detail, and
operation-scoped print-source contracts. Their feature-level implementation
plan is tracked in [Sub Prep Class Information and Output Memory
Plan](sub-prep-class-information-memory-plan.md).

Each use case must have:

- Explicit input.
- Explicit output.
- Structured errors.
- No widget references.
- No hidden singleton state.
- A deterministic test boundary.

### 2.4 Validation and business rules

Move validation rules into domain or application services.

The UI may display validation results and choose when to validate, but it must not own the business rule implementation.

Preserve current validation messages and behavior until visual and behavioral parity is accepted.

### 2.5 State and concurrency

Define:

- Workspace session state.
- Unsaved-change state.
- Current selection state.
- Import-job state.
- Report/export-job state.
- Document-content session state: requested, loading, ready, failed, and released.
- Cancellation behavior.
- Thread ownership.

Background work must return results through application interfaces. Worker code must not mutate widgets directly.

### 2.6 Memory-safe projections and operation contracts

The [Qt Rewrite Memory Hotspot Remediation
Plan](memory-hotspot-remediation-plan.md) defines the compact contracts needed
by the large-data slices. Add application-facing projections equivalent to:

- compact class and teacher summaries plus selected class details;
- a compact schedule view projection;
- an import review session containing matching indexes and compact decisions,
  not the original workbook and every derived UI object;
- staged transfer-reader and transfer-writer records;
- operation-scoped report and PDF render sources.

Every contract must state which layer owns the data, when raw or derived
representations may overlap, and when they are released. Contracts must not
return widget trees, page pointers, or broad compatibility-service snapshots.

## Deliverables

- Domain model library.
- Application use-case library.
- Structured error and warning types.
- Validation services.
- Application-state definitions.
- Domain tests that run without a QApplication.
- Mapping document from old service calls to new use cases.

## Exit gate

Domain and application behavior can be tested without constructing the main window.

Validation, conflict detection, import planning, and state transitions match the baseline fixtures.

For the workspace boundary, acceptance includes the current
`WorkspaceGateway::createWorkspace` contract and `WorkspaceCoordinator` create
behavior: dirty replacement is rejected before the gateway, a successful
session opens `WorkspaceState` and clears `SelectionState`, and gateway or
invalid-session failures preserve both snapshots. The existing app-less
`NextApplicationContractTests` and `NextApplicationWorkspaceCoordinatorTests`
cover these create paths alongside open, close, save, save-as, and export.

No new v2 production path depends on DataService, MainWindow, PageManager, or a widget pointer.

## Heavy-route requirements

- For every Phase 2 slice, use the heavy route: implement the contract across
  its intended v2 boundary, verify it with the owning layers, and remove any
  temporary compatibility wrapper when the slice is accepted.
- Convert core contracts rather than wrapping every old Qt type indefinitely.
- Keep Qt conversion at the UI, filesystem, or platform boundary.
- Prefer explicit immutable snapshots for read models.
- Keep document-content contracts independent of QtPdf; the viewer/platform
  adapter owns the active document session and its release boundary.
- Do not hide business rules inside presenters or delegates.
- Do not allow compatibility methods to become the permanent v2 API.

#### Progress update - 2026-09-20 (class-summary projection slice)

`ClassMngrNext::Application` now owns a Qt-free, copyable
`ClassSummaryProjection` with bounded `TeacherSummary` records, compact
`ClassSummary` navigation rows, and at most one `SelectedClassDetails` value.
The factory rejects blank or oversized required text and typed identifiers,
duplicate IDs, unknown teacher references, invalid selected-class identity,
collection overflow, and student-count overflow. An absent class teacher is
an explicit supported fallback; present references must resolve in the
`TeacherSummaryIndex`. Lookups return value copies, while the projection
retains no rosters, repositories, widgets, page pointers, or rich record
graph. The query/adapter owner may release rich source data after creation;
app-less focused tests cover the 96-class scale, metadata, validation,
copy/equality, missing-teacher behavior, and the ownership boundary.

#### Progress update - 2026-09-20 (schedule-view projection slice)

`ClassMngrNext::Application` now owns a Qt-free, copyable
`ScheduleViewProjection` made of bounded visible `ScheduleViewRow` and
`ScheduleViewCell` values. The factory validates nonnegative ordering/day/slot
values, unique row/cell identities, bounded labels and display text, optional
typed class/teacher references, regular/intensive mode, and row/cell caps.
Empty snapshots, rows, and time-slot cells are explicit valid fallbacks;
lookups return value copies. Query owners must paginate or stage larger
visible scopes and may release rich classes, rosters, repositories, and raw
workbook data after projection creation. App-less tests cover the 96-row /
768-cell scale, ordering, missing references, invalid input, caps, and the
no-rich-record contract boundary.

#### Progress update - 2026-09-20 (staged class-transfer projection slice)

`ClassMngrNext::Application` now owns a Qt-free, copyable
`ClassTransferProjection` with bounded flat teacher/class records, source-key
matching, optional missing-teacher references, and separate regular/intensive
time collections. The deterministic factory rejects blank or oversized text,
duplicate or unknown keys, negative ordering, and teacher/class/time/package
collection overflow with structured `InvalidInput`; lookups return value
copies. Reader adapters stage only this compact package, release raw source
representations before projection handoff, and must paginate larger inputs.
App-less tests cover the boundary, caps, categories, fallback, equality, and
no external-owner/raw-source accessors; legacy transfer models and codecs are
outside this slice.

#### Progress update - 2026-09-20 (campus directory metadata projection slice)

`ClassMngrNext::Application` now owns a Qt-free, copyable
`CampusDirectoryProjection` of bounded `CampusSummary` navigation metadata:
typed `Domain::CampusId`, display name, short key, address, optional notes,
nonnegative order, and active state. Deterministic create/validate rejects
blank or oversized required fields, invalid optional notes, duplicate IDs or
keys, negative order, and entry overflow with structured `InvalidInput`;
empty directories are valid and lookup misses return `nullopt`. ID/key
lookups return value copies. Query/adapters must paginate or stage above the
cap and may release rich campus/location records after projection creation;
the projection has no Qt, repository, widget/page, pointer, or raw-byte state.
The app-less test covers 96 entries, exact-cap acceptance, validation,
copy/equality, metadata retention, and the ownership boundary.

#### Progress update - 2026-09-20 (calendar-event projection slice)

`ClassMngrNext::Application` now owns a Qt-free, copyable
`CalendarEventProjection` of bounded `CalendarEventSummary` event-list
metadata. The contract retains typed calendar-event IDs plus optional typed
class/campus references, bounded title/date/time/location/notes text,
nonnegative ordering, and an explicit all-day policy: all-day events omit
times, while timed events may omit both unknown times but not a partial range.
Dates and times remain opaque adapter-neutral text.

Deterministic create/validate rejects blank or oversized identifiers and
required fields, invalid optional references or time combinations, duplicate
IDs, negative ordering, and collection overflow with structured
`InvalidInput`; empty projections and value-copy ID lookups are explicit.
Query/adapter owners release rich calendar records, recurrence state, and
service data after projection creation and stage or paginate above the cap.
`NextApplicationCalendarEventTests` covers 96-scale metadata, typed
references, temporal policy, validation, caps, copies/equality, safe lookups,
and the no-rich-record boundary without constructing a `QApplication`.

#### Progress update - 2026-09-20 (user-preferences state contract slice)

`ClassMngrNext::Application` now owns a Qt-free, copyable
`UserPreferencesSnapshot`/`UserPreferencesState` for explicit theme and
language preferences, sidebar display flags, teacher and PowerPoint notice
flags, automatic update checks, supported Excel timeout values, and an
optional validated typed `Domain::CampusId` last selection. Defaults match
the verified legacy defaults; invalid enum, timeout, and campus updates return
structured `InvalidInput` errors without mutating the snapshot, and campus
selection is explicitly clearable. The contract intentionally does not retain
the separate legacy JSON campus key: the later outer adapter owns any mapping
between storage keys and the typed campus identifier. App-less tests cover
defaults, all values, validation, copies/equality, value-copy access, and the
absence of persistence, UI, singleton, and legacy-service dependencies.

#### Progress update - 2026-09-20 (ApplicationServices workspace-port slice)

`src/next/platform/application_services_workspace_port.h` now implements
`ApplicationServicesWorkspacePort` behind `LegacyWorkspacePort`. UTF-8/path
normalization, explicit non-destructive `createOrOpen`, close/open
postconditions, the void-save postcondition, save-as through the legacy call
plus reopen while preserving identity, export non-mutation, and legacy error
mapping are covered by real `ApplicationServices` + `QTemporaryDir` tests in
`tests/next_platform_application_services_workspace_port_tests.cpp`.

Configure/ownership checks validated 697 files; the `ClassMngrNextPlatform`
dependency remains limited to `ClassMngrNext::Application`. The focused
build/CTest passed 1/1 with all 9 slots; at that point `FileController`
remained untouched. Invalid UTF-8 and stale/closed save-as/export boundaries
are non-blocking but untested. FileController integration, the runtime
bridge, and feature-service migration remained for subsequent slices.

#### Progress update - 2026-09-20 (FileController workspace lifecycle slice)

`FileController` now owns the concrete
`ApplicationServicesWorkspacePort` -> `LegacyWorkspaceGateway` ->
`WorkspaceUseCase` -> `WorkspaceCoordinator` composition when services are
available, with an out-of-line destructor that permits incomplete service
types. `loadDatabase` closes the current coordinator session before opening
the replacement and returns `false` when that close fails. `closeFile` now
preserves the current file and UI state when coordinator close fails. The
structured open error is converted with explicit UTF-8 decoding. A null
`ApplicationServices` pointer remains safe, and create/initial-setup opens
retain the legacy fallback path.

`FileControllerWorkspaceLifecycleTests` is an offscreen QTest using public
`loadDatabaseOnStartup`, real `ApplicationServices`, and valid
`QTemporaryDir` workspaces. It asserts normalized recent/last-file settings,
missing-file warning capture, legacy open error text, non-empty null-service
safety, and sequential coordinator/legacy fallback. There is no direct
injected FileController close-failure/legacy-operation integration test;
lower-layer tests and source inspection cover that behavior.

The final independent bounded Debug CTest selection passed 9/9: FileController
lifecycle, startup visual settings, data service lifecycle, startup
performance, legacy workspace gateway, ApplicationServices workspace port,
and the workspace coordinator-related targets. Focused build, configure,
source-ownership, and dependency checks passed, and `git diff --check` passed.

Save, save-as, and export remain legacy. The runtime bridge and feature-service
slices remain open.

#### Progress update - 2026-09-20 (FileController create/initial-setup coordinator slice)

`FileController` now keeps explicit ownership of normal-create replacement
(`QFile::remove`) and initial-setup backup rename/cleanup, checks
`closeActiveDatabase()` before either destructive preparation, and routes the
successful prepared path through `WorkspaceCoordinator::createWorkspace` with
an explicit UTF-8 `WorkspaceLocation`. Structured domain errors are decoded
with `QString::fromUtf8` before the existing warning title/message policy is
used. Initial-setup cancellation now also leaves its path and UI state intact
when close fails. Save, save-as, and export calls remain on
`ApplicationServices`.

`FileControllerWorkspaceLifecycleTests` now uses `FakeFileDialogService`, fake
prompts, real `ApplicationServices`, and `QTemporaryDir` to cover normal
creation/recent updates, existing-target replacement, close-failure
non-destructive behavior, initial-setup backup/open/finish and cancel restore,
and structured create-error propagation while retaining the prior open/close,
recent, warning, and null-service coverage. Source inspection confirms the
legacy save/save-as/export call sites remain unchanged.

`cmake --preset windows-x64-debug` passed and validated one explicit target
owner for 698 handwritten source files; the CMake dependency guard and
generated Qt-link report keep `ClassMngrNext` at `Qt6::Core`. The focused
Debug target build passed, the focused lifecycle CTest passed 1/1, and the
bounded regression selection passed 9/9:
`ClassMngrStartupVisualSettingsTests`, `ClassMngrDataServiceLifecycleTests`,
`ClassMngrStartupPerformanceTests`, `ClassMngrNextApplicationContractTests`,
`ClassMngrNextApplicationStateTests`,
`ClassMngrNextApplicationWorkspaceCoordinatorTests`,
`ClassMngrNextPlatformLegacyWorkspaceGatewayTests`,
`ClassMngrNextPlatformApplicationServicesWorkspacePortTests`, and
`ClassMngrFileControllerWorkspaceLifecycleTests`. The resource-pack check
passed for six RCC packs, seven runtime IDs, and seven runtime references;
`git diff --check` passed.

The remaining gates are the legacy FileController save, save-as, and export
paths, including their stale/closed and invalid-UTF-8 boundary coverage, plus
the runtime worker-thread/cancellation bridge and later feature-service
migration slices.

#### Progress update - 2026-09-20 (FileController save/autosave coordinator slice)

`FileController::saveDatabase` now preserves the existing no-service and
no-open early return, dispatches a coordinator-owned workspace session through
`WorkspaceCoordinator::saveWorkspace`, and reports structured failures with
the existing warning service/title policy (`Save Teacher Profile`). The shared
UTF-8 decoder is used for domain error text. A failed coordinator save does
not alter workspace state, current-file/UI state, or invoke a false clean
transition. When the v2 state is closed but `ApplicationServices` is already
open through the compatibility path, the historical void
`ApplicationServices::saveDatabase` fallback remains active. Save-as and
export remain on their legacy service calls.

`FileControllerWorkspaceLifecycleTests` retains the prior lifecycle/create
coverage and adds real `ApplicationServices`/`QTemporaryDir`/fake-prompt
coverage for coordinator save success, stale-session structured failure with
warning and repeated state preservation, and closed-v2 compatibility fallback.
Source assertions keep save-as/export on the legacy calls and reject v2
save-as/export dispatch in this slice.

`cmake --preset windows-x64-debug` passed, validating one explicit owner for
698 handwritten source files; the CMake dependency guard passed, and the
generated Qt-link report keeps `ClassMngrNext` at `Qt6::Core`. The focused
Debug target build passed, the focused lifecycle CTest passed 1/1, and the
bounded regression selection passed 9/9:
`ClassMngrStartupVisualSettingsTests`, `ClassMngrDataServiceLifecycleTests`,
`ClassMngrStartupPerformanceTests`,
`ClassMngrNextApplicationContractTests`,
`ClassMngrNextApplicationStateTests`,
`ClassMngrNextApplicationWorkspaceCoordinatorTests`,
`ClassMngrNextPlatformLegacyWorkspaceGatewayTests`,
`ClassMngrNextPlatformApplicationServicesWorkspacePortTests`, and
`ClassMngrFileControllerWorkspaceLifecycleTests`. `git diff --check` passed
(with only the existing LF-to-CRLF warnings from Git).

The remaining Phase 2 gates are FileController save-as/export migration and
their stale/closed/invalid-UTF-8 coverage, the runtime worker-thread and
cancellation bridge, and later feature-service migration. No feature gate or
runtime bridge was changed here; this slice is ready for its separate commit.

#### Progress update - 2026-09-20 (FileController save-as coordinator slice)

`FileController::saveDatabaseAs` now preserves the existing normalization,
no-service/no-open guard, dialog request, and `Save Teacher Profile` warning
policy. When `WorkspaceState` owns a session it passes the normalized path as
an UTF-8 `WorkspaceLocation` to `WorkspaceCoordinator::saveWorkspaceAs`,
decodes structured UTF-8 failures without changing workspace, selection,
current-file, recent-file, or loaded UI state, and commits the returned
normalized location directly to `m_currentFile`, recent/last-file settings,
and loaded UI state. It does not reopen through `loadDatabase`. A closed v2
state with an already-open compatibility service retains legacy
`saveDatabaseAs` followed by `loadDatabase`; export remains on the legacy
service call.

`FileControllerWorkspaceLifecycleTests` retains the previous create/open/
close/save coverage and now deterministically exercises v2 save-as success and
location/recent/UI updates, stale-session structured failure and recovery,
closed-v2 legacy fallback, dialog policy, and source assertions that export
has not migrated.

Verification passed: `cmake --preset windows-x64-debug` validated one explicit
owner for 698 handwritten sources and the CMake dependency guards; the
generated Qt-link report keeps `ClassMngrNext` at `Qt6::Core`. The focused
Debug target build passed, focused CTest passed 1/1, and the bounded regression
selection passed 9/9 (`ClassMngrStartupVisualSettingsTests`,
`ClassMngrDataServiceLifecycleTests`, `ClassMngrStartupPerformanceTests`,
`ClassMngrNextApplicationContractTests`, `ClassMngrNextApplicationStateTests`,
`ClassMngrNextApplicationWorkspaceCoordinatorTests`,
`ClassMngrNextPlatformLegacyWorkspaceGatewayTests`,
`ClassMngrNextPlatformApplicationServicesWorkspacePortTests`, and
`ClassMngrFileControllerWorkspaceLifecycleTests`). `git diff --check` passed
with only the existing LF-to-CRLF warnings.

The remaining gates are FileController export migration and its boundary
coverage, the runtime worker-thread/cancellation bridge, and later
feature-service migration. No export implementation, platform adapter, v2
contract, memory document, or unrelated code changed in this slice; this slice
is ready for a separate commit.

#### Progress update - 2026-09-20 (FileController export coordinator slice)

`FileController::exportDatabaseAs` now preserves native-output normalization,
the no-service/no-open guard, the existing export dialog, the `Export Teacher
Profile` warning title, and directory remembering. When `WorkspaceState` owns
a session it passes an explicit UTF-8 `WorkspaceLocation` to
`WorkspaceCoordinator::exportWorkspace`; success remembers only the returned
normalized destination directory. It does not change `m_currentFile`,
workspace or selection state, recent/last-file settings, or loaded UI state.
Structured failures decode their UTF-8 message before warning. When v2 state is
closed while `ApplicationServices` is already open, the legacy export call
remains the compatibility fallback.

`FileControllerWorkspaceLifecycleTests` retains all prior lifecycle, create,
save, and save-as coverage and adds FakeFileDialogService/real
ApplicationServices/fake-prompt coverage for v2 export success and
non-mutation, repeated stale-session failure and warning preservation, legacy
compatibility fallback, dialog policy, and the no-open guard. Source checks
continue to assert that save and save-as remain on their migrated paths and
that the legacy export call remains available only as the fallback branch.

Verification passed: `cmake --preset windows-x64-debug` validated one
explicit owner for 698 handwritten source files and the CMake dependency
guards; `build/windows-x64-debug/reports/qt-module-links.json` keeps
`ClassMngrNext` at `Qt6::Core`. The focused Debug target
`ClassMngrFileControllerWorkspaceLifecycleTests` built successfully, focused
CTest passed 1/1, and the bounded regression selection passed 9/9
(`ClassMngrStartupVisualSettingsTests`,
`ClassMngrDataServiceLifecycleTests`, `ClassMngrStartupPerformanceTests`,
`ClassMngrNextApplicationContractTests`, `ClassMngrNextApplicationStateTests`,
`ClassMngrNextApplicationWorkspaceCoordinatorTests`,
`ClassMngrNextPlatformLegacyWorkspaceGatewayTests`,
`ClassMngrNextPlatformApplicationServicesWorkspacePortTests`, and
`ClassMngrFileControllerWorkspaceLifecycleTests`). The resource-pack check
validated 6 RCC packs, 7 runtime IDs, and 7 runtime references. `git diff
--check` passed with only the existing LF-to-CRLF warnings.

The remaining Phase 2 gate is later feature-service migration. No platform
adapter, v2 contract, memory document, or unrelated feature changed in this
slice; the worktree remains uncommitted.

#### Progress update - 2026-09-20 (Qt runtime worker/cancellation bridge slice)

`ClassMngrNext::Platform` now has QtCore-only `QtJobWorkerLifetime` plus typed
`QtImportJobWorker` and `QtReportJobWorker` adapters over the existing Qt-free
`Application` worker ports. Work runs on joined, non-detached `QThread`s;
cancellation is cooperative and asynchronous; generation-tagged events are
posted to bounded queues; owner coordinators remain the only state mutators;
task, exception, and post failures are structured and observable; and
destruction joins before releasing task/sink captures.

Focused deterministic QtTest coverage for both adapters includes off-thread
execution, completion, cancellation, failure/exception, overflow visibility,
restart, late-cancel, and destructor join. Configure/ownership checks passed
for 701 sources with dependency guards; `ClassMngrNextPlatform` depends on
`ClassMngrNext::Application` and `Qt6::Core`, while `Application` remains
Qt-free. The generated `ClassMngrNextPlatformQtJobWorkerTests` CTest target
passed 1/1, the exact 9-target regression passed 9/9, and the resource report
validated 6 RCC packs, 7 runtime IDs, and 7 references. `git diff --check`
passed. An initial unsandboxed MSBuild FileTracker `E_ACCESSDENIED` required
an elevated retry and then passed. Stress/TSAN coverage and a direct report
queue-post-failure test remain non-blocking gaps.

The remaining Phase 2 gate is later feature-service migration; the worktree
remains uncommitted.

#### Progress update - 2026-09-20 (document-catalog adapter and document-route slice)

`ClassMngrNext::Platform` now provides an
`ApplicationServicesDocumentCatalogPort` that maps legacy `DocumentCatalog`
metadata into bounded, typed `DocumentCatalogProjection` values.
`NavigationController` resolves only its document route through that port and
`DocumentCatalogUseCase`, while preserving confirm-leave behavior, the
`ResourcePaths` document lease, `PdfViewerDocumentDescriptor`, viewer loading,
and page navigation. `MainWindow`/`Sidebar` catalog ownership and document
content-byte/session loading remain legacy and open; this is not a full
document-service migration or Phase 2 completion.

Verification passed: configure/ownership checks validated 703 sources with
dependency guards for `Platform -> Application + Qt6::Core` and
`ClassMngrNext -> Qt6::Core`; focused CTest passed 1/1; the exact 9-target
regression passed 9/9; the resource report validated 6 RCC packs, 7 runtime
IDs, and 7 references; and `git diff --check` passed. A fresh MSBuild
FileTracker access-denied was an environment/toolchain issue; the isolated
build/regression passed. Malformed-catalog fixture injection remains a
non-blocking gap. The worktree remains uncommitted.
