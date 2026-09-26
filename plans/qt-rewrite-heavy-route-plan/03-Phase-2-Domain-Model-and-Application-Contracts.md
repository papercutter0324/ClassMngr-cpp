# Phase 2 — Domain Model and Application Contracts

## Status

- Status: In progress
- Default route: Heavy
- Depends on: Phase 1
- Blocks: Persistence, bootstrap, shared UI, and feature migration
- Owner: Unassigned
- Last updated: 2026-09-26
- Earlier code slice: F70 moves Class Transfer preview matching into a Qt-free
  Application policy (`matchClassTransferCandidates`). The repository adapter retains Qt
  simplified/case-folded input normalization and legacy integer preview presentation.
  App-less tests cover matching rules and order; production tests preserve normalized
  matching, checked-in success/conflict preview IDs, and conflict no-write behavior.
  Two fresh Windows x64 Debug Ninja/MSVC 19.51.36257/Qt 6.12 trees validated 917
  handwritten source owners and passed the application and production Class Transfer
  CTests 2/2. Fixture parity is verified; Unicode case-fold equivalence is not exhaustive.
  Gate 1 and Gate 2 remain Partial. Source commit: `2f3d414c`.
- Earlier code slice: F71 gives Class Transfer review match and issue identities typed
  `Domain::ClassId`/`TeacherId` values and optional typed resolution targets. UI and
  repository adapters retain legacy integer APIs, map positive IDs and exactly `-1`,
  reject `0`/`-2` with existing action-specific errors, and preserve invalid-action
  precedence. App-less validators retain action, membership, duplicate, and missing-target
  rules. The checked-in success fixture now exercises a nonempty exact teacher/class
  match and dialog-selected Replace, asserting retained identity/profile, replaced class
  details/schedule/roster, and cleared evaluation; Create and conflict/no-write fixtures
  remain. UI/repository sentinel tests cover their assigned matrices. Two fresh Windows
  x64 Debug Ninja/MSVC 19.51.36257/Qt 6.12 trees validated 917 handwritten source owners
  and passed the app-less and production Class Transfer CTests 2/2; diff check passed,
  no full suite. Gate 1 and Gate 2 advance but remain Partial. Source commit: `9b090cb5`.
  Phase 2 exit gate remains Open. Sub Prep remains limited to the current and following
  calendar years, 2026-2027.
- Previous code slice: F72 adds production parity coverage for Class Transfer teacher
  replacement. The checked-in `success_source.json` test selects Teacher
  ReplaceExisting using dialog action/target metadata, then verifies the retained
  teacher identity/profile, absence of a duplicate, and class linkage. Two fresh
  Windows x64 Debug Ninja/MSVC 19.51.36257/Qt 6.12 trees validated 917 handwritten
  source owners and passed both Class Transfer CTests 2/2; diff check passed, no full
  suite. Gate 2 gained fixture parity; Gate 1 was unchanged. Test-only commit:
  `aa1af5fe`.
- Previous code slice: F73 extends the checked-in `conflict_source.json` production
  fixture test. Default schedule-collision rejection/no-write coverage remains;
  the test also selects class Skip and teacher ReplaceExisting through production
  dialog plan metadata. Import succeeds with one skipped class and no
  created/replaced class; teacher profile, class details and ClassInfo schedules,
  roster, speaking evaluation, and record counts remain unchanged. The source
  commit changes only `tests/class_transfer_tests.cpp`. Executor and independent
  fresh Tester Windows x64 Debug Ninja 1.13.2/MSVC 19.51.36257/Qt 6.12.0 trees
  each validated 917 source owners and passed
  `ClassMngrNextApplicationClassTransferTests` and `ClassMngrClassTransferTests`
  (2/2); Tester also ran the fixture test directly. `git diff --check` passed;
  no full suite. F73 adds Gate 2 fixture-backed action evidence; Gate 1 is
  unchanged and both remain Partial. Test-only commit: `60bbd015`.
- Latest code slice: F74 adds synthetic Intensive Schedule Import production-flow
  coverage: fixed preview values, explicit UpdateExisting apply, persisted target
  rows/class identity, no class creation, untouched Intensive row ID/value, and
  unchanged regular rows. No historical Intensive workbook or legacy-output
  oracle exists, so this is not historical baseline parity. Independent fresh
  Windows x64 Debug Ninja/Qt 6.12 Executor and Tester trees passed the three
  focused Schedule Import CTests (3/3); no full suite. Gate 1 is unchanged and
  Gate 2 remains Partial. Test-only commit: `3e0a8d64`.
- Current note: Replace implicit behavior and UI-coupled service calls with explicit contracts. The typed Domain slice, workspace persistence/state contracts, current-selection state owner, import-job lifecycle contract, report/export-job lifecycle contract, document-content session contract and partial PdfViewerPage/NavigationController integration, legacy application mapping document, Qt-free legacy workspace gateway seam, concrete ApplicationServices workspace port, FileController open/close/create/initial-setup/save/save-as/export integration, Qt runtime worker/cancellation bridge, bounded resource/platform document resolver, typed Sidebar/MainWindow catalog cutover, language preference bridge, schedule-output direct-theme boundary, calendar database-query/worker ownership separation, narrow typed calendar cache/model boundary, narrow typed upcoming-events retrieval and next-ten prefetch read cutovers, typed calendar activation reads, the typed non-repeat save, repeat-occurrence save, new-repeat series-create, single-event delete, repeat-series suffix-delete, this-and-following repeat-series edit/save, calendar-dialog edit-draft, and calendar-dialog constructor/input ownership seams, typed Calendar Import planning, signature queries, ordered batch save, and use case are implemented, with signatures flowing as typed values through parser, query, plan, and use-case boundaries. `CalendarEventCache` retains typed `CalendarEventSummary` values and exposes date-scoped and range-scoped typed projections; `eventsForDate`/`eventsInRange` remain legacy compatibility paths for other callers, while `CalendarPage::ensureNextTenEvents` uses the typed range projection. `CalendarEventModel` consumes the typed projection and summary values for QML rows, converting dates, times, and `QVariant` only at the UI boundary; `calendar_page_events.cpp` passes typed summary values directly into `CalendarEventEditDraft` on activation and creates drafts for new events; edit and mutation paths no longer round-trip through a legacy `CalendarEvent` record, consumes drafts for all typed save/series-create/edit requests, and retains typed next-ten retrieval, typed by-ID activation reads, typed non-repeat save and delete, typed repeat-occurrence save, typed new-repeat series creation, typed repeat-series suffix-delete, and typed this-and-following repeat-series edit/save calls. `CalendarEventDialog` stores and returns the draft while legacy conversion remains private to its implementation. `repeatedCalendarEvents` generation and existing typed edit/save/delete/dialog paths remain preserved; defaults, validation, inline errors, warnings, repeat/delete/mutation routing, `schedule_use_24h`, invalidation/refresh, edit-dialog ownership, schedule settings, other legacy callers, and integer-ID semantics remain unchanged. Sidebar owns a copied/move-assigned `Application::DocumentCatalogProjection` without a legacy `DocumentCatalog` pointer, include, or dependency; MainWindow requests locale-specific projections through `Platform::ApplicationServicesDocumentCatalogPort` and passes them by value. The application layer remains Qt-free. Broader typed calendar UI/page migration, generic settings persistence, remaining feature-service migrations, and broader document-service migration remain open. Invalid-UTF-8 boundary coverage and live UI integration are non-blocking and not directly covered; no live MainWindow projection-failure/retranslation integration test exists. Sub Prep interval coverage remains limited to the current and following calendar years at most.

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

### Exit-gate status after F48 - 2026-09-25 (commit `2055bbb5f74842e4f146a48e211df58e65908b6b`)

This audit applies the formal criteria above to the verified F36/F38/F39/F40/F41/F42/F43/F44/F45/F46/F47/F48 evidence. F48's independent fresh-build and focused-test evidence is recorded below; no tests were rerun for this documentation update.

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior | Partial | F40 adds app-less `Domain::Weekday`/`Domain::ScheduleTime`; F41 and F43 add Schedule and Class Transfer review-decision behavior, F44 adds Teacher Import review-decision validation, F45 adds `Domain::Course`, F46 adds `Domain::KoreanTeacherKey`, F47 adds typed `Domain::Course::WeeklyMeetingDayRule`, and F48 adds typed `Domain::ScheduleEntry` with distinct ClassId/TeacherId types and validated ScheduleTime. Broader Domain records remain incomplete. |
| Baseline parity | Partial | Required fixtures cover F36 Calendar import, F38 Schedule preview, F39 conflict review/apply, F41 Schedule review through persisted apply, F43 Class Transfer persistence/conflict, F44 Teacher Import review through persisted apply, F45 valid Schedule apply/invalid-course rejection, F46 Teacher Import and Schedule matching/persistence plus overlap rejection, F47 accepted Schedule persistence plus prohibited-pattern rejection before writes, and F48 persistence facts checked against resolved typed entries. F37's pre-write sentinel and F39/F43/F44/F45/F46/F47/F48 no-write cases add state evidence; F48 also verifies overlap rejection preserves seeded teacher, class, class_info, class_times, and app_settings snapshots. Wider baseline parity remains incomplete. |
| Workspace boundary | Satisfied | The formal criterion is the `WorkspaceGateway::createWorkspace`/`WorkspaceCoordinator` behavior stated above: dirty replacement is rejected before the gateway, success opens `WorkspaceState` and clears `SelectionState`, and gateway/invalid-session failures preserve both snapshots. The focused app-less workspace CTests verify these cases. F42 additionally verifies failed production replacement-open preservation and abort-before-target-preparation if close fails; F43/F44/F45/F46/F47/F48 do not change this criterion. FileController integration limitations remain separate and non-gating. |
| v2 dependency isolation | Satisfied in the audited v2 scope | The `src/next` source scan and target dependencies found no direct `DataService`, `MainWindow`, `PageManager`, or widget-pointer dependency. F43's Class Transfer, F44's Teacher Import, F45/F47's `Domain::Course`, F46's `Domain::KoreanTeacherKey`, and F48's `Domain::ScheduleEntry` contracts have no Qt or legacy Application dependencies; outer adapters bridge legacy services, and `FileController` remains MainWindow-aware. |

Gate 1 (app-less Domain/Application behavior) and Gate 2 (baseline parity)
advance with F48 but remain Partial. F48 was independently verified using
fresh x64 MSVC 19.51.36257/Ninja 1.13.2/CMake 4.4.2/Qt 6.12.0 builds; both
focused CTests passed 2/2. Direct QtTest results were Domain 17/0/0 and Schedule
Import 26/0/1, with only the optional external-workbook sample skipped. The
required `schedule_review.xlsx` path checked persisted SQL values against
resolved `ScheduleEntry` values, and the expanded overlap-rejection test
confirmed all five seeded snapshots remained unchanged. CMake ownership covered
902 handwritten files; `git diff --check` passed and the user-owned
`cmake/sources.cmake` SHA-256 remained unchanged. Remaining work includes wider
baseline parity, shared workbook decoding, broader Domain records, generic
settings persistence, remaining feature-service migrations, and broader
calendar/UI and document work. The Sub Prep interval query remains capped at
the current and following calendar years at most.
Non-gating FileController integration gaps remain: dirty-page approval still
comes from `MainWindow`; New Profile and Initial Setup replacement can close the
active session before target preparation and coordinator create have succeeded;
and same-path replacement plus the complete live MainWindow snapshot lack direct
coverage. Phase 2 remains In Progress and the formal exit gate remains open.

### Exit-gate status after F49 - 2026-09-25 (commit `6a41e958671b7fa93c301d8b25c9c4381178fd7f`)

This audit adds the independently verified F49 Calendar Import use-case
evidence to the F48 audit above; no tests were rerun for this documentation
update.

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior | Partial | F49 adds a Qt-free use case that composes the existing signature-query port, duplicate planner, and batch-save port. Six fake-port cases cover ordering, duplicate handling, parser skips, empty and duplicate-only candidates, exact UTF-16 signatures, and query/save failures. Broader Domain records remain incomplete. |
| Baseline parity | Partial | The required `calendar_import_parity_2026.xlsx` fixture exercises the modified production service and verifies persisted rows and counts. This advances Calendar Import parity; wider baseline parity remains incomplete. |
| Workspace boundary | Satisfied | The formal `WorkspaceGateway::createWorkspace`/`WorkspaceCoordinator` criterion and its prior focused app-less coverage are unchanged by F49. FileController integration limitations remain separate and non-gating. |
| v2 dependency isolation | Satisfied in the audited v2 scope | The F49 use case remains Qt-free and composes adapter-neutral ports; workbook/network/campus handling and localized errors stay at the feature edge. Existing audited `src/next` isolation remains satisfied. |

F49 was independently checked in fresh Executor and Tester builds; both
focused CTests passed 2/2, including the production fixture path. No full suite
was run. Wider baseline parity, shared workbook decoding, broader Domain
records, generic settings persistence, remaining feature migrations, and
broader calendar/UI and document work remain open. Phase 2 remains In Progress
and the formal exit gate remains open. Sub Prep remains capped at the current
and following calendar years at most.

### Exit-gate status after F50 - 2026-09-25 (commit `92d001db11d8c8eb973de5f238444abe855ea5c5`)

This audit adds the independently verified F50 signature-identity and
Calendar Import fixture evidence to the F49 audit above; no tests were rerun
for this documentation update.

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior | Partial | F50 adds a Qt-free six-field signature value shared by the parser and query port. App-less cases verify formatting, all fields, exact UTF-16 code units, `%2` title text, and type members without metadata. This advances a narrow Calendar Import behavior contract; broader Domain records remain incomplete. |
| Baseline parity | Partial | Required `calendar_import_parity_2026.xlsx` production parity passed through the Calendar Import path. This advances fixture-backed import parity; wider baseline parity remains incomplete. |
| Workspace boundary | Satisfied | The formal workspace create criterion and its prior coverage are unchanged by F50. |
| v2 dependency isolation | Satisfied in the audited v2 scope | The shared signature value and query contract remain Qt-free; string normalization and date conversion remain at the Qt adapters. Existing audited `src/next` isolation remains satisfied. |

Executor and independent Tester each freshly configured Windows x64 MSVC/Ninja,
validated 906 source owners, built three focused targets, and passed CTest 3/3,
including the required fixture path. No full suite was run. Wider baseline
parity, shared workbook decoding, broader Domain records, generic settings,
remaining feature migrations, and broader calendar/UI and document work remain
open. Phase 2 remains In Progress with its exit gate Open. Sub Prep remains
capped at the current and following calendar years at most.

### Exit-gate status after F56 - 2026-09-26 (commit `c73e896fe34e186a045d73b653aa8ec9dfa89e83`)

This audit adds F56 speaking-evaluation grade-contract and production-path
evidence to the F50 audit above; no tests were rerun for this documentation
update.

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior | Partial | `Domain::SpeakingEvaluationGrade` represents six criteria and C/B/B+/A/A+ values, parses exact labels, and centralizes legacy aggregation, rounding, invalid/missing outcomes, and clamping. Exhaustive valid-combination and boundary coverage adds evidence; broader Domain/Application behavior remains incomplete. |
| Baseline parity | Partial | Roster widget import verifies a padded saved label, incomplete evaluation to N/A, retained 16/6-to-B+ result, persistence, and idempotence. The report widget path verifies B+ and N/A through assembly/rendering. Wider baseline parity remains incomplete. |
| Workspace boundary | Satisfied | F56 does not change the formal workspace create criterion or its focused app-less coverage. |
| v2 dependency isolation | Satisfied in the audited v2 scope | The new grade contract is Qt-free; F56 does not change the audited `src/next` dependency-isolation finding. |

Fresh independent MSVC 19.51/Ninja/Qt 6.12 verification validated 909
handwritten source owners, built `ClassMngrNextDomainContractTests`,
`ClassMngrRosterEditorWidgetImportTests`,
`ClassMngrSpeakingEvaluationServiceTests`, and
`ClassMngrSpeakingEvalReportWidgetTests`, and passed exact CTest 4/4. No full
suite was run. Gate 1 and Gate 2 remain Partial; Phase 2 remains In Progress
with its exit gate Open. Sub Prep remains capped at the current and following
calendar years at most.

### Cumulative exit-gate status after F58 - 2026-09-26 (commit `9b9183818fc2163d625a8ffb088a492a4aa631a9`)

This audit carries forward the verified F48-F57 findings and adds F58; no
checks were rerun for this documentation update.

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior | Partial | F48 adds typed schedule entries; F54-F57 add shared event, course-grade, speaking-grade, and evaluation-period rules; F58 adds typed teacher/class matching identities and an optional class suggestion. Focused contracts exercise these rules, but broader Domain/Application behavior remains incomplete. |
| Baseline parity | Partial | Fixture-backed evidence includes typed Schedule Import persistence/no-write checks, roster-score persistence, and F58's checked-in `schedule_review.xlsx` preview/apply path against a seeded database. F58 preserves matching order, ranks, confidence, explanations, fallback and legacy edge behavior; broader baseline parity remains incomplete. |
| Workspace boundary | Satisfied | F58 does not change the formal `WorkspaceGateway::createWorkspace`/`WorkspaceCoordinator` criterion or its focused app-less coverage. FileController integration caveats remain non-gating. |
| v2 dependency isolation | Satisfied in the audited v2 scope | F58's matching contract uses typed Domain identifiers and an optional suggestion; the legacy repository adapter owns conversion to integer preview IDs. The audited `src/next` isolation finding remains unchanged. |

Fresh independent x64 Ninja/MSVC 19.51/Qt 6.12 verification validated 912
handwritten source owners, built the matching projection and
`ClassMngrScheduleImportTests`, and passed exact CTest 2/2, including
`previewsAndAppliesCheckedInWorkbookAgainstSeededDatabase` with the checked-in
fixture. Executor focused CTest passed 1/1. No full suite was run. At F58,
there was no direct production assertion for the adapter's no-suggestion `-1`
sentinel; F63 closes this specific gap. The data model retained that default
and app-less contract tests asserted an absent optional suggestion. Gate 1 and Gate 2 remain Partial; Phase 2 remains
In Progress and its exit gate Open. Sub Prep remains capped at the current and
following calendar years at most.

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
class/campus references, bounded title/date/time/location/notes/`eventType`/
`timeStatus` text, an optional bounded `repeatSeriesId`, nonnegative
ordering, and an explicit all-day policy: all-day events omit times, while
timed events may omit both unknown times but not a partial range. Dates and
times remain opaque adapter-neutral text.

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

#### Progress update - 2026-09-20 (document-content session runtime integration slice)

`DocumentContentSession` is integrated into the `PdfViewerPage` lifecycle for
descriptors carrying a content reference. `NavigationController` propagates
the projected reference. Request and `beginLoading` precede `QPdfDocument`
loading; Qt `Ready`/`Error` map to session state; and release follows
`QPdfDocument::close()` on replacement, navigation, and destruction. Direct
no-reference descriptors remain compatible with direct loading.

Tests cover Ready, failure, release, replacement, and new-generation paths.
Ownership/dependency configure passed at 703 sources; focused
PageManager/content/catalog checks passed 6/6; the exact nine-target
production regression passed 9/9; resource checks passed for 6 RCC packs, 7
runtime IDs, and 7 references; and `git diff --check` passed. An initial
MSBuild FileTracker `E_ACCESSDENIED` required an elevated rerun; the elevated
build passed. Remaining work includes resource/platform adapter completion,
Sidebar/MainWindow ownership, and other legacy service migration. Phase 2
remains in progress.

#### Progress update - 2026-09-20 (bounded resource/platform document resolver slice)

Against baseline commit `e031317c`, the bounded resolver slice is complete.
`ClassMngrNext::Platform` now provides `DocumentContentResourcePort`, which
accepts `ResourcePackManager&`, validates `resource://documents/` references
including malformed, empty, and traversal rejection, acquires one
documents-pack lease, resolves primary and optional export paths, and returns
a move-only lease/path value. `NavigationController` uses this port instead of
directly acquiring or parsing `ResourcePaths::Documents`. `PdfViewerPage`
retains ownership of closing the PDF before releasing the lease, and
descriptors without a reference retain direct-loading compatibility. The
application layer remains Qt-free.

Configure/source-ownership/dependency checks passed at 705 sources; focused
resolver/navigation CTest passed 2/2; the exact nine-target CTest passed 9/9;
resource validation passed for 6 RCC packs, 7 runtime IDs, and 7 references;
and `git diff --check` passed with LF-to-CRLF warnings only. An initial MSBuild
FileTracker `E_ACCESSDENIED` required an elevated retry; the focused build/link
passed. Invalid UTF-8 and live UI integration lack direct coverage;
close-before-release is source-order verified. Sidebar/MainWindow catalog
ownership and other feature migrations remain open. Phase 2 remains in
progress.

#### Progress update - 2026-09-20 (document-folder hierarchy metadata prerequisite slice)

After baseline commit `fd695fd`, `DocumentFolderMetadata` carries bounded
`parentPath` metadata, empty for roots, and projection validation handles it.
`ApplicationServicesDocumentCatalogPort` copies legacy
`DocumentFolderDefinition::parentPath`. This preserves nested hierarchy for
the upcoming `Sidebar`/`MainWindow` projection cutover while keeping the
application layer Qt-free and aggregate initialization compatible.

Configure/ownership/dependency checks passed at 705 handwritten files;
focused projection/adapter CTest passed 2/2; Qt-free application and
standalone syntax checks passed; and `git diff --check` passed. The embedded
fixture contains root folders only, so nested adapter transfer lacks runtime
coverage; nested projection behavior is covered. Phase 2 remains in progress;
at this prerequisite handoff, UI cutover was still pending. The subsequent
typed `Sidebar`/`MainWindow` cutover is recorded below; other feature migration
remains open.

#### Progress update - 2026-09-20 (Sidebar/MainWindow typed catalog cutover)

After baseline commit `662e5f2`, the typed document-catalog projection is now
cut over at the Sidebar/MainWindow boundary. `Sidebar` owns a copied or
move-assigned `Application::DocumentCatalogProjection`; it has no legacy
`DocumentCatalog` pointer, include, or dependency. Its tree maps typed
`parentPath`, folder IDs, keys, and display names while preserving nested
hierarchy, order, localized labels, and empty-projection behavior.

`MainWindow::initializeSidebar` and `MainWindow::retranslateUi` request
locale-specific projections through
`Platform::ApplicationServicesDocumentCatalogPort` and pass them to Sidebar
by value. Projection failure supplies an empty projection. Configure,
ownership, and dependency checks covered 705 handwritten sources; Sidebar CTest
passed 1/1; adjacent catalog/projection/port tests passed 4/4; the exact
nine-target regression passed 9/9; and resource validation covered 6 RCC
packs, 7 runtime IDs, and 7 references. Application Qt-free and projection
standalone syntax checks passed. An elevated MSBuild retry was required after
`E_ACCESSDENIED`; the retry passed, and `git diff --check` passed.

There is no live MainWindow projection-failure/retranslation integration test;
static and production-compilation coverage is present, so this remains a
non-blocking gap. Phase 2 remains open for the remaining feature-service
migrations and is not complete.

#### Progress update - 2026-09-20 (theme preference bridge slice)

After baseline commit `9f0d86d`, `ClassMngrNext::Platform::ThemePreferencePort`
explicitly maps typed `Application::ThemePreference`
(`SystemDefault`, `Light`, or `Dark`) to the legacy `ThemeService`.
`ThemeController` owns typed `UserPreferencesState`, synchronizes the
persisted `ActionRegistry` theme without reapplying it during action
connection, and applies valid changes through the port. Invalid input and
state updates remain atomic; valid changes preserve persistence, icon refresh,
and live palette behavior. `MainWindow` now passes an explicit `ThemeService`
reference. `schedule_output_controller.cpp` still read the legacy theme at
that baseline; the follow-on direct-theme handoff is recorded below.

Configure/ownership/dependency checks passed at 706 sources. Focused
`StartupVisualSettings` passed 1/1; the next preferences/launch targets passed
2/2; the exact nine-target CTest passed 9/9; and resource validation covered
6 RCC packs, 7 runtime IDs, and 7 references. Qt-free application checks
passed, and `git diff --check` passed with CRLF warnings. The Ninja/MSVC
fallback build passed after the environment/FileTracker issue. No dedicated
icon-pixel assertion exists; this is a non-blocking gap. Phase 2 remains in
progress.

#### Progress update - 2026-09-20 (language preference bridge slice)

After baseline commit `3ad3ef1`, `ClassMngrNext::Platform::LanguagePreferencePort`
explicitly maps typed `Application::LanguagePreference`
(`SystemDefault`, `English`, or `Korean`) to the legacy `LanguageService`.
`LanguageController` owns typed `UserPreferencesState`, synchronizes the
persisted `ActionRegistry` language without reapplying it during action
connection, and applies valid changes through the port while preserving font
refresh, retranslation, and persistence behavior. `MainWindow` now passes an
explicit `LanguageService` reference. Generic settings persistence and other
feature-service migrations remain open.

Configure/ownership/dependency checks passed at 708 sources; focused
language/controller CTest passed 3/3; the exact nine-target CTest passed 9/9;
`LanguageService`/startup visual tests passed; and resource validation covered
6 RCC packs, 7 runtime IDs, and 7 references. Qt-free/raw-pointer checks and
`git diff --check` passed, and an elevated FileTracker retry passed. No live
`MainWindow::retranslateUi` assertion exists, failure rollback is not
deterministically exercised, and the nullable legacy `MainWindow`
`LanguageService` pointer has no null-construction coverage. Phase 2 remains
in progress.

#### Progress update - 2026-09-20 (schedule output explicit-theme slice)

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

#### Progress update - 2026-09-20 (calendar read-projection adapter slice)

Against baseline commit `08b86215`,
`src/next/platform/application_services_calendar_event_port.h` maps
`CalendarService::eventsInRange` into an owned, typed
`Application::CalendarEventProjection`. It copies bounded metadata and typed
IDs, and validates ordered valid ranges, unavailable service, technical
failures, invalid IDs/metadata, partial timed ranges, and projection capacity.
All-day and unknown-time policy remains explicit. No legacy pointers escape,
and the Application layer remains Qt-free. The malformed partial-time fixture
is inserted directly with `QSqlQuery` at the persistence boundary because
`CalendarService::saveEvent` correctly rejects malformed input.

The production header is registered in `cmake/next.cmake` and the focused test
is registered in `cmake/tests/next.cmake`. Focused adapter CTest passed 1/1;
existing `ClassMngrCalendarEventCacheTests` passed 1/1; the workspace control
test passed 1/1; and the exact existing nine-target regression passed 9/9 in
58.92s. Configure/ownership/dependency checks passed at 710 sources;
resource validation passed for 6 RCC packs, 7 runtime IDs, and 7 references;
strict UTF-8/encoding review passed; and `git diff --check` passed with only
LF-to-CRLF warnings. The focused build passed after an environmental
FileTracker `E_ACCESSDENIED` retry.

The calendar read-projection boundary is complete. The narrow typed
cache/model boundary is recorded below; broader typed calendar UI/page
migration remains future. The later worker-boundary handoff separates
database-query and worker ownership while leaving the legacy compatibility
conversions available. The metadata enrichment and its verification are
recorded below.
Generic settings and other feature migrations remain open. Phase 2 remains in
progress and is not complete.

#### Progress update - 2026-09-20 (calendar-event projection enrichment slice)

Against baseline commit `695d1065`, `CalendarEventSummary` now owns bounded
`eventType` and `timeStatus` strings plus an optional bounded `repeatSeriesId`.
The projection remains Qt-free, copyable, bounded, typed-ID based, ordered,
and pointer-free; existing all-day/unknown-time behavior, order, ID,
capacity, and legacy mapped-field byte semantics remain intact.

`ApplicationServicesCalendarEventPort` maps and validates the new fields,
trims only `repeatSeriesId` for normalization, preserves surrounding
whitespace for existing title/date/time and other mapped fields, and returns
structured failures for blank, over-bounds, or malformed values. Application
and adapter tests cover bounds, validation, copy/equality/lookups, legacy
value/repeat-series preservation, malformed persisted input, and title
whitespace compatibility. CMake registrations remain unchanged; the narrow
typed cache/model boundary is recorded below, while broader typed calendar
UI/page migration remains future.

The elevated VS Debug build passed after an environmental FileTracker
`UnauthorizedAccessException` retry. Focused application, adapter,
calendar-cache, and workspace-control tests passed 1/1; the exact nine-target
regression passed 9/9; configure/ownership/dependency checks passed with 710
handwritten sources and one explicit owner; resource validation passed 6 RCC
packs, 7 runtime IDs, and 7 references; `git diff --check` passed with
LF/CRLF warnings only; and the static Qt-free/pointer review passed.

Phase 2 remains open. The typed projection now feeds the completed narrow
cache/model boundary, and the later worker-boundary handoff separates calendar
database-query/worker ownership while legacy compatibility conversions remain
for unchanged callers. Broader typed calendar UI/page migration, generic
settings, and other migrations remain open.

#### Progress update - 2026-09-20 (typed calendar query-port slice)

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

#### Progress update - 2026-09-20 (typed calendar cache/model cutover)

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

#### Progress update - 2026-09-20 (typed upcoming-events read cutover)

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

#### Progress update - 2026-09-20 (final next-ten-events read cutover)

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

#### Progress update - 2026-09-20 (typed calendar activation-read boundary)

Against baseline commit `a7498732`,
`src/next/platform/application_services_calendar_event_port.h` now exposes
typed `projectionById(int)`. It preserves ID, title, event type, status,
repeat-series, all-day, unknown-time, date, and time fields, and returns
structured unavailable, missing, invalid, partial-time, overflow, and
malformed-repeat failures. `calendar_page_events.cpp` reads activation data
through the adapter and converts the typed result to legacy `CalendarEvent`
only at the existing UI boundary before opening the unchanged dialog.

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

#### Progress update - 2026-09-20 (typed single-event delete boundary)

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

The non-repeat single-event delete seam is closed; the repeat-series
suffix-delete boundary is recorded below. Save, repeat-series edit/save,
dialog ownership, generic settings, and other migrations remain future work;
Phase 2 remains open.

#### Progress update - 2026-09-20 (typed repeat-series suffix-delete boundary)

Against baseline commit `d4c186be`, added
`src/next/application/calendar_event_series_delete_port.h` with a Qt-free
bounded request carrying `repeatSeriesId` and an ISO start date plus
`Result<void>`, and
`src/next/platform/application_services_calendar_event_series_delete_port.h`
over `CalendarService::deleteRepeatSeriesFromDate`. The platform header is
registered in `cmake/next.cmake`; the existing platform-port test target is
reused.

Only the `thisAndFollowing` branch in `calendar_page_events.cpp` changed.
Exact series/date semantics remain intact; successful deletion invalidates and
refreshes, while failures preserve the existing state and warnings. Save,
single-event delete, repeat-edit/save, dialog, schedule, and all other branches
remain unchanged. Tests cover valid suffix deletion, blank or overlong IDs,
invalid ISO dates, unavailable service, and injected legacy failure; the valid
fixture forwards the exact series ID and `2026-12-08` and preserves the
pre-cutoff event.

The elevated Debug build passed; focused port/calendar/page tests passed
11/11; the exact nine-target regression passed 9/9 in 59.69s (57.05s startup);
configure/ownership/dependency passed with 718 sources and 9 production
targets (`ClassMngrNext -> Qt6::Core`); resource checks passed 6 RCC packs,
7 IDs, and 7 references; diff, cached-diff, trailing-whitespace, and static
checks passed. Verification reported exactly five scoped implementation files
dirty. Existing Vulkan, Qt-zlib, and MSVC notices are non-blocking.

The typed non-repeat delete and typed repeat-series suffix-delete seams are
closed. Save, repeat-series edit/save, dialog ownership, generic settings, and
other migrations remain future work; Phase 2 remains open.

#### Progress update - 2026-09-20 (typed non-repeat calendar-event save boundary)

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

The typed non-repeat save seam is closed. Repeat-series edit/save, dialog
ownership, generic settings, and broader page, document, and feature
migrations remain future work; Phase 2 remains open.

#### Progress update - 2026-09-20 (typed this-and-following repeat-series edit/save boundary)

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

#### Progress update - 2026-09-20 (typed this-event-only repeat-occurrence save boundary)

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

#### Progress update - 2026-09-20 (typed new-repeat series creation/batch save boundary)

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

#### Progress update - 2026-09-20 (typed calendar-dialog edit-draft boundary)

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

#### Progress update - 2026-09-20 (typed calendar-dialog constructor/input ownership boundary)

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

#### Progress update - 2026-09-20 (typed read-only schedule_use_24h settings bridge)

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

#### Progress update - 2026-09-20 (typed five-key schedule-preferences persistence boundary)

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

#### Progress update - 2026-09-20 (typed excelImportTimeoutSeconds boundary)

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

#### Progress update - 2026-09-21 (typed sidebar display preferences boundary)

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

#### Progress update - 2026-09-21 (typed PowerPoint data-access notice boundary)

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

#### Progress update - 2026-09-21 (typed skipped-update-version persistence boundary)

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

#### Progress update - 2026-09-21 (typed recent-workspace history boundary)

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

#### Progress update - 2026-09-21 (typed evaluation-default-policy boundary)

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

#### Progress update - 2026-09-21 (typed automatic-update preference boundary)

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

#### Progress update - 2026-09-21 (typed middle-school analytics preference boundary)

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

#### Progress update - 2026-09-21 (typed class day-filter reset-policy boundary)

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

#### Progress update - 2026-09-21 (typed class-selection reset-policy boundary)

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

#### Progress update - 2026-09-21 (typed class-navigation visibility-scope boundary)

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

#### Progress update - 2026-09-21 (typed last-selected-campus persistence boundary)

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

#### Progress update - 2026-09-21 (typed last-database-directory persistence boundary)

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

#### Progress update - 2026-09-21 (typed AI custom-website persistence boundary)

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

#### Progress update - 2026-09-21 (typed AI-comment voice read bridge)

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

#### Progress update - 2026-09-21 (typed AI-comment provider read bridge)

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

#### Progress update - 2026-09-21 (typed font-size startup read bridge)

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

#### Progress update - 2026-09-21 (typed theme startup read bridge)

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

#### Progress update - 2026-09-21 (typed DialogShell geometry persistence)

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

#### Progress update - 2026-09-21 (typed language-preference persistence/migration bridge)

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

#### Progress update - 2026-09-21 (typed upcoming-birthday dismissal write port)

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

#### Progress update - 2026-09-21 (typed document-viewer-background read bridge)

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

#### Progress update - 2026-09-21 (typed document-page-spacing read bridge)

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

#### Progress update - 2026-09-21 (typed SaveMode preference read bridge)

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

#### Progress update - 2026-09-21 (ActionRegistry typed AI-comment-voice read cutover)

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

#### Progress update - 2026-09-21 (ActionRegistry typed AI-comment-provider read cutover)

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

#### Progress update - 2026-09-21 (ActionRegistry typed font-size read cutover)

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

#### Progress update - 2026-09-21 (ActionRegistry typed theme read cutover)

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

#### Progress update - 2026-09-21 (ActionRegistry typed language read cutover)

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

#### Progress update - 2026-09-21 (typed ScheduleWidget display-mode persistence cutover)

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

#### Progress update - 2026-09-21 (ClassesPage typed schedule-display-mode caller cutover)

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

#### Progress update - 2026-09-21 (final SpeakingEvalPage schedule-display-mode seam)

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

#### Progress update - 2026-09-21 (typed two-key calendar event-display preferences boundary)

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

#### Progress update - 2026-09-21 (typed AcademicCalendarProvider first-day-of-week preferences boundary)

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

#### Progress update - 2026-09-21 (typed AcademicCalendarProvider schedule-persistence boundary)

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

#### Progress update - 2026-09-21 (typed CalendarPage event-type color persistence boundary)

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

#### Progress update - 2026-09-21 (typed CalendarPage current-campus read boundary)

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

#### Progress update - 2026-09-21 (typed custom-color palette persistence boundary)

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

#### Progress update - 2026-09-21 (typed personal display-name read bridge)

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

#### Progress update - 2026-09-21 (typed Sub Prep saved-content settings bundle)

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

#### Progress update - 2026-09-21 (typed Sub Prep current-campus read cutover)

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

#### Progress update - 2026-09-21 (typed Sub Prep personal-Zoom read/migration boundary)

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

#### Progress update - 2026-09-21 (typed personal-display-name writer extension)

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

#### Progress update - 2026-09-21 (typed personal signature-image read port)

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

#### Progress update - 2026-09-21 (typed InitialSetupWizard signature-image reads)

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

#### Progress update - 2026-09-21 (typed InitialSetupWizard display-name prefill)

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

#### Progress update - 2026-09-21 (typed current-campus writer boundary)

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

#### Progress update - 2026-09-21 (typed PersonalDetailsPage signature-image read)

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

#### Progress update - 2026-09-21 (typed PersonalDetailsPage current-campus read)

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

#### Progress update - 2026-09-21 (typed PersonalDetailsPage display-name read)

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

#### Progress update - 2026-09-21 (typed PersonalDetailsPage Zoom-read reuse)

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

#### Progress update - 2026-09-21 (typed personal-signature-preferences bundle)

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

#### Progress update - 2026-09-21 (typed PersonalDetailsPage aggregate atomic writer)

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

#### Progress update - 2026-09-21 (typed InitialSetupWizard aggregate-writer cutover)

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

#### Progress update - 2026-09-21 (typed InitialSetupWizard read composition)

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

#### Progress update - 2026-09-21 (typed language persistence cutover)

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

#### Progress update - 2026-09-21 (typed SaveMode persistence cutover)

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

#### Progress update - 2026-09-21 (typed AI-comment voice persistence slice)

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

#### Progress update - 2026-09-21 (typed AI-comment provider persistence slice)

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

#### Progress update - 2026-09-23 (typed document-viewer background persistence)

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
selection; malformed or unknown reads fall back to `Default`. The existing
MainWindow `onChanged` propagation and PDF viewer behavior remain intact; no
CMake changes were needed.

The focused Debug rebuild passed after retrying a Visual Studio FileTracker
access-denied failure with elevated access. A serial CTest run passed all 9
selected targets, including the background adapter, AI/ActionRegistry,
PageManager, StartupVisualSettings, and provider, voice, language, and SaveMode
regressions. CMake ownership validation passed with 837 handwritten sources;
Qt-free, call-site, source-path, and diff checks passed. Tests used an isolated
`CLASSMNGR_SETTINGS_ROOT`.

#### Progress update - 2026-09-23 (typed document-page-spacing persistence)

Against baseline commit `790082c4` (`Phase2 - Cut ActionRegistry viewer
background persistence over`), completed the typed document-page-spacing
persistence cutover. The Qt-free `DocumentPageSpacingPreferencesPort` now
exposes `write()`, and the SettingsManager adapter persists valid `None`,
`Small`, `Medium`, and `Large` values as `0`, `1`, `2`, and `3`. Invalid typed
values are rejected without modifying storage. Existing reads remain
compatible: missing or unknown values map to `Small`, while malformed text
continues through unchecked `QVariant::toInt()` and maps to `None`.

`ActionRegistry` installs typed persistence before initial state selection;
MainWindow/PageManager update propagation remains unchanged. The expected
implementation/test scope is:

- `src/next/application/document_page_spacing_preferences.h`
- `src/next/platform/settings_manager_document_page_spacing_preferences_port.h`
- `src/ui/shared/actions/action_registry.cpp`
- `tests/next_platform_settings_manager_document_page_spacing_preferences_port_tests.cpp`
- `tests/ai_comment_options_tests.cpp`

No CMake changes were made.

Independent review, build, and serial CTest passed for
`ClassMngrNextPlatformSettingsManagerDocumentPageSpacingPreferencesPortTests`,
`ClassMngrAiCommentOptionsTests`, `ClassMngrPageManagerTests`, and
`ClassMngrStartupVisualSettingsTests`; `git diff --check` was clean. Phase 2
remains open; this document-page-spacing persistence boundary is closed.

#### Progress update - 2026-09-23 (typed font-size persistence cutover)

Completed the typed FontSize writer cutover in the current Phase 2 working
tree. The Qt-free `FontSizePreferencesPort` now exposes `write()`, and the
SettingsManager adapter writes only canonical offsets `Small=-2`, `Normal=0`,
`Large=2`, and `ExtraLarge=4`; invalid typed values are ignored without
changing storage. Missing, unknown, and malformed reads continue to fall back
to `Normal`.

`ActionRegistry` installs typed `onPersist` before startup selection and no
longer performs the direct raw write. `FontSizeController` `onChanged`
behavior remains unchanged. The exact five-file scope is:

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
was clean. Phase 2 remains open.

#### Progress update - 2026-09-23 (Sub Prep schedule-summary query contract)

`src/next/application/sub_prep_schedule_summary_query.h` adds a Qt-free
Application query for typed visible class IDs, Application-owned weekdays,
and typed regular/intensive schedule mode. It uses an injected read port and
reuses `ClassSummaryProjection`; projection validation is all-or-nothing and
ordering is deterministic. Empty visible-class or selected-day scopes return
a successful empty projection without reading.

The app-less test is
`tests/next_application_sub_prep_schedule_summary_query_tests.cpp`, registered
in `cmake/tests/next.cmake`. Release configuration validated ownership of 701
handwritten sources, `ClassMngrNext` built, and
`ctest -R ClassMngrNextApplicationSubPrepScheduleSummaryQueryTests
--output-on-failure` passed 1/1. The query is not connected to the legacy page
and has no persistence adapter; batching, output/UI migration, feature parity,
and memory improvement remain unclaimed. Phase 2 remains In progress.

#### Progress update - 2026-09-23 (Sub Prep selected-details and selection-state contracts)

Commit `389d90a6a433ae6c5c7ce7263daba02f4a27a5ce` adds the Qt-free
`SubPrepClassDetailsQuery` for one selected class. Its injected read port
propagates structured failures, verifies the returned class ID, and permits a
missing teacher identity. `ClassMngrNextApplicationSubPrepClassDetailsQueryTests`
passed 1/1.

Commit `7959eb07` adds `SubPrepClassInformationState` for scope refresh,
selection, detail application, and clear. It retains only a selection within
the visible scope, clears details on each successful refresh or selection
change, and accepts details only when class and teacher identity match. The
focused `ClassMngrNextApplicationSubPrepClassInformationStateTests` passed 1/1. Both
app-less targets compiled and linked in the Windows x64 developer environment;
neither contract is connected to legacy UI or persistence. No batching,
package/PDF migration, or memory reduction is claimed. Package/roster/PDF and
Release memory gates remain later work. Phase 2 remains In progress.

#### Progress update - 2026-09-23 (Sub Prep operation-scoped print-source contract)

`SubPrepPrintSourceRequest`, `SubPrepPrintSourceReadPort`, and
`SubPrepPrintSourceQuery` in
[`sub_prep_print_source_query.h`](../../src/next/application/sub_prep_print_source_query.h)
define a Qt-free boundary for selected class IDs, weekdays, and regular or
intensive schedule mode. The query validates the full request before reading,
returns an empty source without I/O for an empty class or day scope, propagates
structured read errors, and validates the complete bounded result before
returning an owned source. Class rows refer to teacher IDs; teacher values are
stored once, and missing teachers or a subset of requested classes are
permitted.

[`ClassMngrNextApplicationSubPrepPrintSourceQueryTests`](../../tests/next_application_sub_prep_print_source_query_tests.cpp)
is an app-less test registered in [`next.cmake`](../../cmake/tests/next.cmake).
The focused target build passed and CTest passed 1/1. No Sub Prep adapter,
page/UI, or PDF wiring, SQL batching, release/memory improvement, roster/package
migration, or output parity was established. Phase 2 remains In Progress.

#### Progress update - 2026-09-23 (custom-color palette caller boundary)

Commit `83163b0a` completed the custom-color palette caller seam. [`ColorUtils`](../../src/core/utils/colorutils.h)
now accepts the existing [`CustomColorPalettePreferencesPort`](../../src/next/application/custom_color_palette_preferences.h)
and has no `SettingsService` or Platform dependency. All seven UI picker call
sites pass the existing Platform adapter. Picker behavior and the stored
palette format were preserved; no other UI-service boundary is covered by
this slice.

The offscreen QtTest [`ColorUtilsCustomColorPaletteTests`](../../tests/colorutils_custom_color_palette_tests.cpp)
covers load/save across all 16 `QColorDialog` custom-color slots, canonical
writes, and slot restoration. The
[`NextPlatformApplicationServicesCustomColorPalettePreferencesPortTests`](../../tests/next_platform_application_services_custom_color_palette_preferences_port_tests.cpp)
also covers null `SettingsService` defaults and no-op writes.
Both Windows x64 Ninja targets built and focused CTest passed 2/2; diff checks
passed. Generic settings persistence remains open. This is not the Phase 3
persistence rewrite or a Phase 7 feature migration; Phase 2 remains In
Progress.

#### Progress update - 2026-09-23 (calendar import planning contract)

Commit `d5a5cab9` adds the Qt-free
[`CalendarEventImportPlan`](../../src/next/application/calendar_event_import_plan.h)
and uses it from
[`CalendarEventImportService::handleFinished`](../../src/features/calendar/calendar_event_import_service.cpp).
After the legacy range read, the service supplies exact UTF-16 signature keys
for existing and candidate events. The planner returns accepted candidate
indices in input order and carries forward parser skips, preserving duplicate
behavior including exact comparison of malformed surrogate sequences. The
batch save, error, metric, and signal paths remain in the legacy service.

[`NextApplicationCalendarEventImportPlanTests`](../../tests/next_application_calendar_event_import_plan_tests.cpp)
and [`CalendarImportTests`](../../tests/calendar_import_tests.cpp) cover
planning order/counts, exact six-field identity behavior, and UTF-16 equality.
Both Windows x64 Ninja targets built and focused CTest passed 2/2. Range
retrieval and batch persistence remain legacy responsibilities; this contract
does not complete calendar import migration or Phase 2. The existing-event
signature-key read is completed in commit `d14155c1` below; candidate parsing
and batch save remain unchanged. The general event projection has a 4,096-row
cap and stricter metadata validation, so the import uses a dedicated key read
to preserve legacy range behavior.

#### Progress update - 2026-09-23 (calendar import existing-signature read cutover)

Commit `d14155c1` adds
[`ApplicationServicesCalendarEventPort::importSignatureKeysInRange`](../../src/next/platform/application_services_calendar_event_port.h)
and routes duplicate planning through it. The port reads the same requested
date range, preserves legacy result order, and returns UTF-16 keys matching
`CalendarImport::calendarEventImportSignature`. It bypasses the general
4,096-row event projection and its stricter metadata validation. Candidate
parsing and batch save remain in the legacy import service.

`ClassMngr` and the focused calendar-event port target built on Windows x64
Ninja; focused CTest passed 1/1 and `git diff --check` passed. Phase 2 remains
open. The next slice integrates the Sub Prep print-source contract with a
production read adapter; the contract still has no adapter, page, or PDF
wiring, and no output-parity or memory acceptance is claimed.

#### Progress update - 2026-09-23 (Sub Prep print-source Platform read adapter)

Commit `2daae4ef` adds
[`ApplicationServicesSubPrepPrintSourcePort`](../../src/next/platform/application_services_sub_prep_print_source_port.h)
and a scoped `ClassService::classInfosForScheduleScope` /
`ClassInfoRepository::loadClassInfosForScheduleScope` SQL read. The query
filters requested class IDs, selected weekdays, and the selected regular or
intensive schedule table before materializing class information. Positive
typed IDs are validated and rendered as decimal integer literals, keeping the
4,096-class scope within SQLite's bind limit. Per-class and aggregate meeting
limits read at most one sentinel row beyond each bound and report overflow.

The adapter returns owning values, preserves requested-class order, copies
each referenced teacher once, and omits classes without selected meetings or
with missing class information, an unassigned teacher, or a missing teacher.
Roster lookup failures retain the legacy zero-count fallback. The class-scope
read requires the active repository session and has no `DataService` fallback.

`ClassMngr`, the Platform adapter test target, and the app-less print-source
query target built on Windows x64 Ninja. Focused CTest passed
`ClassMngrNextPlatformApplicationServicesSubPrepPrintSourcePortTests` and
`ClassMngrNextApplicationSubPrepPrintSourceQueryTests`; `git diff --check`
passed. This closes the selected-scope source-read seam only. Page/PDF wiring,
output parity, teacher/roster batching, Release memory acceptance, and full
Sub Prep completion remain open; Phase 2 remains In Progress. The next bounded
slice adds the selected-class details Platform read adapter, initially
unconnected to the page.

#### Progress update - 2026-09-23 (Sub Prep selected-class details Platform read)

The selected-class Application details value now exposes separate bounded
fields for room, WiFi name, WiFi password, internet type, Zoom ID, Zoom
password, and projection type. The session-backed
`ApplicationServicesSubPrepClassDetailsPort` reads one class through
`ClassService` and the active repository session; it copies only class notes,
preferred teacher display name, those seven fields, and teacher notes. It does
not query either schedule table and has no `DataService` fallback. An existing
class without a `class_info` row returns blank details; absent classes return
`NotFound`. Unassigned, missing, or stale teacher references produce empty
teacher values.

The Windows x64 Debug build succeeded. Focused `NextApplicationClassSummary`,
`NextApplicationSubPrepClassDetailsQuery`, and
`NextPlatformApplicationServicesSubPrepPrintSourcePort` suites passed. This
establishes the bounded read seam only: no page wiring, full legacy-output
parity, memory improvement, or broader Sub Prep/Phase 2 acceptance is claimed.
Page/output integration and parity plus the large-workspace Release memory
evidence remain open. The next Phase 2-bounded Sub Prep continuation is the
scoped schedule-summary persistence read behind its existing Application
contract. Phase 2 remains In Progress.

#### Progress update - 2026-09-23 (Sub Prep scoped schedule-summary Platform read)

Commit `dd429838` adds
[`ApplicationServicesSubPrepScheduleSummaryPort`](../../src/next/platform/application_services_sub_prep_schedule_summary_port.h)
for the existing scoped schedule-summary query. It reads only the requested
class/day/mode scope through the active `ClassService` repository session and
returns bounded owning class and teacher summary values. Roster counts are
batched; a roster read failure preserves the zero-count fallback. Empty class
or day scopes return without reads, and the adapter has no `DataService`
fallback. The intensive-mode test succeeds with `class_times` dropped,
proving the mode-specific read does not require the regular schedule table.

The Windows Debug build of
`ClassMngrNextPlatformApplicationServicesSubPrepPrintSourcePortTests`
succeeded. Focused CTest passed
`ClassMngrNextApplicationClassSummaryTests`,
`ClassMngrNextApplicationSubPrepScheduleSummaryQueryTests`, and
`ClassMngrNextPlatformApplicationServicesSubPrepPrintSourcePortTests` 3/3 in
13.39 seconds. Configure validated 857 handwritten files, and
`git diff --check` passed. This closes the scoped schedule-summary read seam
only. Phase 2 remains In Progress; page/output wiring, behavior parity, and
96-class Release memory evidence remain open. Work Package D is now complete;
the following progress update records the model-backed list/navigation and
reusable selected-class details view.

#### Progress update - 2026-09-23 (Sub Prep model-backed navigation and reusable details view)

Work Package D wires the schedule-summary and selected-class details queries
to the live Sub Prep class-information view. A Qt list model filters and orders
the compact `ClassSummaryProjection` by grade and configured level; one
`QListView` replaces per-class pages, and one details card is reused as the
selection changes. `SubPrepClassInformationState` owns the selected typed
class ID and current bounded details. Refresh keeps the selected class only
while it remains in the schedule scope, and the schedule display-mode signal
refreshes the projection in place.

Windows x64 Debug Ninja built `ClassMngr`,
`ClassMngrSubPrepClassInformationListModelTests`,
`ClassMngrSubPrepPageTests`, and `ClassMngrStartupPerformanceTests`. Focused
CTest passed the list-model and page suites 2/2; configure validated 860
handwritten files and `git diff --check` passed. The startup-performance
target compiled, but this slice did not run the packaged 96-class Release
route. This closes the model-backed view integration only. Page-leave resource
release, output/package/PDF migration, parity, and Release memory acceptance
remain open; Phase 2 remains In Progress. Work Package E is next.

#### Progress update - 2026-09-23 (Sub Prep page-leave lifecycle release)

Work Package E uses the existing `BasePage::deactivate()` resource-release
hook. When PageManager leaves Sub Prep, the page clears the selected details,
summary projection, and selection state, then marks itself stale. On the next
activation, Sub Prep reloads the current schedule scope and one selected-class
detail instead of retaining the prior operation state.

Windows x64 Debug Ninja built `ClassMngr` and `ClassMngrSubPrepPageTests`;
focused CTest passed the page suite 1/1. The lifecycle test verifies the list
and details are cleared at deactivation and restored on activation, including
a new details read. This closes page-leave release only. Package/PDF output
migration, parity, and 96-class Release memory acceptance remain open; Phase 2
remains In Progress. Work Package F is next: connect the operation-scoped
print-source query to package generation.

#### Progress update - 2026-09-24 (Sub Prep information-sheet print-source integration)

Work Package F1 connects the Sub Prep page's output request to
`SubPrepPrintSourceQuery`. After dialog acceptance, the page sends the
selected class IDs, selected weekdays, and current regular/intensive mode to
the query. The bounded owning result is mapped into the existing
`SubPrepClassInformation::TeacherGroup` renderer model. The Application value
now carries English name, Korean name, preferred name, and preferred
romanization so the mapper preserves `Teacher::preferredDisplayName()` and
the existing teacher facts.

The previous output path loaded all classes and then looked up each class's
information, roster count, and teacher. That path is removed for the main
information sheet. The query source is released when the mapper returns. The
separate roster-PDF package stage still reads legacy class, teacher, and full
roster values; `SubPrepDocumentModel` still copies the renderer model. No full
package output parity or memory improvement is claimed.

Windows x64 Debug Ninja built `ClassMngr`, the page, mapper, Application
query, Platform adapter, PDF, and package targets. Focused CTest passed 6/6 for
the mapper, page, print-source query and Platform adapter, PDF renderer, and
package service. This closes only the main information-sheet read integration.
Work Package F continues with the remaining output-source and renderer-model
lifetime boundaries; parity, cancellation/error cleanup, the 96-class
packaged Release memory gate, and Phase 2 remain open.

#### Progress update - 2026-09-24 (Sub Prep information-sheet render model lifetime)

Work Package F2 removes the second rich `TeacherGroup` list created when
`SubPrepDocumentModel::build()` copied `SubPrepPrintService::Request` into its
renderer value. `Document::classInformation` now holds an explicit const
reference wrapper to the request's list. `saveSubPrepPdf()` keeps the source
request alive in a named local through the synchronous `SubPrepPdfRenderer`
call; no renderer-retained pointer is introduced. The page moves its print
request into the package request so this handoff does not clone the
`TeacherGroup` list.

The PDF test verifies that the render document refers to the exact request
list. Windows x64 Debug Ninja built `ClassMngr`, the PDF, package, and page
targets; focused CTest passed those suites 3/3. This removes one overlapping
rich model allocation only. The package request still owns those values after
the main sheet finishes, and roster PDF generation still reads full legacy
class, teacher, and roster values. Work Package F continues with stage release
and the roster-source contract; output parity, 96-class packaged Release
memory acceptance, and Phase 2 remain open.

#### Progress update - 2026-09-24 (Sub Prep package stage release)

Work Package F3 makes `SubPrepPackageService::generate()` take ownership of
its operation request by value. `SubPrepPage` moves its completed package
request into the call. `generateAt()` now writes the information-sheet PDF
before loading full class, teacher, and roster values; after that PDF succeeds,
it clears `request.subPrep` before beginning the roster-output stage. This
releases the schedule and rich information-sheet model instead of overlapping
them with the roster projection.

Windows x64 Debug Ninja built `ClassMngr`,
`ClassMngrSubPrepPackageServiceTests`, and `ClassMngrSubPrepPageTests`.
Focused CTest passed the page, PDF, and package suites 3/3. Existing package
tree, document order, and status behavior tests pass. The roster stage still
uses legacy class, teacher, and full roster reads; the next Work Package F
slice adds an operation-scoped roster source. Output parity, the 96-class
packaged Release memory gate, and Phase 2 remain open.

#### Progress update - 2026-09-24 (Sub Prep roster-output Application contract)

Work Package F4 adds the Qt-free
`SubPrepRosterOutputSourceRequest`, read port, bounded source value, and query.
The request carries selected class IDs, weekdays, schedule mode, and requested
extra roster columns. The source holds only renderer-facing class/teacher
names, selected schedule facts, room/network/Zoom values, requested roster
columns, and owning cell strings. Query validation enforces unique in-scope
IDs and teacher links, selected weekdays, row shape, per-class and aggregate
row/cell/text limits, and no I/O for empty class/day scopes.

The app-less query test covers request and source rejection, no-read behavior,
read failure propagation, stable port order, maximum per-class dimensions, and
aggregate overflow. CMake source ownership validated 865 handwritten files;
Windows x64 Debug built `ClassMngr` and the query target, and focused CTest
passed 1/1. Query-side caps do not replace adapter-side limits: the next slice
must enforce bounds while reading the active database session, before
materializing roster rows. Package integration, PDF/package parity, the
96-class packaged Release memory gate, and Phase 2 remain open.

#### Progress update - 2026-09-24 (Sub Prep bounded roster repository read)

Work Package F5 adds `RosterRepository::loadRosterForOutput` and forwards it
through `DataService` and `RosterService`. The query resolves only requested
columns, checks `MAX(row_index)` on those physical columns, and validates the
dense row/cell budget before allocating the projected matrix. SQL reads are
forward-only; selected values use a bounded substring and the original byte
length is checked before conversion. Unrequested column values are never read.

`ClassMngr` and `ClassMngrDataServiceLifecycleTests` built on Windows x64
Debug Ninja; the lifecycle suite passed 1/1. It covers selected-column order,
row/cell/text budgets, an oversized stored cell, and a sparse out-of-range row
index. The repository method enforces aggregate budgets supplied by its
caller; the next Platform adapter must decrement those budgets across the
operation while mapping bounded values. Package integration, output parity,
the 96-class packaged Release memory gate, and Phase 2 remain open.

#### Progress update - 2026-09-24 (Sub Prep roster-output Platform read)

Work Package F6 adds
[`ApplicationServicesSubPrepRosterOutputSourcePort`](../../src/next/platform/application_services_sub_prep_roster_output_source_port.h).
It maps canonical typed IDs to legacy IDs, reads the requested class/day/mode
scope through the active services, retains unassigned classes for roster
output, shares teacher facts, and projects only baseline and requested roster
columns. Remaining operation row/cell/text budgets are passed to the bounded
roster service before values become the Application-owned source. The scoped
schedule read now accepts an explicit include-unassigned option; existing
callers retain their prior default behavior.

The new database-backed Platform test covers request ordering, day and mode
selection, unassigned teachers, roster column projection, sparse-row overflow,
and oversized teacher output. CMake validated 867 handwritten source owners.
Windows x64 Debug built `ClassMngr`; focused CTest passed the new Platform
suite, the existing Sub Prep print-source Platform suite, and the roster-output
Application query suite (3/3). `git diff --check` passed. This closes the
bounded roster-source adapter only. Package-service wiring, renderer mapping,
output parity/cleanup, 96-class packaged Release memory evidence, and Phase 2
acceptance remain open. Work Package F7 wires the source into package
generation.

#### Progress update - 2026-09-24 (Sub Prep package roster-source integration)

Work Package F7 replaces the package service's direct legacy class, teacher,
and roster reads with `SubPrepRosterOutputSourceQuery`. The request carries
typed selected class IDs, dates, schedule mode, and extra columns. The package
service maps bounded Application values into the existing renderer model,
checks canonical integer IDs and UTF-8 round trips, and preserves the current
class ordering, folder naming, package tree, and per-class/daily/by-day output
selection. `SubPrepPage` owns the session-backed Platform adapter for the
synchronous generation call; the package service has no `ApplicationServices`
or `DataService` dependency.

Windows x64 Debug built `ClassMngr`, the package service tests, and the page
tests. Focused CTest passed 5/5 for package, page, PDF, Application query, and
Platform source suites. Tests cover scoped class/day/mode/column forwarding,
daily and per-class output, and source/mapping failure cleanup before a final
package is committed. Full PDF/package parity and the 96-class packaged
Release memory gate have not been established. Work Package F8 compares output
with retained references and closes cancellation/error/cleanup parity before
the Release acceptance run; the broader Phase 2 exit gate remains open.

#### Progress update - 2026-09-24 (Sub Prep output-reference parity)

Work Package F8 extends the 96-class package probe to compare the generated
`Sub Prep.pdf` and Daily roster PDF with the committed references under
`docs/qt-rewrite/visual-baseline/release/sub-prep-output/reference/`. It checks
page counts, every page's point dimensions, and extracted text. On Windows it
also renders every page at 150 DPI and compares the images exactly. The
information-sheet and Daily roster PDFs match the references at 19 and 16
pages. PDF bytes are regenerated and are not used as the parity oracle.

Cancellation and failure cleanup coverage confirms that print-only temporary
PDFs are removed and no package directory is committed on cancellation, roster
read failure, or invalid text mapping. Windows x64 Debug built the package
suite; focused CTest passed 5/5 for package, page, PDF, Application query, and
Platform source suites. Next is the packaged 96-class Windows x64 Release
memory gate; UI visual-state and the broader Phase 2 exit gates remain open.

#### Progress update - 2026-09-24 (Sub Prep packaged Release measurement)

Work Package F9 extends the Sub Prep output-boundary workflow test to capture
working set at both one and five seconds after completion. The updated Windows
x64 Debug startup test target built, and the route test passed while driving
the packaged Release application. Route-scoped validation passed for
`output-sub-prep`, with two PDFs, 17 pages, normal exit, and no timeout. This
single-route run leaves the aggregate Phase 0 exit gate incomplete, as
expected.

The report measured a 315,740,160-byte peak working set and 351,821,824-byte
peak private usage, compared with retained legacy peaks of 498,176,000 and
480,948,224 bytes. Settled working set was 306,466,816 bytes at one second and
306,470,912 bytes at five seconds. The route is below the temporary 512 MiB
diagnostic ceiling but remains above the final 250 MiB target. Package output
parity and this route result do not close the broader Phase 2 exit gate; the
typed calendar UI/page migration, generic settings persistence, remaining
feature-service migrations, and broader document-service migration remain.


#### Progress update - 2026-09-24 (typed calendar import batch-save boundary)

The calendar import service now maps only the duplicate planner's accepted
candidate indices into `CalendarEventImportSaveRequest`. The Qt-free request
validates every typed create value, rejects update IDs, supports the empty
batch used when all candidates are duplicates, and bounds one batch at 4,096
events. The Platform adapter converts the ordered batch and calls
`CalendarService::saveEvents()` once, keeping the repository transaction over
the whole import; it returns typed event IDs in the same order. Import metrics,
skipped counts, failure reporting, and completion signals continue to use the
same operation result.

Windows x64 Debug built `ClassMngr` and the Application, Platform, and parser
test targets. CMake validated 869 explicit source owners. Focused CTest passed
3/3 for `ClassMngrNextApplicationCalendarEventTests`,
`ClassMngrNextPlatformApplicationServicesCalendarEventPortTests`, and
`ClassMngrCalendarImportTests`. Database tests cover ordered IDs and event
fields, duplicate-only no-op, invalid-batch preflight, unavailable service,
and rollback after a later insert fails. Workbook parsing and campus-directory
lookup remain legacy responsibilities; the calendar import migration is not
complete, and broader Phase 2 work remains open.


#### Progress update - 2026-09-24 (typed calendar availability boundary)

The calendar feature no longer calls `ApplicationServices::calendarService()`
directly. Import-start and dialog-opening availability guards now use
`ApplicationServicesCalendarEventPort::isAvailable()`, which contains the
legacy service check and treats exceptions as unavailable. This preserves the
existing early-return behavior while keeping raw service ownership in the
Platform boundary. Windows x64 Debug built `ClassMngr`, the calendar import
test target, and the Platform calendar event suite; focused CTest passed 2/2.
The broader calendar UI/value migration and other feature-service migrations
remain open.


#### Progress update - 2026-09-24 (typed calendar reset mutation)

The calendar preferences panel now routes its confirmed reset through
`CalendarEventDeleteAllPort` and
`ApplicationServicesCalendarEventDeleteAllPort`. The adapter exposes service
availability separately so the UI preserves the existing behavior of checking
availability before displaying the destructive confirmation. The panel keeps
the same prompt, warning, success status, and refresh notification and no
longer stores `CalendarService`.

Windows x64 Debug built `ClassMngr` and the Application and Platform calendar
event test targets. CMake validated 871 explicit source owners. Focused CTest
passed 2/2; database cases cover successful reset, unavailable service, and a
trigger-injected delete failure that leaves its event intact. The import
signature read, generic settings persistence, remaining feature-service
migrations, and broader document migration remain open.


#### Progress update - 2026-09-24 (typed calendar edit-draft flow)

Calendar day activation now creates a `CalendarEventEditDraft`, and event
activation maps the already typed `CalendarEventSummary` directly to a draft.
The page passes that value into the dialog and bases repeat, delete, and save
requests on it. The old projection-to-`CalendarEvent`-to-draft conversion is
removed, as are the unused legacy `QList<CalendarEvent>` upcoming filter and
its visibility overload.

Windows x64 Debug built `ClassMngr`, `ClassMngrDialogShellTests`, and the
Platform calendar event suite. Focused CTest passed 2/2. Broader calendar
visual-state and page integration coverage remains part of the Phase 2 exit
work.


#### Progress update - 2026-09-24 (calendar display-preference boundary)

`CalendarPreferencesPanel` now passes its `ApplicationServices*` to
`ApplicationServicesCalendarEventDisplayPreferencesPort` rather than retaining
a `SettingsService*` solely to construct the adapter. The adapter continues to
own the same setting keys, default-false reads, unavailable-save no-op, and
atomic two-key save. Windows x64 Debug built `ClassMngr` and the Platform
display-preferences suite; focused CTest passed 1/1, including pointer-based
round-trip and null/unavailable service behavior.


#### Progress update - 2026-09-24 (academic calendar preference-port injection)

AcademicCalendarProvider now owns the Application schedule and first-day
preference ports. CalendarPage and evaluation-default selection construct
the Platform adapters and inject those ports, removing SettingsService from
the provider. The schedule, first-day, and display-preference Platform
adapters no longer expose SettingsService-pointer constructors.

Windows x64 Debug built ClassMngr, ClassMngrAcademicCalendarTests, and the
three Platform preference suites. Focused CTest passed 4/4; CMake validated
871 source owners, a provider source search found no SettingsService
references, and git diff --check passed. The remaining calendar
upcoming-events preference callers and broader Phase 2 migrations remain open.


#### Progress update - 2026-09-24 (calendar event-type color preference boundary)

Calendar event-type color reads and writes now construct
`ApplicationServicesCalendarEventTypeColorPreferencesPort` from
`ApplicationServices*`. The feature no longer gates these operations with or
passes a raw `SettingsService*`; unavailable reads retain the default-color
fallback and unavailable saves remain no-ops.

Windows x64 Debug built `ClassMngr` and the Platform color-preference suite;
focused CTest passed 1/1, including ApplicationServices-pointer round-trip
and null-service fallback. `git diff --check` passed. Other upcoming-events
preference access and broader Phase 2 migrations remain open.

#### Progress update - 2026-09-24 (current-campus availability and options boundary)

`Application::CurrentCampusPreferencesPort` exposes Qt-free `isAvailable()`;
`Platform::ApplicationServicesCurrentCampusPreferencesPort` maps availability
to the legacy service. CalendarPage upcoming-event options use the port before
preference reads and campus projection, preserving the old availability gate.
The feature file no longer retains or queries `SettingsService`.

Windows x64 Debug built `ClassMngr` and the CurrentCampus preference test
target. Focused CTest passed 1/1 for available, unavailable, null
`ApplicationServices`, and null `SettingsService`; `git diff --check` passed.
No dedicated CalendarPage test exists; the Tester judged the port tests plus
guard/source comparison reasonable for this refactor. Other calendar
input/lookup work and broader Phase 2 migrations remain open.


#### Progress update - 2026-09-24 (calendar import signature-query contract)

The exact existing-signature read now crosses the Qt-free Application
`CalendarEventImportSignatureQueryPort`, with a dedicated Platform
`ApplicationServicesCalendarEventImportSignatureQueryPort`. The import
workflow uses the port for availability and date-range signatures; the former
`ApplicationServicesCalendarEventPort::importSignatureKeysInRange` method has
been removed. Platform's enforced dependencies remain Application and
Qt6::Core.

Windows x64 Debug built `ClassMngr` and both query-port targets. Independent
focused CTest passed 2/2; CMake validated 874 source owners, and
`git diff --check` passed. Coverage preserves the six-field QString/UTF-16
signature, ordering, invalid/unavailable/read failures, and 4,097-row results
beyond the general projection cap. Workbook parsing and campus-directory
lookup remain legacy; broader calendar import, generic settings, other
feature-service migrations, and Phase 2 remain open.

#### Progress update - 2026-09-24 (personal display-name caller migration)

Schedule output, schedule import, and the Sub Prep print dialog now construct
`ApplicationServicesPersonalDisplayNamePreferencesPort` from
`ApplicationServices&`, removing their `SettingsService*` adapter callers.
Unavailable reads remain empty and writes remain silent no-ops; null services
retain empty/default names. Schedule output reads only after dialog acceptance
and preserves stored whitespace, while import and Sub Prep continue to trim.

The ScheduleWidget tests cover a preference value changed by the accepted
signal, including exact whitespace in the print request, unavailable
preferences, and null services. Independent verification built
`ClassMngrScheduleWidgetTests`, `ClassMngrScheduleImportDialogTests`, and
`ClassMngrSubPrepPageTests`; CTest passed 3/3 and ScheduleWidget passed 19/19.
`git diff --check HEAD` passed with line-ending conversion notices. My
Information and Initial Setup remain display-name adapter callers; generic
settings persistence, other feature-service migrations, and Phase 2 remain
open.

#### Progress update - 2026-09-24 (F20 My Information and Initial Setup migration)

My Information and Initial Setup now construct
`ApplicationServicesPersonalDisplayNamePreferencesPort` from
`ApplicationServices&`. The adapter's `SettingsService*` constructor and its
constructor-only test were removed. Existing availability guards remain;
My Information preserves stored UTF-8/whitespace, Initial Setup fills only a
blank name field, and both retain aggregate personal-details saves.

An independent fresh Windows x64 Ninja/MSVC build completed 322 steps and
compiled the changed production and test translation units. Initial Setup and
adapter test targets passed. In My Workspace, F20 display-name, availability,
aggregate-save, and rollback cases passed; three PageManager cases failed
before reaching F20 code because required documents/campuses resource packs
were unavailable, and no baseline checkout was available. The independent
review found no F20 defect and `git diff --check` passed. Setup's prefilled-name
reinitialization behavior still lacks a direct assertion; its fill-only-if-blank
source condition remains. Generic settings persistence, other feature-service
migrations, and Phase 2 remain open.

#### Progress update - 2026-09-24 (F21 unused class-navigation preferences cleanup)

Removed the unused `ClassNavigationPreferences` header and implementation,
their production and Classes Page test source entries, and stale includes.
`speaking_eval_page_p.h` now includes `class_tab_navigation_model.h` directly
for `ClassTabNavigation`. The active typed Application contracts, Platform
adapters, and preference tests remain.

Independent fresh Windows x64 Ninja/MSVC verification configured and built
376+8 steps; CTest passed 8/8 across ClassesPage, ClassTabNavigation, evaluation
defaults, and the five typed preference suites. CMake validated 872 source
owners. Searches found no deleted API/file references in `src`, `tests`,
`cmake`, or `CMakeLists.txt`; `git diff --check` passed. Phase 2 remains open.
At the close of F21, the next planned slice was calendar-import campus-code
lookup; F22 completes it below. Workbook decoding and other campus lookups
remain legacy.

#### Progress update - 2026-09-24 (F22 calendar-import campus-code query)

Calendar import now obtains campus codes through the Qt-free
`Application::CalendarEventImportCampusCodeQueryPort` and the Platform
`CalendarEventImportCampusCodeQueryAdapter`. The adapter owns `ResourcePaths`
and `CampusJsonRepository` access and accepts an injected directory for tests;
`CalendarEventImportService` no longer performs direct resource or repository
lookup. The query returns `std::vector<std::string>`.

The adapter preserves repository campus-name ordering, trims codes, removes
blank values, and retains only the first exact duplicate. Default,
malformed, and unreadable records are skipped; missing or empty directories
produce no codes. Workbook parsing, parser behavior, and CalendarPage behavior
are unchanged. A fixture includes actual Korean UTF-8. Independent fresh
Windows x64 Ninja/MSVC Debug configure and full 356-step build passed; CMake
validated 875 source owners, focused parser and adapter CTest passed 2/2, and
source/dependency and `git diff --check` reviews passed. Phase 2 remains open.
The next planned slice was CalendarPage's distinct campus metadata read; F23
completes it without reusing or widening the importer's campus-code-list port.

#### Progress update - 2026-09-24 (F23 CalendarPage campus-directory query)

CalendarPage campus metadata now crosses the Qt-free
`Application::CalendarPageCampusDirectoryQueryPort`; its Platform adapter
reads through `CampusJsonRepository` and accepts an injected fixture directory.
The query returns owning UTF-8 IDs and names with an optional code. CalendarPage
no longer reads `CampusJsonRepository` or `ResourcePaths` directly, and the
F22 importer code-list port remains separate.

Source comparison confirmed the existing availability branch and timing,
repository order, alias order and matching, trimmed display-name fallback,
whitespace-only-code behavior, and exact-empty removal/deduplication. Adapter
coverage includes Unicode and missing, empty, whitespace, blank, malformed,
default, and missing-directory cases. Executor and independent fresh Windows
x64 Ninja/MSVC configure/builds validated 878 handwritten owners; both focused
CTest runs passed 3/3 for the F23 adapter, F22 adapter, and CalendarEventCache.
No dedicated CalendarPage test exists. This closes the campus-directory
migration seam only; workbook parsing and broader calendar migration remain
open. The next slice is the personal-signature-image caller cutover.

#### Progress update - 2026-09-24 (F24 personal signature-image caller cutover)

`ApplicationServicesPersonalSignatureImagePort` retains its
`ApplicationServices&` constructor, adds a nullable `ApplicationServices*`
constructor, and removes the `SettingsService*` constructor. All five reads in
Initial Setup, My Information, and Speaking Eval now use the Application
adapter. The `myInfo/signatureImage` key, Base64 conversion, one-time
`SignatureImage::prepareForEmbedding`, read-only behavior, empty results, and
existing availability guards remain unchanged.

Executor verification reused the Ninja/MSVC x64 configure with 878 handwritten
owners, built `ClassMngr` and the adapter, Initial Setup, and MyWorkspace
targets, and passed focused CTest 3/3; source scan and `git diff --check` were
clean. Independent fresh configure/build also validated 878 owners and compiled
all targets. Focused CTest passed 2/3: adapter and Initial Setup passed; three
MyWorkspace PageManager cases failed because the `documents` and `campuses`
resource packs were unavailable. Running all 18 MyWorkspace functions
individually confirmed the F24 image preview, missing/corrupt/unavailable
image, display-name, and aggregate-save cases passed; only those same three
resource-dependent cases failed. This is a fresh-tree resource limitation, not
an F24 repair. Phase 2 remains open. The next planned slice was F25
custom-color adapter constructor cleanup; F25 completes it below.

#### Progress update - 2026-09-24 (F25 custom-color adapter constructor cleanup)

The custom-color adapter retains its `ApplicationServices&` constructor, adds a
nullable `ApplicationServices*` constructor, and removes `SettingsService*`.
Seven callers in five UI files now pass their `ApplicationServices*`: Schedule
Editor (two), Testing Classes (two), Schedule Import Review, Class Details, and
Initial Setup. The `custom_colors` key, all 16 palette slots, payload formats,
defaults, unrelated settings, and getColor load-before/save-after/cancel
behavior remain unchanged. No ColorUtils logic changed.

Executor fresh Ninja/MSVC x64 configure validated 878 handwritten owners; the
382-step build covered `ClassMngr`, adapter and ColorUtils tests, Initial Setup,
Testing Classes, Schedule Import Review, and ScheduleWidget targets. Focused
CTest passed 6/6. Independent fresh configure also validated 878 owners; the
repeat Ninja build returned success with no work, and the same focused suites
passed 6/6. Source scan found exactly seven callers and no `SettingsService*`
constructor or use; `git diff --check HEAD` passed. Schedule Editor and Class
Details have no picker-specific tests, though their translation units compiled
through `ClassMngr`. Phase 2 remains open. The next candidate was F26 Sub Prep
typed settings-gate removal; it is completed below.

#### Progress update - 2026-09-24 (F26 Sub Prep typed settings-gate removal)

Sub Prep no longer exposes the raw `openSettingsService` helper. Saved-content
and Zoom preference paths use their existing typed ports with
`ApplicationServices`; the nullable current-campus port checks availability
before campus loading or mutation. When settings are unavailable, save returns
before stopping autosave or restoring grading. The four existing preference
paths and keys, atomic save behavior, grading default, Zoom primary/legacy
fallback and best-effort migration, and campus matching/fallback are preserved.
The full campus-detail lookup and all-years calendar read remain untouched.

Page tests cover unavailable loading with sentinel fields and unavailable save
with page values, dirty state, timer, blank grading, and stored settings
preserved. The test stub defaults to database-open; each unavailable fixture
sets it false before page construction and does not close its fake service.
Executor and independent fresh Ninja/MSVC x64 configure runs each validated
878 handwritten owners and built `ClassMngr`, the page, and three adapter
targets. Focused CTest passed 4/4 in both runs; the independent repeat build
returned exit 0 with no work. `git diff --check` was clean. No verification gaps
remain. Phase 2 remains open. The next candidate was F27 My Information campus
chooser directory query; F27 completes it below.

#### Progress update - 2026-09-24 (F27 My Information campus-directory query)

My Information now reads campus chooser metadata through the Qt-free
`MyInfoCampusDirectoryQueryPort`, implemented by a Platform adapter over
`CampusJsonRepository`. The query returns owning UTF-8 IDs and display names;
the adapter preserves repository order, trimmed name/ID fallback, and the raw
ID. The page no longer reads `CampusJsonRepository` or `ResourcePaths`
directly. Stored ID/name matching and correction writes remain unchanged.

Two new Application/Platform query suites and the existing MyWorkspace behavior
test cover the cutover. Executor and independent fresh Ninja/MSVC x64
configures validated 882 handwritten owners, built `ClassMngr`, MyWorkspace,
and both new suites, and passed focused CTest 3/3. The independent repeat build
returned exit 0 with no work; diff check passed. `CampusJsonCodec` normalizes a
blank ID/name to `campus`, so the empty-display filter cannot be exercised from
a repository fixture; the filter remains in place. Phase 2 remains open. Next
#### Progress update - 2026-09-24 (F28 Sub Prep campus-detail directory query)

Sub Prep campus details now cross the Qt-free
`SubPrepCampusDirectoryQueryPort`; its Platform adapter wraps
`CampusJsonRepository` and accepts an injected fixture directory. The page no
longer references `CampusJsonRepository`, `ResourcePaths::Campuses`, or
`CampusInfo`. It preserves repository order and omission, trimmed
case-insensitive saved ID/name matching, first-campus fallback, availability
checks before lookup or state mutation, raw selected IDs, and `N/A` for empty
detail fields.

New Application and Platform suites cover ordering, UTF-8 fields, fallback,
malformed/default records, and missing or empty directories; the page suite
checks selected details and `N/A`. Executor and independent fresh Ninja/MSVC
x64 configures validated 886 handwritten owners and built `ClassMngr`, the
SubPrepPage suite, and both new suites. Executor CTest passed 3/3; independent
CTest passed 6/6, including three existing preference suites. Resource
generation passed, and the independent repeat build had no work. Phase 2
remains open. F29 completes the Personal Details atomic-save caller cutover;
workbook decoding, generic settings, other feature services, broader document
work, and the formal Phase 2 exit gate remain open.

#### Progress update - 2026-09-24 (F29 Personal Details atomic-save caller cutover)

`ApplicationServicesPersonalDetailsSavePort` retains its
`ApplicationServices&` constructor, adds a nullable `ApplicationServices*`
constructor, and removes the `SettingsService*` constructor. Initial Setup and
My Information now pass their existing `ApplicationServices*`. The adapter
still checks availability before saving, prepares UTF-8 values and signature
image data, normalizes the request, and performs one atomic `saveAll` for all
nine keys. Initial Setup's failure warning, My Information's early return
before autosave cancellation or field normalization, and rollback behavior
remain intact.

Adapter tests cover the nullable constructor; a MyWorkspace regression verifies
that an unavailable save preserves whitespace Zoom fields and dirty state.
Executor and independent fresh Ninja/MSVC x64 configs each validated 886
handwritten owners, built `ClassMngr`, the adapter, InitialSetupWizard, and
MyWorkspace targets, and passed focused CTest 3/3. Diff check and caller/
constructor scan passed; no resource limitation occurred. Phase 2 remains
open. The next slice is F30: cut the personal-signature-preferences callers in
My Information and Initial Setup over to nullable `ApplicationServices*`,
preserving defaulting, value normalization, UTF-8 text, availability, and
read-only/no-write behavior. F30 completes below.

#### Progress update - 2026-09-24 (F30 personal-signature-preferences caller cutover)

`ApplicationServicesPersonalSignaturePreferencesPort` retains its
`ApplicationServices&` constructor, adds a nullable `ApplicationServices*`
constructor, and removes `SettingsService*`. My Information and Initial Setup
now pass their existing `ApplicationServices` owners. Availability guards,
exact read-only keys and defaults, UTF-8 typed text, mode/font normalization,
unavailable failure behavior, and the no-write contract remain unchanged.

Executor and independent fresh Ninja/MSVC x64 configs each validated 886
handwritten owners, built `ClassMngr` and the adapter, InitialSetupWizard, and
MyWorkspace targets, and passed focused CTest 3/3. Diff and source scans passed;
there was no resource limitation. Phase 2 remains open. The next slice is F31:
cut the current-campus-preferences callers over to nullable
`ApplicationServices*` while preserving caller guards and read/correction
semantics. Workbook decoding, generic settings, other feature services,
broader document work, and the formal Phase 2 exit gate remain open.

#### Progress update - 2026-09-24 (F31 current-campus-preferences caller cutover)

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
check passed; there was no resource limitation. Phase 2 remains open. F32 is
the Sub Prep personal-Zoom preference caller cutover: remove the raw
`SettingsService*` constructor from
`ApplicationServicesSubPrepPersonalZoomPreferencesPort` and update My
Information and Initial Setup to use `ApplicationServices*`, preserving
primary-over-legacy precedence, best-effort migration only when primary
values are absent, legacy-value return on migration failure, UTF-8/defaults,
unavailable handling, and UI behavior. Workbook decoding, generic settings,
other feature services, broader document work, and the formal Phase 2 exit
gate remain open.

#### Progress update - 2026-09-24 (F32 Sub Prep personal-Zoom preference caller cutover)

`ApplicationServicesSubPrepPersonalZoomPreferencesPort` retains its
`ApplicationServices&` constructor, adds a nullable `ApplicationServices*`
constructor, and removes the `SettingsService*` constructor. My Information and
Initial Setup now pass their existing services. Primary `myInfo/zoom*` values
take precedence; legacy `subPrep/personalZoom*` values are fallback inputs and
are best-effort migrated only when the primary key is absent. Migration failure
still returns the legacy values. UTF-8 conversion, defaults, unavailable
behavior, and page display remain unchanged. Null-constructor coverage was
updated.

The executor built `ClassMngr`, the adapter, MyWorkspace, and InitialSetupWizard;
focused CTest passed 1/1. An independent fresh Ninja/MSVC x64 configure
validated 886 handwritten owners, built all four targets, and passed focused
CTest 3/3. Adapter/source audits and diff check passed; there was no resource
limitation. Phase 2 remains open. F33 completes the typed settings-availability
migration for My Information and Initial Setup. Workbook decoding, generic
settings, other feature services, broader document work, and the formal Phase 2
exit gate remain open.

#### Progress update - 2026-09-24 (F33 My Information and Initial Setup typed availability boundary)

My Information and Initial Setup now use
`ApplicationServicesCurrentCampusPreferencesPort::isAvailable()` instead of
direct settings-availability access. My Information's raw availability helper
and Initial Setup's raw getter were removed. Guards preserve My Information's
load no-mutation and save-before-autosave-cancel/Zoom-normalization behavior,
and Initial Setup's early returns. Regressions verify that unavailable My
Information loading preserves sentinel fields and unavailable Initial Setup
validation preserves the entered name and signature preview, stays on the page,
and shows no warning; the unavailable-initialization test remains.

Executor and independent fresh Ninja/MSVC x64 configure/builds validated 886
handwritten owners and built `ClassMngr`, the CurrentCampus adapter,
MyWorkspace, and InitialSetupWizard. After a test-only coverage repair, the
independent rerun passed the exact focused CTest suites
`ClassMngrInitialSetupWizardTests`, `ClassMngrMyWorkspacePageTests`, and
`ClassMngrNextPlatformApplicationServicesCurrentCampusPreferencesPortTests`
(3/3). Source scans and diff check passed. Phase 2 remains in progress.

F34 Class Notes save is recorded above. F35's Sub Prep calendar interval query
is recorded below. Next, audit current code against this plan and the formal
Phase 2 exit gate. Phase 2 remains in progress and the exit gate remains open.

#### Progress update - 2026-09-24 (F34 Class Notes save cutover)

Only the Class Notes page save path now uses the Qt-free
`Application::ClassNotesSavePort`; its `std::u16string` fields preserve the
existing exact 10,000 UTF-16-code-unit semantics. The Platform
`ApplicationServicesClassNotesSavePort` maps to the existing
`ClassService::saveClassNotes`. The page's other reads remain unchanged.

The cutover preserves trimming, the single two-field upsert, warning and
autosave behavior, dirty state on failure, and clean state on success. New
contract, adapter, and feature-page suites plus the existing Classes page and
DataServiceLifecycle suites passed 5/5 on a fresh x64 Ninja/MSVC configure;
all targets built with 891 handwritten owners. The added
`defaultPortSavesBothFieldsToPersistence()` case verifies persistence of both
fields and clean page state through the real default adapter. The Qt-free
contract/source audit and diff check passed.

#### Progress update - 2026-09-24 (F35 Sub Prep calendar interval query)

The typed `SubPrepCalendarEventIntervalsQuery` and Platform
`ApplicationServicesSubPrepCalendarEventIntervalsPort` replace Sub Prep's
direct generic calendar range read. One captured date sets the inclusive range
from January 1 of its calendar year through December 31 of the following year
(the current and following calendar years at most), clamped at year 9999, and
is also passed to the dialog. The repository selects
only normalized Vacation/Holiday events overlapping the full range; invalid
or reversed intervals are ignored. This purpose-specific read does not apply
the generic 4,096-event projection cap.

Unavailable services, query failures, and conversion exceptions still produce
empty intervals while generation continues. Independent fresh Ninja/MSVC x64
verification validated 894 source owners, built the executable and focused
targets, and passed the eight focused CTests 8/8. Coverage includes 4,097
events, the production ApplicationServices-to-repository path, the two-year
boundary and year-9999 clamp, interval overlaps, day-28/day-29 cases, and
historical/future holiday bridges. `git diff --check` passed. The exceptional
conversion fallback was source-inspected but not fault-injected.

The read-only exit-gate audit is recorded above. Phase 2 remains in progress
and its formal exit gate remains open; next, select a slice from the remaining
gate gaps.

#### Progress update - 2026-09-24 (F36 Calendar import parity)

The production `CalendarEventImportService` now has deterministic offline
coverage using a required checked-in XLSX fixture, loopback transport, and a
temporary database. The test exercises workbook parsing, typed existing-event
signature lookup, planner execution, and ordered batch save; it asserts exactly
three inserted events, two skipped rows, and the persisted event set.

Independent fresh Windows x64 configure/build and all five focused suites
passed. Parser-level signature deduplication and planner duplicate-candidate
handling are covered by their separate planner suite, not end-to-end in this
service test. This closes only the exercised Calendar import-planning parity
path; Domain completeness and broader validation, conflict, import, and state
parity remain open. Phase 2 remains in progress and its formal exit gate is
open.

#### Progress update - 2026-09-24 (F37 Schedule import state validation contract)

`ClassMngr::Next::Application` now owns the Qt-free
`validateScheduleImportState` contract. The legacy repository adapts its
structurally checked plan and existing teacher/class snapshots to the request
after snapshot reads and before applying writes in the current transaction;
the duplicate legacy state validator was removed. The request covers
Normal/Intensive projections, teacher reuse/update-room/create/skip
resolution, class update/create/skip resolution, selected-target availability
and identity, the unique exact-match rule for skipped classes, valid projected
day/time ranges, and overlaps. It preserves skipped classes in the projected
normal schedule and absent intensive classes only in intensive update mode.

The repository test installs a SQLite `BEFORE UPDATE` trigger that aborts if a
teacher write is reached before validation. A stale selected class is rejected
by the new contract, and persisted teacher, class, schedule-time, and settings
sentinels remain unchanged. This proves pre-write rejection for that path; it
does not prove every validation rule through the database integration.

The Application contract is Qt-free; this does not establish that the whole
`src/next` tree is Qt-free. Executor and independent Tester each completed a
fresh Windows x64 configure with 895 handwritten source owners, and both
focused suites passed 2/2. Schedule matching/preview, required workbook parity,
the workbook decoder, and broader Domain completeness remain open. Gate 2 is
partial; Phase 2 remains in progress and its formal exit gate remains open.

#### Progress update - 2026-09-24 (F38 Schedule Import matching and preview)

Schedule Import now uses the Qt-free
[`ScheduleImportMatchingProjection`](../../src/next/application/schedule_import_matching_projection.h)
contract, and [`ScheduleImportRepository::preview`](../../src/data/repositories/schedule_import_repository.cpp)
is wired through it. The legacy `ScheduleImportMatcher` was removed. At the Qt
edge, grade, level, and room matching keys are simplified and case-folded; raw
room text remains available for display. The app-less
[`matching-projection suite`](../../tests/next_application_schedule_import_matching_projection_tests.cpp)
covers all seven ranking buckets, stable ties, no-match and inventory results,
and Normal/Intensive fallback.

The required checked-in [`schedule_review.xlsx`](../../tests/fixtures/imports/schedule_review.xlsx)
drives production preview with no skip or external-workbook path. The database
fixture in [`schedule_import_tests.cpp`](../../tests/schedule_import_tests.cpp)
seeds room as `' 416 '` against workbook room `416` and asserts exact/weaker
candidate IDs `[43,42]`, suggestion `43`, exact/confident status, two regular
and no intensive inventory candidates, and initially absent IDs. Executor and
independent fresh x64 Ninja/MSVC configure/build runs each validated 895
source owners, built both focused targets, and passed CTest 2/2. QtTest
reported Schedule 24 passed, 0 failed, 1 skipped (only the optional external
sample was unset), and matching 5 passed, 0 failed, 0 skipped.

F38 improved Gate 2 but left review-time conflict projection open; F39 now
provides that shared Application projection and fixture-backed review/apply
coverage. Wider baseline parity, shared workbook decoding, and broader Domain
completeness remain open. Phase 2 remains in progress and the formal exit gate
remains open. F38 is committed as
`bc6ac011504e0a499a8cdfd4b1533b49ea3f4bcb`.

#### Progress update - 2026-09-24 (F39 Schedule Import conflict projection)

The Qt-free standard-C++
[`schedule_import_overlap_projection.h`](../../src/next/application/schedule_import_overlap_projection.h)
is now shared by Schedule Import review presentation and apply-time state
validation. It owns half-open overlap versus adjacency, matching days,
deterministic conflict order, Normal/Intensive projection, skipped classes,
and retained intensive schedules. UI translation remains at the edge.

The required checked-in
[`schedule_overlap_conflict.xlsx`](../../tests/fixtures/imports/schedule_overlap_conflict.xlsx)
drives production preview and review: it shows the expected conflict warning
and disables import in [`schedule_import_dialog_tests.cpp`](../../tests/schedule_import_dialog_tests.cpp).
Apply rejects the conflicting state, and database assertions in
[`schedule_import_tests.cpp`](../../tests/schedule_import_tests.cpp) verify
that no teachers, classes, or `class_times` rows persist; the F37 pre-write
trigger sentinel also passes. App-less rule coverage is in
[`next_application_schedule_import_state_validation_tests.cpp`](../../tests/next_application_schedule_import_state_validation_tests.cpp).
Independent Tester
`PH2-F39-INDEPENDENT-VERIFY` configured a fresh Windows x64 Ninja/MSVC build
with 896 handwritten source owners, built three focused targets, and passed
CTest 3/3. QtTest passed 25 ScheduleImport, 21 ScheduleImportDialog, and 12
Application state-validation tests (58 passed, 0 failed), with one existing
optional external-workbook skip because `CLASSMNGR_SCHEDULE_IMPORT_SAMPLE` was
unset. `git diff --check` passed.

F39 is committed as `3121d90c2db6af8e225048f016eec6f0843c1c18`. It improves
Gate 2 but does not close Phase 2: wider baseline parity and the remaining
work listed in the gate audit remain open.

#### Progress update - 2026-09-24 (F40 Domain schedule-time value)

Standard-C++ [`Domain::Weekday` and `Domain::ScheduleTime`](../../src/next/domain/schedule_time.h)
now provide a validated schedule value with weekday and minute bounds,
half-open overlap behavior, and value comparison. Raw Application inputs are
retained for error reporting. After validation,
`ScheduleImportProjectedTime` carries the Domain value plus Application-owned
labels through the overlap projection; apply validation and F39 UI review both
consume it. Invalid raw values preserve `InvalidProjectedTime` labels.

The independent `PH2-F40-SCHEDULE-TIME-VERIFY` recheck configured a fresh
Windows x64 Ninja/MSVC Debug build with 897 handwritten sources, each with one
explicit owner, and passed four focused CTest suites (4/4). QtTest passed
[`Domain`](../../tests/next_domain_contract_tests.cpp) 8,
[`state validation`](../../tests/next_application_schedule_import_state_validation_tests.cpp)
13, [`Schedule Import repository`](../../tests/schedule_import_tests.cpp) 25,
and [`review dialog`](../../tests/schedule_import_dialog_tests.cpp) 21 cases
(67 passed, 0 failed), with one existing optional external-workbook skip because
`CLASSMNGR_SCHEDULE_IMPORT_SAMPLE` was unset. The F39 fixture, conflict warning,
disabled review action, zero-write rejection, and F37 pre-write trigger
sentinel passed; `git diff --check` passed.

F40 is committed as `2ab23fb1796dfb1761a4c48644869a9ae6e1060d`. It adds Domain
coverage but no baseline-parity scope; Phase 2 remains in progress and the
formal exit gate remains open.

#### Progress update - 2026-09-24 (F41 Schedule Import review decisions)

The Qt-free
[`schedule_import_review_decisions.h`](../../src/next/application/schedule_import_review_decisions.h)
is the shared authority for teacher/class choice validation in dialog
readiness and the PlanValidator adapter. It checks complete, unique
resolutions; valid actions and targets; duplicate targets; required or foreign
rooms; and skipped teacher/class consistency. Workbook content, colors, and
meeting validation remain at the feature edge; SQL and stale/current-state
checks remain in the repository and F37 validator.

Required [`schedule_review.xlsx`](../../tests/fixtures/imports/schedule_review.xlsx)
now exercises production parse, preview, explicit review choices, and
repository apply. Assertions cover the result summary, persisted teacher,
class, color, and time state, and retained unrelated class metadata. The F39
conflict fixture still verifies the review warning, disabled Import action,
and zero-write apply rejection; the F37 pre-write trigger sentinel passes.

Independent verification `PH2-F41-SCHEDULE-IMPORT-VERIFY` configured fresh
Windows x64 Debug/Ninja/MSVC with 899 handwritten source owners; four focused
CTest suites passed 4/4. QtTest passed the
[`decision contract`](../../tests/next_application_schedule_import_review_decisions_tests.cpp)
26/0/0, [`Schedule Import repository`](../../tests/schedule_import_tests.cpp)
25/0/1, [`review dialog`](../../tests/schedule_import_dialog_tests.cpp) 21/0/0,
and [`F37 state validation`](../../tests/next_application_schedule_import_state_validation_tests.cpp)
13/0/0 (85 passed, 0 failed, one existing optional skip because
`CLASSMNGR_SCHEDULE_IMPORT_SAMPLE` was unset). `git diff --check` passed.

F41 is committed as `30ec8d7512a8847a5b1d32addabf25f252b0eabb`. This expands
fixture-backed Schedule review/apply parity but does not complete Gate 2 or
Phase 2; wider baseline parity and the remaining gate-audit items remain open.

#### Progress update - 2026-09-24 (F42 workspace replacement failure handling)

[`FileController` workspace integration tests](../../tests/file_controller_workspace_lifecycle_tests.cpp)
verify that a failed production profile replacement-open preserves the active
database, settings sentinel, recent/last-file entries, and UI action
availability. If the coordinator reports close failure, interactive create
stops before preparing or replacing the selected target. No new `src/next`
production file was added.

Independent fresh x64 Ninja/MSVC verification validated 899 handwritten
owners. Four focused CTest targets passed 17/17, 33/33, 25/25, and 11/11; all
passed. `git diff --check` was clean. The formal workspace criterion is
satisfied by the verified `WorkspaceGateway::createWorkspace` and
`WorkspaceCoordinator` dirty-rejection, successful-state-transition, and
failure-atomicity cases. This does not close Phase 2.

Non-gating FileController gaps remain: dirty-page approval still comes from
`MainWindow`; normal new-profile and initial-setup flows close the current
session before target preparation and coordinator create succeed; and
same-path replacement plus the complete live MainWindow snapshot lack direct
coverage. F42 is committed as
`8b2eb8a2a7a0dae6a22ce8a4163b35d5d9dd24ee`. Phase 2 remains In Progress and
the formal exit gate remains open.

#### Progress update - 2026-09-24 (F43 Class Transfer review decisions)

The Qt-free [`class_transfer_projection.h`](../../src/next/application/class_transfer_projection.h)
provides shared Class Transfer review-decision validation to dialog readiness
and repository apply validation. Required checked-in
[`success_source.json`](../../tests/fixtures/transfers/success_source.json)
drives fixture-backed apply and verifies persisted class, teacher, schedule
(including end time), and roster state. The checked-in
[`conflict_source.json`](../../tests/fixtures/transfers/conflict_source.json)
verifies a conflicting transfer performs no partial writes. The application
contract remains free of Qt and legacy Application dependencies.

Independent fresh Windows x64 Ninja/MSVC verification validated 899 handwritten
source owners; both focused CTest targets passed 2/2, and the app-less and
repository QtTest suites passed 19 and 15 cases respectively. `git diff --check`
was clean. See the [app-less contract tests](../../tests/next_application_class_transfer_tests.cpp)
and [repository integration tests](../../tests/class_transfer_tests.cpp).
F43 is committed as `c0e03e55aa5f5cc1897ccf97a25901a5e119c8e5`. It advances
Domain/Application behavior and fixture-backed parity but leaves both gates
partial; the formal WorkspaceCoordinator create criterion and audited
dependency-isolation criterion remain satisfied. Phase 2 remains In Progress
with its exit gate open. Sub Prep remains limited to the current and following
calendar years at most.

## Verified F44 Teacher Import review decisions - commit `28170a914a4dc76dc62f66677ad8f1067dfd42bf`

Qt-free [`import_review_session.h`](../../src/next/application/import_review_session.h)
provides Teacher Import review-decision validation shared by production dialog
readiness/plan creation and repository apply. The repository validates that
reviewed candidate identities match the plan before opening its transaction;
the contract has no Qt or legacy Application includes.

Required checked-in
[`sectioned_review.xlsx`](../../tests/fixtures/teacher_import/sectioned_review.xlsx)
drives the production dialog to produce the plan passed directly to apply in
[`teacher_import_dialog_tests.cpp`](../../tests/teacher_import_dialog_tests.cpp).
It verifies selected and omitted candidates and persisted Korean, Native English,
and GS Team records (3/0/0 pass/fail/skip). The repository suite also verifies
deterministic summary values, manually maintained-field preservation, and that
invalid reviewed plans neither write records nor advance the source date.
Direct invalid-decision coverage in
[`teacher_import_tests.cpp`](../../tests/teacher_import_tests.cpp) passed 3/0/0,
including an empty group ID.

Independent fresh out-of-tree Windows x64 MSVC/Ninja Debug verification with
Qt 6.12.0 built both focused Teacher Import targets and passed CTest 2/2. The
required fixture case did not skip; optional external-sample tests skipped
because `CLASSMNGR_TEACHER_IMPORT_SAMPLE` was unset. `git diff --check` passed.
Gate 1 and Gate 2 advance but remain Partial; Workspace boundary and dependency
isolation remain Satisfied. Wider baseline parity, shared workbook decoding,
broader Domain records, generic settings, remaining feature migrations, and
broader calendar/UI and document work remain open. Phase 2 remains In Progress
with its exit gate Open. Sub Prep remains capped at the current and following
calendar years at most.

## Verified F45 Domain Course catalog - commit `eb2167e9d39a65446265b9506d749dc0e6be0d35`

Qt-free [`Domain::Course`](../../src/next/domain/course.h) owns the exact
ordered 25 supported grade/level pairs. [`ClassInfoConfig`](../../src/features/classes/config/class_info_config.cpp)
adapts that catalog to the existing Qt lists, and
[`ScheduleImportPlanValidator`](../../src/features/schedule/services/schedule_import_plan_validator.cpp)
validates imported pairs through `Domain::Course`. This is a course-value and
validation boundary; broader Domain records and other class/catalog consumers
remain open.

The required checked-in
[`schedule_review.xlsx`](../../tests/fixtures/imports/schedule_review.xlsx)
still applies its valid pairs successfully (3/0/0 pass/fail/skip). The three
direct Course cases each reported 3/0/0; valid-fixture apply and repaired
invalid-course apply also each passed 3/0/0. The invalid case in
[`schedule_import_tests.cpp`](../../tests/schedule_import_tests.cpp) seeds
`teacher`, `class`, `class_info`, `class_times`, and profile-setting snapshots and
confirms all remain unchanged after rejection. After the independent review
found empty-table counts insufficient to prove preservation, the test was
strengthened and the same tester reran the target build and rejection case.

Independent fresh Windows x64 MSVC 19.51/Ninja Debug verification with Qt 6.12.0
built `ClassMngrNextDomainContractTests` and `ClassMngrScheduleImportTests`;
focused CTest passed 2/2. Source ownership validated 900 handwritten sources,
with `course.h` assigned once under `CLASSMNGR_NEXT_DOMAIN_SOURCES`.
`git diff --check` passed; the full suite was not run. Gate 1 and Gate 2 advance
but remain Partial; workspace boundary and audited v2 dependency isolation
remain Satisfied. Wider baseline parity, shared workbook decoding, broader
Domain records, generic settings, remaining feature migrations, and broader
calendar/UI and document work remain open. Non-gating FileController caveats
remain recorded above. Phase 2 remains In Progress with its exit gate Open.
Sub Prep remains capped at the current and following calendar years at most.

## Verified F46 Korean teacher identity key - commit `bedb52e0045731bba3e4f4b7a576021bdc007b72`

Qt-free [`Domain::KoreanTeacherKey`](../../src/next/domain/korean_teacher_key.h)
owns the shared UTF-16 Hangul code-unit ranges U+1100–U+11FF, U+3130–U+318F,
U+A960–U+A97F, U+AC00–U+D7AF, and U+D7B0–U+D7FF. It filters other code units while
preserving order, does not trim or normalize, and permits an empty key.
[`TeacherImportNameUtils`](../../src/features/teacher/import/teacher_import_name_utils.h)
converts QString values at the edge; Schedule matching uses the same Domain key
through [`ScheduleImportMatchingProjection`](../../src/next/application/schedule_import_matching_projection.h),
removing its duplicate range implementation. Contract tests cover range
boundaries, excluded code units, empty/equality/accessor semantics, and
composed, decomposed, and compatibility forms. Existing empty-key behavior
remains: the contact parser keeps its validation, the repository keeps its
required-name error and leaves the row count unchanged, and Schedule matching
keeps its empty-key match.

Required fixture coverage remains green for Teacher Import parsing, review,
persistence, and the production dialog-plan-to-repository path using
[`sectioned_review.xlsx`](../../tests/fixtures/teacher_import/sectioned_review.xlsx);
Schedule matching/persistence using
[`schedule_review.xlsx`](../../tests/fixtures/imports/schedule_review.xlsx);
and overlap rejection-before-write using
[`schedule_overlap_conflict.xlsx`](../../tests/fixtures/imports/schedule_overlap_conflict.xlsx).
Evidence suites are [`Domain contracts`](../../tests/next_domain_contract_tests.cpp),
[`Schedule matching projection`](../../tests/next_application_schedule_import_matching_projection_tests.cpp),
[`Teacher Import`](../../tests/teacher_import_tests.cpp),
[`Teacher Import dialog`](../../tests/teacher_import_dialog_tests.cpp),
[`Schedule Import`](../../tests/schedule_import_tests.cpp), and
[`Schedule Import dialog`](../../tests/schedule_import_dialog_tests.cpp).

Independent fresh Windows x64 MSVC 19.51/Ninja 1.13/CMake 4.4.2/Qt 6.12.0
verification passed four focused CTests 4/4 and Teacher Import dialog CTest
1/1. QtTest counts: Domain 14/0/0, matching projection 6/0/0, Teacher Import
16/0/1, Schedule Import 26/0/1, and dialog 5/0/1; skips were optional
external-sample cases. Source ownership validated 901 handwritten files.
`git diff --check` passed; the full suite was not run. Gate 1 and Gate 2 remain
Partial; Workspace boundary and audited v2 dependency isolation remain
Satisfied. Wider baseline parity, shared workbook decoding, broader Domain
records, generic settings, remaining feature migrations, and broader
calendar/UI and document work remain open. Non-gating FileController caveats
remain recorded above. Phase 2 remains In Progress with its exit gate Open.
Sub Prep remains capped at the current and following calendar years at most.

## Verified F47 Course weekly meeting-day rule - commit `7cba8abf952b5b32f90391844beec68eac2c3f69`

Qt-free [`Domain::Course::WeeklyMeetingDayRule`](../../src/next/domain/course.h)
owns typed weekly meeting-day policy over `Domain::Weekday` patterns, rejecting
invalid, out-of-range, and duplicate weekdays and matching patterns without
depending on their order. The Schedule Import production parser/partitioning
and plan/apply validation use the Course rule; QString parsing and translated
diagnostics remain at the feature edge. Legacy grade trim/uppercase and
trimmed, case-insensitive Athena/Song's handling are preserved, as are the
allowed/forbidden categories and E5/Zeus Tuesday-only Skip behavior. Unsupported
`M3 Zeus` still has no meeting-pattern error and is rejected by separate Course
validation.

The required [`schedule_review.xlsx`](../../tests/fixtures/imports/schedule_review.xlsx)
persists accepted rows. A fixture-derived one-day E4/Theseus pattern is rejected
before writes, preserving seeded teachers, classes, class_info, class_times,
and app_settings snapshots. Existing invalid-course, overlap no-write, and
Skip-with-prohibited-pattern coverage remains green.

Executor and independent fresh x64 MSVC 19.51/Ninja 1.13/CMake 4.4/Qt 6.12
builds each passed focused CTest 2/2 for `ClassMngrNextDomainContractTests` and
`ClassMngrScheduleImportTests`; direct fixture persistence, rejection, overlap,
and Domain policy cases passed. `git diff --check` passed; the full suite was
not run. Gate 1 and Gate 2 advance but remain Partial. Workspace boundary and
audited `src/next` dependency isolation remain Satisfied. Phase 2 remains In
Progress with its exit gate Open. Sub Prep remains capped at the current and
following calendar years at most.

#### Progress update - 2026-09-25 (F49 Calendar Import use case)

Qt-free [`CalendarEventImportUseCase`](../../src/next/application/calendar_event_import_use_case.h)
composes the existing signature-query port, `planCalendarEventImport`, and
batch-save port. The production `CalendarEventImportService` delegates query,
deduplication, ordered signature/request pairing, saving, and imported/skipped
counts to the use case. Workbook/network/campus handling, signals, localized
errors remain at the feature edge; an injected observer preserves profiler
timing at the prior boundaries.

Six app-less fake-port cases cover ordered mapping, existing and in-batch
duplicates, parser skip counts, empty and duplicate-only inputs, exact UTF-16
signature identity (including a lone surrogate), and query/save failures. The
required [`calendar_import_parity_2026.xlsx`](../../tests/fixtures/imports/calendar_import_parity_2026.xlsx)
production path verifies persisted rows and counts. Executor and independent
Tester fresh builds each passed the focused CTest 2/2; no full suite was run.
F49 is committed as `6a41e958671b7fa93c301d8b25c9c4381178fd7f`. Gate 1 and Gate
2 advance but remain Partial; workspace create and audited v2 dependency
isolation remain Satisfied. Phase 2 remains In Progress with its exit gate
Open. Sub Prep remains limited to the current and following calendar years at
most.

#### Progress update - 2026-09-25 (F50 Calendar Import signature identity)

Qt-free [`Application::CalendarEventImportSignature`](../../src/next/application/calendar_event_import_signature.h)
is the shared six-field key value used by
[`academic_calendar_event_parser.cpp`](../../src/features/calendar/academic_calendar_event_parser.cpp)
and the signature-query port. The Qt adapters retain title simplification,
type/time-status normalization, and ISO date conversion; the value preserves
field order, delimiters, exact UTF-16 code units, and the all-day `1/0` flag,
and excludes times, database ID, and repeat-series ID. App-less coverage checks
the format, every field, UTF-16 code units, `%2` title text, and type members
without metadata. Existing CalendarImportTests covers normalization and
excluded metadata; a Qt 6.12 probe confirmed inserted `%2` text is not rescanned
by the legacy six-argument `QString::arg` call.

Executor and independent Tester each freshly configured Windows x64
MSVC/Ninja, validated 906 source owners, built three focused targets, and
passed CTest 3/3, including required `calendar_import_parity_2026.xlsx`
production parity. No full suite was run. F50 is committed as
`92d001db11d8c8eb973de5f238444abe855ea5c5`. Gate 1 and Gate 2 remain Partial;
Workspace boundary and audited `src/next` dependency isolation remain
Satisfied. Phase 2 remains In Progress with its exit gate Open. Sub Prep stays
capped at the current and following calendar years at most.

#### Progress update - 2026-09-25 (F51 typed Calendar Import signature flow)

F51 carries the Qt-free Application::CalendarEventImportSignature value
through signature-query results, plan inputs, candidate deduplication, and the
Calendar Import use case; the parser and Platform query adapter produce the
same typed value. This removes raw UTF-16 string conversions between those
Application boundaries; Qt normalization and date conversion remain at the
adapters. A fresh Windows x64 MSVC/Ninja configure validated 906 source owners.
Executor and independent Tester each verified seven focused CTest targets,
each passing 1/1:
ClassMngrNextApplicationCalendarEventImportSignatureTests,
ClassMngrNextApplicationCalendarEventImportSignatureQueryPortTests,
ClassMngrNextApplicationCalendarEventImportPlanTests,
ClassMngrNextApplicationCalendarEventImportUseCaseTests,
ClassMngrCalendarImportTests,
ClassMngrNextPlatformApplicationServicesCalendarEventPortTests, and
ClassMngrCalendarEventImportParityTests. The production path used the required
calendar_import_parity_2026.xlsx fixture. With QCOMPARE diagnostics restored,
ClassMngrCalendarImportTests was rebuilt and rerun, passing 1/1. No full suite
was run. F51 is committed as e940f0c0ed8a63e740a3c2375631a22c08875f84. Gate 1
improves but remains Partial because broader Domain and Application behavior
is outstanding; baseline parity Gate 2 remains Partial because parity coverage
is incomplete. Workspace create and audited v2 dependency isolation remain
Satisfied. Phase 2 remains In Progress with its exit gate Open.

#### Progress update - 2026-09-25 (F52 centralized Calendar event timing)

F52 adds Qt-free [`Domain::CalendarEventTiming`](../../src/next/domain/calendar_event_timing.h)
and uses it in `CalendarEventEditDraft`, `CalendarEventSavePort`, and
`CalendarEventSeriesEditPort`. Canonical dates require hyphens at positions 4
and 7 and digits elsewhere; regression coverage rejects `2026006010` and
`2026-06110`. The feature boundaries retain their specific errors and
validation order, including the existing cross-day clock rule.

Fresh MSVC 19.51/Ninja/Qt 6.12 configure validated 907 source owners; three
focused targets built, exact CTest passed 3/3, and the checked-in Calendar
Import parity fixture passed. No full suite was run. F52 is committed as
`9cd9a2a4469482bc803cdc172d18a072c0fb3949`. Gate 1 and Gate 2 remain Partial;
Workspace boundary and audited `src/next` dependency isolation remain
Satisfied. Phase 2 remains In Progress with its exit gate Open. Sub Prep stays
capped at the current and following calendar years at most.

#### Progress update - 2026-09-25 (F53 Roster Score Import parity)

[`roster_editor_widget_import_tests.cpp`](../../tests/roster_editor_widget_import_tests.cpp)
exercises the real `RosterEditorWidget::importScores` slot through
`ClassMngrRuntime`. It seeds saved evaluations through production services in
a temporary database; this workflow reads saved evaluations and does not parse
a workbook. Coverage verifies all four grade columns, English/Korean name-pair
matching including collisions, preservation of unmatched, empty, and
English-only partial rows, autosave persistence via a fresh roster-service
read, idempotent re-import, and missing-English/Korean-column warnings with no
persisted changes. A mixed Winter score totaling 16/6 is asserted as B+.

Fresh independent Ninja/MSVC 19.51/Qt 6.12 configure validated 908 source
owners; `ClassMngrRosterEditorWidgetImportTests`,
`ClassMngrRosterModelTests`, and `ClassMngrSpeakingEvaluationServiceTests`
built and exact CTest passed 3/3. No full suite was run. F53 expands Gate 2
parity evidence only: Gate 1 and Gate 2 remain Partial; Workspace boundary
and audited `src/next` dependency isolation remain Satisfied. Phase 2 remains
In Progress with its exit gate Open. Sub Prep remains capped at the current
and following calendar years at most.

#### Progress update - 2026-09-25 (F54 Calendar event vocabulary)

F54 adds Qt-free Domain classifiers for the six Calendar event type names and
three time statuses in `calendar_event_timing.h`. The calendar edit-draft,
single-save, and repeat-series-edit validators reuse these classifiers while
preserving request strings and raw fields, trimming at each Application
boundary, 64-character limits, validation order, feature-specific errors, and
projection behavior. Domain and Application tests cover known and unknown
values, casing/whitespace, raw-field preservation, length boundaries, and
request-specific errors.

Fresh independent Ninja/MSVC 19.51/Qt 6.12 configure/build ran
`ClassMngrNextDomainContractTests` and
`ClassMngrNextApplicationCalendarEventTests`; exact CTest passed 2/2. No full
suite was run. F54 adds Gate 1 evidence only; Gate 1 and Gate 2 remain Partial,
Workspace boundary and audited `src/next` dependency isolation remain
Satisfied, and the Phase 2 exit gate remains Open. Sub Prep remains capped at
the current and following calendar years at most.

#### Progress update - 2026-09-25 (F55 shared Course grade-band classification)

`Domain::Course::gradeBandForName` provides Qt-free classification from the
grade name alone. Classes page visibility, Evaluation Default Selection, and
Schedule testing-score suppression share this classifier while retaining each
caller’s existing `trimmed().toUpper()` normalization and its own policy:
Classes hides Analytics/Evaluations for M1–M3 subject to the preference;
Evaluation selects Middle only for M1–M3 and Elementary otherwise; Schedule
suppresses M2/M3, and suppresses M1 only when `testingAffectsM1` is true.
Classification remains independent of whether a grade/level pair is valid.

Coverage includes all six bands plus Other, invalid, case, whitespace, and an
invalid level-pair classification; class preference behavior; the grade/school-
level helper used by Evaluation Default Selection; and Schedule suppression
without assignments, including the M1 toggle. The Evaluation test calls the
same private policy helper used by `forClass`, but does not exercise the full
`ApplicationServices` integration.

Independent fresh MSVC 19.51/Ninja/Qt 6.12 verification validated 908
handwritten owners, built `ClassMngrNextDomainContractTests`,
`ClassMngrClassesPageTests`, `ClassMngrSchedulePrintModelTests`, and
`ClassMngrEvaluationDefaultSelectionTests`, and passed exact CTest 4/4. No
full suite was run. F55 adds Gate 1 evidence only: Gate 1 and Gate 2 remain
Partial; Workspace boundary and audited `src/next` dependency isolation remain
Satisfied; the Phase 2 exit gate remains Open. Sub Prep remains capped at the
current and following calendar years at most.

#### Progress update - 2026-09-26 (F56 Speaking Evaluation grade contract)

Qt-free [`Domain::SpeakingEvaluationGrade`](../../src/next/domain/speaking_evaluation_grade.h)
represents the six evaluation criteria and C/B/B+/A/A+ values, parses exact
labels, and centralizes legacy aggregation, >=0.4 rounding, invalid/missing
outcomes, and clamping. It replaces duplicated calculation in roster import,
the report data assembler, and the report widget. Repository import continues
to trim input; report paths continue to require exact labels.

Domain coverage exhausts all 15,625 valid combinations and checks invalid or
missing values, labels, and rounding. The real roster widget-import test adds a
padded saved label and incomplete evaluation (N/A), retaining F53's mixed
16/6-to-B+ result, persistence, and idempotence coverage. Report-widget tests
check B+ and N/A through assembly and rendering. Independent fresh
MSVC 19.51/Ninja/Qt 6.12 verification validated 909 handwritten source owners,
built `ClassMngrNextDomainContractTests`,
`ClassMngrRosterEditorWidgetImportTests`,
`ClassMngrSpeakingEvaluationServiceTests`, and
`ClassMngrSpeakingEvalReportWidgetTests`, and passed exact CTest 4/4. No full
suite was run. F56 is committed as
`c73e896fe34e186a045d73b653aa8ec9dfa89e83`. Gate 1 and Gate 2 advance but
remain Partial; Workspace boundary and audited `src/next` dependency isolation
remain Satisfied; the Phase 2 exit gate remains Open. The Evaluation Default
Selection test still exercises its policy helper rather than full
`ApplicationServices::forClass` integration. Sub Prep remains capped at the
current and following calendar years at most.

#### Progress update - 2026-09-26 (F57 Evaluation Default Selection contract, commit `b38b3afef0088b4c05d6d540dda15600f48c7f59`)

Qt-free `Application::EvaluationPeriod` now selects Winter, Speech Contest,
Summer, Fall, or no evaluation for All, supporting current/previous period
selection and the Winter-to-Fall cycle. The feature adapter preserves exact
legacy labels and schedule/service boundaries. App-less tests cover populated
and empty current/previous cycles, All, and invalid input. Temporary-database
integration invokes production `EvaluationDefaultSelection::forClass`: at
2026-09-07 it selects current Fall for M2 with Summer fallback, and current
Summer for E4 with Speech Contest fallback; it also covers All, missing saved
schedule, and missing ClassInfo. Failed class-info or evaluation reads return
no default; the integration test does not explicitly simulate an evaluation
read failure.

Executor verification built `ClassMngrEvaluationDefaultSelectionTests`,
`ClassMngrEvaluationDefaultSelectionIntegrationTests`,
`ClassMngrNextApplicationEvaluationDefaultSelectionTests`,
`ClassMngrNextPlatformApplicationServicesAcademicCalendarSchedulePreferencesPortTests`,
and `ClassMngrNextPlatformApplicationServicesEvaluationDefaultPolicyPortTests`;
exact CTest passed 5/5. Fresh independent Ninja/MSVC 19.51/Qt 6.12 verification
validated 912 handwritten owners, built the selection, integration, app-less
contract, and Evaluation Default Policy port targets, and passed exact CTest
4/4. The unchanged
`ClassMngrNextPlatformApplicationServicesAcademicCalendarSchedulePreferencesPortTests`
target failed MSVC build with C1083 for a generated `.moc` include, despite
the file appearing after failure; its CTest was not run and the baseline cause
was not established. No full suite was run. F57 closes F55's helper-only
integration limitation and adds Gate 1 and Gate 2 evidence; both remain
Partial. Workspace boundary and audited `src/next` dependency isolation remain
Satisfied, and the Phase 2 exit gate remains Open. Sub Prep remains capped at
the current and following calendar years at most.

#### Progress update - 2026-09-26 (F58 typed Schedule Import matching identities, commit `9b9183818fc2163d625a8ffb088a492a4aa631a9`)

[`ScheduleImportMatchingProjection`](../../src/next/application/schedule_import_matching_projection.h)
now uses `Domain::TeacherId`/`Domain::ClassId` in its app-less matching
contract and represents the suggested class as optional. The legacy repository
adapter converts resolved numeric IDs back to the existing integer preview.
Parity retains ordering, ranks, confidence and explanations, intensive
fallback, empty teacher-key behavior, unfiltered teacher IDs including 0/-1,
and nonpositive class IDs excluded from match/suggestion but retained in
`initiallyAbsentClassIds`.

Executor focused CTest passed 1/1. Independent fresh x64 Ninja/MSVC
19.51.36257/Qt 6.12 verification in
`build/phase2-f58-schedule-matching-tester-20260926` validated 912 handwritten
source owners, built the matching projection and
`ClassMngrScheduleImportTests`, and passed exact CTest 2/2, including the
checked-in [`schedule_review.xlsx`](../../tests/fixtures/imports/schedule_review.xlsx)
path `previewsAndAppliesCheckedInWorkbookAgainstSeededDatabase`. No full suite
was run. At F58, the adapter's no-suggestion `-1` sentinel lacked a direct
production assertion; F63 closes this specific gap. The data model default
was `-1` and app-less contract tests already asserted absent optional
suggestions. Gate 1 and Gate 2 remain Partial; Workspace
boundary and audited `src/next` dependency isolation remain Satisfied; the
Phase 2 exit gate remains Open. Sub Prep remains capped at the current and
following calendar years at most.

#### Progress update - 2026-09-26 (F59 typed Schedule Import state-validation identities, commit `7769912e1a8ccec02ecc3ace2de11ff98c719327`)

`Application::validateScheduleImportState` now carries teacher and class
targets, snapshots, and class-to-teacher links as `Domain::TeacherId` and
`Domain::ClassId`. The `scheduleService()` repository adapter performs
action-aware conversion from legacy numeric IDs and sentinels. Compile-time
checks keep the identity categories distinct. Regression coverage preserves
state-validation ordering, skip target exactness and uniqueness, conflict
ordering for numeric IDs and synthetic classes, and
`rejectsSkippedClassWithMismatchedTeacherKey`.

Fresh independent MSVC 19.51/Ninja/Qt 6.12 verification passed exact CTest
2/2: `ClassMngrNextApplicationScheduleImportStateValidationTests` and
`ClassMngrScheduleImportTests`. The latter includes the checked-in
[`schedule_review.xlsx`](../../tests/fixtures/imports/schedule_review.xlsx)
apply fixture. No full suite was run. Direct sentinel coverage does not yet
exhaustively enumerate every action/sentinel combination.

#### Exit-gate status after F59

This cumulative audit applies the formal exit criteria above through F59. The
F59 focused tests were freshly and independently built; no full suite was run.

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior | Partial | Typed Schedule Import matching and state-validation identities add compile-time separation and deterministic ordering/conflict rules; broader Domain and Application behavior remains incomplete. |
| Baseline parity | Partial | F59 preserves the Schedule Import workbook apply path and covers mismatched teacher-key Skip rejection; wider baseline fixture parity remains incomplete, and the full suite has not run. |
| Workspace boundary | Satisfied | The `WorkspaceGateway::createWorkspace`/`WorkspaceCoordinator` acceptance and focused app-less tests remain satisfied; F59 does not change this boundary. |
| v2 dependency isolation | Satisfied in the audited v2 scope | The audited `src/next` sources remain free of direct `DataService`, `MainWindow`, `PageManager`, and widget-pointer dependencies; F59's contract remains app-less and the repository conversion stays at the legacy edge. |

Gate 1 and Gate 2 remain Partial; Workspace boundary and audited `src/next`
dependency isolation remain Satisfied. Phase 2 remains In Progress and its
exit gate remains Open. Sub Prep remains limited to the current and following
calendar years, 2026-2027.

#### Progress update - 2026-09-26 (F60 typed Schedule Import review-decision targets, commit `730955dd1feb24e5a46dd0bfef9f86b8ff619621`)

Schedule Import review-decision requests and issues now carry optional
`Domain::ClassId` targets. `PlanValidator` and the dialog feature edge convert
legacy integer targets, representing nonpositive values as absent. App-less
validation preserves required targets for UpdateExisting, target-free CreateNew,
and optional-target Skip; duplicate-claimant and issue details/order remain
stable. Dialog coverage preserves translated issue text and existing class-label
behavior. Compile-time checks establish typed fields and keep `ClassId` distinct
from `TeacherId`; explicit CreateNew-without-target and Skip-without-target
acceptance cases are covered.

Independent fresh x64 Ninja/MSVC 19.51/Qt 6.12 verification validated 912
handwritten source owners, built three targets in 321 steps, and passed exact
CTest 3/3: `ClassMngrNextApplicationScheduleImportReviewDecisionsTests`,
`ClassMngrScheduleImportDialogTests`, and `ClassMngrScheduleImportTests`. The
latter includes `previewsAndAppliesCheckedInWorkbookAgainstSeededDatabase` with
the checked-in [`schedule_review.xlsx`](../../tests/fixtures/imports/schedule_review.xlsx).
After adding Skip-without-target acceptance, the executor reran the app-less
suite successfully (1/1); `git diff --check` passed. No full suite was run.
The remaining coverage gap is the F59 action/sentinel matrix.
`reviewWarnsForDuplicateClassTargets` directly asserts that the warning
includes the resolved existing class label `E5 Athena`.

#### Exit-gate status after F60

This cumulative audit applies the formal exit criteria above through F60. The
focused F60 tests were freshly and independently built; no full suite was run.

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior | Partial | Typed Schedule Import matching, apply-state validation, and review-decision targets strengthen identity and decision contracts; broader Domain and Application behavior remains incomplete. |
| Baseline parity | Partial | F60 preserves review-decision details/order, dialog labels/translations, and the workbook apply path; wider baseline fixture parity remains incomplete and the full suite has not run. |
| Workspace boundary | Satisfied | The `WorkspaceGateway::createWorkspace`/`WorkspaceCoordinator` acceptance and focused app-less tests remain satisfied; F60 does not change this boundary. |
| v2 dependency isolation | Satisfied in the audited v2 scope | The audited `src/next` sources remain free of direct `DataService`, `MainWindow`, `PageManager`, and widget-pointer dependencies; F60 keeps legacy conversion at the feature edges. |

Gate 1 and Gate 2 remain Partial; Workspace boundary and audited `src/next`
dependency isolation remain Satisfied. Phase 2 remains In Progress and its
exit gate remains Open. Sub Prep remains limited to the current and following
calendar years, 2026-2027.

#### Progress update - 2026-09-26 (F61 Evaluation Default Selection read-failure coverage, commit `5e08c2aab8c4c326463e969445757fa90e25d79c`)

`failedCurrentEvaluationReadReturnsNoDefault()` adds temporary-database
integration coverage through production `ApplicationServices` and
`EvaluationDefaultSelection::forClass`; no production code changed. The test
creates a valid M2 class and ClassInfo with a 2026 saved schedule. With an
empty Fall evaluation, `CurrentOrPreviousTerm` selects Summer on 2026-09-07.
After the `speaking_evaluations` table is dropped, a direct Fall evaluation
service read reports an error and `forClass` returns no default.

Executor and independent fresh x64 Ninja/MSVC 19.51/Qt 6.12 configure/build
validated 912 handwritten source owners and each passed exact CTest 1/1:
`ClassMngrEvaluationDefaultSelectionIntegrationTests`. No full suite was run.

#### Exit-gate status after F61

This cumulative audit applies the formal exit criteria above through F61. The
focused F61 integration test was freshly and independently built; no full
suite was run.

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior | Partial | The typed Schedule Import matching, state-validation, and review-decision contracts remain covered; broader Domain and Application behavior remains incomplete. |
| Baseline parity | Partial | F61 verifies the production Evaluation Default Selection read-failure path, while wider baseline fixture parity remains incomplete and the full suite has not run. |
| Workspace boundary | Satisfied | The `WorkspaceGateway::createWorkspace`/`WorkspaceCoordinator` acceptance and focused app-less tests remain satisfied; F61 does not change this boundary. |
| v2 dependency isolation | Satisfied in the audited v2 scope | The audited `src/next` sources remain free of direct `DataService`, `MainWindow`, `PageManager`, and widget-pointer dependencies; F61 adds integration coverage without changing production dependencies. |

Gate 1 and Gate 2 remain Partial; Workspace boundary and audited `src/next`
dependency isolation remain Satisfied. Phase 2 remains In Progress and its
exit gate remains Open. F59's exhaustive action/sentinel matrix remains a
follow-up. Sub Prep remains limited to the current and following calendar years, 2026-2027.

#### Progress update - 2026-09-26 (F62 Schedule Import apply-boundary sentinel characterization, commit `691e56fbdcc536aaaf577602feeac25fc5b7227f`)

F62 adds data-driven production apply tests for the action/sentinel behavior that
can reach `ScheduleImportRepository::scheduleService()`. Reuse/UpdateRoom
teacher targets -1/0 are checked with matching snapshot rows present and
absent; Create/Skip teacher targets cover -1/0 and positive foreign IDs. Class
cases cover CreateNew sentinels, targetless Skip sentinels, exact and mismatched
positive Skip targets, and stale positive UpdateExisting targets. Rejected
applies compare persisted snapshots and confirm no changes. Positive CreateNew
and nonpositive UpdateExisting class targets are rejected upstream by the F60
PlanValidator.

The CreateNew class sentinel conversion itself is not isolated: F60
PlanValidator canonicalizes nonpositive IDs to absence and state validation does
not inspect CreateNew targets. Those test rows establish overall apply behavior,
not the repository conversion. The F62 matrix therefore adds bounded
characterization without claiming exhaustive adapter-branch coverage.

Fresh independent x64 Ninja/MSVC 19.51/Qt 6.12 verification built
`ClassMngrNextApplicationScheduleImportStateValidationTests` and
`ClassMngrScheduleImportTests` and passed the exact CTest filter 2/2. The latter
includes `previewsAndAppliesCheckedInWorkbookAgainstSeededDatabase` with the
checked-in [`schedule_review.xlsx`](../../tests/fixtures/imports/schedule_review.xlsx).
No full suite was run.

#### Exit-gate status after F62

This cumulative audit applies the formal exit criteria above through F62. The
focused F62 targets were freshly and independently built; no full suite was run.

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior | Partial | Typed Schedule Import matching, apply-state validation, and review-decision contracts remain covered; broader Domain and Application behavior remains incomplete. |
| Baseline parity | Partial | F62 adds apply-boundary action/sentinel characterization and preserves the checked workbook apply path; wider baseline fixture parity remains incomplete and the full suite has not run. |
| Workspace boundary | Satisfied | The `WorkspaceGateway::createWorkspace`/`WorkspaceCoordinator` acceptance and focused app-less tests remain satisfied; F62 does not change this boundary. |
| v2 dependency isolation | Satisfied in the audited v2 scope | The audited `src/next` sources remain free of direct `DataService`, `MainWindow`, `PageManager`, and widget-pointer dependencies; F62 changes tests only. |

Gate 1 and Gate 2 remain Partial; Workspace boundary and audited `src/next`
dependency isolation remain Satisfied. Phase 2 remains In Progress and its
exit gate remains Open. F62 narrows but does not exhaust the F59 action/sentinel
matrix, with the CreateNew class conversion limitation above. Next slice:
select F63. Sub Prep remains capped at 2026-2027, the current and following
calendar years.

#### Progress update - 2026-09-26 (F63 Schedule Import no-suggestion sentinel assertion, commit `bf4251eca530066ba65b00021f63779d185bd64e`)

`previewsAndAppliesCheckedInWorkbookAgainstSeededDatabase` now asserts that
the first fixture candidate, M3/Song, has no matching IDs, the legacy
`suggestedClassId == -1`, `exactMatch == false`, and confidence None. The
seeded class set (E4/Hercules and M2/Atlas) makes the no-match production
reachable; the existing positive-suggestion case remains. This closes the F58
production-adapter assertion gap for the no-suggestion `-1` sentinel. The
app-less projection already covered absence of a suggested class.

Fresh independent Windows x64 Debug Ninja/MSVC 19.51/Qt 6.12.0 verification
validated 912 handwritten source owners. Executor and Tester each passed the
exact `ClassMngrScheduleImportTests` and
`ClassMngrNextApplicationScheduleImportMatchingProjectionTests` CTests (2/2)
in separate fresh trees. No full suite was run.

#### Exit-gate status after F63

This cumulative audit applies the formal exit criteria above through F63. The
focused F63 targets were freshly and independently built; no full suite was
run.

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior | Partial | Typed Schedule Import matching, apply-state validation, and review-decision contracts remain covered; broader Domain and Application behavior remains incomplete. F63 adds no new Gate 1 behavior; app-less absence coverage was already present. |
| Baseline parity | Partial | F63 adds Gate 2 production-adapter evidence for the no-suggestion sentinel and retains the positive-suggestion case through the checked-in workbook; broader baseline fixture parity remains incomplete and the full suite has not run. |
| Workspace boundary | Satisfied | The `WorkspaceGateway::createWorkspace`/`WorkspaceCoordinator` acceptance and focused app-less tests remain satisfied; F63 does not change this boundary. |
| v2 dependency isolation | Satisfied in the audited v2 scope | The audited `src/next` sources remain free of direct `DataService`, `MainWindow`, `PageManager`, and widget-pointer dependencies; F63 changes tests only. |

Gate 1 remains Partial and unchanged by F63; Gate 2 remains Partial with new
adapter evidence. Workspace boundary and audited `src/next` dependency
isolation remain Satisfied. Phase 2 remains In Progress and its exit gate
remains Open. F59's action/sentinel coverage remains bounded; F62's
CreateNew class conversion limitation still applies. Next slice: select F64.
Sub Prep remains capped at 2026-2027, the current and following calendar years.

#### Progress update - 2026-09-26 (F64 StudentNamePair Domain value and roster score-import join, commit `559b4feaa8fd67c01cd2f4d0f3ddd7dc0f166de5`)

Qt-free `Domain::StudentNamePair` stores exact English and Korean UTF-16
components separately, rejects either empty half, and provides equality and
ordering. `RosterEditorWidget::importScores` trims both names at the Qt
boundary, skips incomplete pairs, and joins using the typed pair while
preserving last-write-wins behavior for duplicate imported score pairs.
Separate components remove delimiter ambiguity for invalid stored names
containing U+001F.

Domain coverage includes empty components; exact, case, internal-whitespace,
UTF-16, and ordering behavior; and separator-containing pairs. The real widget
fixture verifies one-sided outer trimming and later-grade persistence for a
duplicate pair in the second saved speaking-evaluation row; current validation
rejects new duplicates. Fresh independent Windows x64 Debug Ninja/MSVC 19.51/Qt 6.12 builds
each passed exact CTest 2/2: `ClassMngrNextDomainContractTests` and
`ClassMngrRosterEditorWidgetImportTests`. No full suite was run.

#### Exit-gate status after F64

This cumulative audit applies the formal exit criteria through F64. Executor
and independent Tester freshly built the two focused F64 targets; no full
suite was run.

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior | Partial | F64 adds a Qt-free, directly tested student name-pair value; broader Domain and Application behavior remains incomplete. |
| Baseline parity | Partial | F64 adds a real widget-import check for one-sided trim and persisted duplicate-row behavior; broader baseline parity remains incomplete and the full suite has not run. |
| Workspace boundary | Satisfied | The `WorkspaceGateway::createWorkspace`/`WorkspaceCoordinator` acceptance and focused app-less tests remain satisfied; F64 does not change this boundary. |
| v2 dependency isolation | Satisfied in the audited v2 scope | The audited `src/next` sources remain free of direct `DataService`, `MainWindow`, `PageManager`, and widget-pointer dependencies; F64's new Domain value remains Qt-free. |

Gate 1 and Gate 2 remain Partial; Workspace boundary and audited `src/next`
dependency isolation remain Satisfied. Phase 2 remains In Progress and its
exit gate remains Open. F59's action/sentinel coverage remains bounded; F62's
CreateNew class conversion limitation still applies. F65 selection is pending.
Sub Prep remains capped at the current and following calendar years,
2026-2027.

#### Progress update - 2026-09-26 (F65 legacy profile startup migration coverage, commit `a4fbffb91228ab1d783ac782ff572d49d3c28b65`)

[`FileControllerWorkspaceLifecycleTests`](../../tests/file_controller_workspace_lifecycle_tests.cpp)
now materializes the checked-in
[`legacy_startup.sql`](../../tests/fixtures/workspaces/legacy_startup.sql)
fixture as a `.db` and opens it through `FileController` into
`ApplicationServices`/`WorkspaceCoordinator`. The test checks the normalized
active path; migrated teacher, class, ClassInfo, and schedule values; schema
version 6; the service projection of the unassigned teacher as `-1` while the
database stores `NULL`; and the retained `.pre-schema-v4-backup` at schema
version 3.

Fresh Executor and independent Tester Windows x64 Debug Ninja/MSVC 19.51/Qt
6.12 builds each validated 913 handwritten source owners, built
`ClassMngrFileControllerWorkspaceLifecycleTests` and
`ClassMngrDatabaseSchemaManagerTests`, and passed their exact CTests 2/2. No
full suite was run.

#### Exit-gate status after F65

This cumulative audit applies the formal exit criteria through F65. The two
focused F65 targets were freshly and independently built; no full suite was
run.

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior | Partial | F64 adds a directly tested Qt-free student name-pair value; the broader Domain and Application behavior remains incomplete. F65 adds no Gate 1 behavior. |
| Baseline parity | Partial | F65 adds production-path legacy `.db` startup/open and schema-migration evidence, including migrated service values and retained pre-v4 backup. Wider fixture-backed baseline parity remains incomplete. |
| Workspace boundary | Satisfied | The formal `WorkspaceGateway::createWorkspace`/`WorkspaceCoordinator` acceptance and focused app-less coverage remain satisfied; F65 adds startup lifecycle integration evidence without changing that criterion. |
| v2 dependency isolation | Satisfied in the audited v2 scope | The audited `src/next` sources remain free of direct `DataService`, `MainWindow`, `PageManager`, and widget-pointer dependencies; F65 changes tests only. |

Gate 1 and Gate 2 remain Partial; Workspace boundary and audited `src/next`
dependency isolation remain Satisfied. Phase 2 remains In Progress and its
exit gate remains Open. F59's action/sentinel coverage remains bounded; F62's
CreateNew class conversion limitation still applies. F66 selection is pending.
Sub Prep remains capped at the current and following calendar years,
2026-2027.

#### Progress update - 2026-09-26 (F66 Sub Prep teacher display-name rule, commit `9afa17f47aadb7188916cc091e370f5d0bea98bb`)

Qt-free [`Domain::TeacherDisplayName`](../../src/next/domain/teacher_display_name.h)
owns the selected UTF-16 display value using the existing precedence: preferred
name, English, preferred romanization, then Korean. Both Sub Prep platform
adapters trim the source `QString` fields at the Qt boundary before calling the
Domain rule. The schedule-summary adapter retains its `N/A` empty fallback;
class details retains an empty value.

Domain tests cover each precedence branch, empty and copied values, and
non-ASCII text. Production adapter tests cover padded preferred-name input and
both all-empty fallbacks. Fresh Executor and independent Tester Windows x64
Debug Ninja/MSVC 19.51.36257/Qt 6.12 builds each validated 914 handwritten
owners and passed exact CTests 2/2:
`ClassMngrNextDomainContractTests` and
`ClassMngrNextPlatformApplicationServicesSubPrepPrintSourcePortTests`. No full
suite was run. F66 does not change Sub Prep range logic; the query remains
capped at the current and following calendar years, 2026-2027.

#### Exit-gate status after F66

This cumulative audit applies the formal exit criteria through F66. The two
focused F66 targets were freshly and independently built; no full suite was
run.

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior | Partial | F64 adds the typed student-name pair; F66 adds the Qt-free teacher display-name precedence rule with direct tests. Broader Domain and Application behavior remains incomplete. |
| Baseline parity | Partial | F65 adds production-path legacy profile startup/migration coverage; F66 adds Sub Prep adapter parity for boundary trimming and the two existing empty fallbacks. Wider fixture-backed baseline parity remains incomplete. |
| Workspace boundary | Satisfied | The formal `WorkspaceGateway::createWorkspace`/`WorkspaceCoordinator` acceptance and focused app-less coverage remain satisfied; F65 startup integration and F66 do not change that criterion. |
| v2 dependency isolation | Satisfied in the audited v2 scope | The audited `src/next` sources remain free of direct `DataService`, `MainWindow`, `PageManager`, and widget-pointer dependencies. F66 keeps the selection rule Qt-free and normalization in the platform adapters. |

Gate 1 and Gate 2 advance but remain Partial; Workspace boundary and audited
`src/next` dependency isolation remain Satisfied. Phase 2 remains In Progress
with its exit gate Open. F59's action/sentinel coverage remains bounded; F62's
CreateNew class conversion limitation remains distinct from F67's directly
tested application-state rule: `PlanValidator` rejects the targeted input
upstream, so the production regression suite does not reach that branch. Next
entry: select F68. Sub Prep remains capped at the current and following
calendar years, 2026-2027.

#### Progress update - 2026-09-26 (F67 targeted Schedule Import CreateNew state, commit `4081cc0fcbfb504766e8e10f98839f9eeffbf6ce`)

`Application::validateScheduleImportState` now returns the distinct
`CreateNewClassHasTarget` error when a CreateNew class decision carries a
target, and continues to accept a targetless CreateNew decision. The repository
maps the error to its user-facing text. Direct app-less tests cover target ID
42 rejected and no target accepted; the existing review-decision contract also
rejects the combination and its focused test was rerun.

Independent fresh Windows x64 Debug Ninja/MSVC 19.51/Qt 6.12 builds each
validated 914 handwritten source owners and passed the exact state-validation,
review-decision, and production Schedule Import CTests 3/3. `git diff --check`
was clean; no full suite was run. The production suite is a regression guard:
normal `PlanValidator` rejects this input upstream, so it does not exercise the
new state-validation branch end-to-end and adds no new Gate 2 evidence.

#### Exit-gate status after F67

This cumulative audit applies the formal exit criteria through F67. The three
focused targets were freshly and independently built; no full suite was run.

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior | Partial | F67 adds a directly tested application-state invariant for targeted CreateNew class decisions. Broader Domain and Application behavior remains incomplete. |
| Baseline parity | Partial | Existing F63/F65/F66 production-path parity evidence remains; F67's production suite revalidates behavior but does not reach the new branch, so it adds no new parity evidence. Wider baseline parity remains incomplete. |
| Workspace boundary | Satisfied | The formal `WorkspaceGateway::createWorkspace`/`WorkspaceCoordinator` acceptance and focused app-less coverage remain satisfied; F67 does not change the criterion. |
| v2 dependency isolation | Satisfied in the audited v2 scope | The audited `src/next` sources remain free of direct `DataService`, `MainWindow`, `PageManager`, and widget-pointer dependencies; F67 keeps the validation rule in the app-less Application contract. |

Gate 1 remains Partial with new app-less state-validation evidence. Gate 2
remains Partial with prior evidence revalidated but no F67 parity gain. Workspace
boundary and audited `src/next` dependency isolation remain Satisfied; Phase 2
remains In Progress and its exit gate Open. F62's upstream-unreachable
CreateNew sentinel-conversion observability limitation remains distinct from
F67's newly tested application-state rejection. Next entry: select F68. Sub
Prep remains capped at the current and following calendar years, 2026-2027.

#### Progress update - 2026-09-26 (F68 Qt-free calendar campus visibility policy, commit `3ee0b1c6`)

`Application::CalendarEventCampusVisibilityPolicy` now owns literal campus-token
matching without Qt dependencies. The Qt feature adapter retains QString
trimming, campus-code normalization, and one-to-one case-fold preprocessing,
then delegates matching. App-less policy tests and typed-summary/legacy adapter
regressions cover show-all and empty defaults, punctuation, S2/S20 and token
boundaries, Kelvin U+212A, and dotless i U+0131. These Unicode cases are targeted
regressions, not an exhaustive equivalence proof.

Fresh independent Windows x64 Debug Ninja/MSVC 19.51/Qt 6.12 builds each
validated 915 handwritten source owners and passed the exact
`ClassMngrNextApplicationCalendarEventTests`,
`ClassMngrCalendarEventCacheTests`, and `ClassMngrCalendarImportTests` CTests
3/3. `git diff --check` passed; no full suite was run.

#### Exit-gate status after F68

This cumulative audit applies the formal exit criteria through F68. The three
focused F68 targets were freshly and independently built; no full suite was
run.

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior | Partial | F68 adds directly tested Qt-free calendar campus-visibility behavior. Broader Domain and Application behavior remains incomplete. |
| Baseline parity | Partial | F68 adds typed-summary and legacy adapter regression checks for campus visibility; these are not checked-in baseline fixture parity. Wider fixture-backed parity remains incomplete. |
| Workspace boundary | Satisfied | The formal `WorkspaceGateway::createWorkspace`/`WorkspaceCoordinator` acceptance and focused app-less coverage remain satisfied; F68 does not change that criterion. |
| v2 dependency isolation | Satisfied in the audited v2 scope | The audited `src/next` sources remain free of direct `DataService`, `MainWindow`, `PageManager`, and widget-pointer dependencies; F68 keeps campus-token matching in a Qt-free Application policy. |

Gate 1 gains direct app-less behavior evidence and Gate 2 gains adapter
regression evidence; both remain Partial. Workspace boundary and audited
`src/next` dependency isolation remain Satisfied. Phase 2 remains In Progress
with its exit gate Open. F62's CreateNew sentinel-conversion observability
limitation and F67's upstream-unreachable production branch remain distinct
and unresolved. Next entry: select F69. Sub Prep remains capped at the current
and following calendar years, 2026-2027.

## Verified F69 Schedule Import state projection - commit `95aaefa4`

Qt-free [`Application::projectScheduleImportStateSchedules`](../../src/next/application/schedule_import_state_projection.h)
projects the final normal or intensive schedule rows using typed
`Domain::ClassId | ScheduleImportStateCandidateIndex` references. Each row also
carries an explicit `ReplaceRows` or `KeepExistingRows` persistence disposition.
The repository computes this projection once, passes the same rows to overlap
validation, and resolves candidate indexes to generated class IDs only after
inserting new classes. Intensive `UpdateExisting` keeps untouched existing rows
and their identities; normal import, `ReplaceWithNew`, and Skip retain replacement
and preservation behavior.

App-less tests cover typed references, source meeting order, overlap, intensive
modes, and skip dispositions. Production tests assert persisted-row parity for
checked-in [`schedule_review.xlsx`](../../tests/fixtures/imports/schedule_review.xlsx),
intensive replacement and untouched-row identity; the checked-in overlap fixture
and existing skipped exact-match, pre-write, and rollback cases remain covered.
Two independent fresh Windows x64 Debug Ninja/MSVC 19.51.36257/Qt 6.12 trees
validated 916 handwritten source owners and passed
`ClassMngrNextApplicationScheduleImportStateValidationTests` and
`ClassMngrScheduleImportTests` (2/2). `git diff --check` passed; no full suite was
run.

#### Exit-gate status after F69

This cumulative audit applies the formal exit criteria through F69. Both focused
targets were independently built and passed; no full suite was run.

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior | Partial | F69 directly tests the Qt-free final schedule-row projection, typed class/candidate references, source meeting order, overlap, intensive modes, and skip dispositions. Broader Domain and Application behavior remains incomplete. |
| Baseline parity | Partial | F69 adds persisted-row parity for the checked-in `schedule_review.xlsx` fixture. Separate production assertions cover intensive row replacement and untouched-row identity; checked-in `schedule_overlap_conflict.xlsx` verifies pre-write overlap rejection. Broader baseline fixture parity remains incomplete. |
| Workspace boundary | Satisfied | The formal `WorkspaceGateway::createWorkspace`/`WorkspaceCoordinator` acceptance and focused app-less coverage remain satisfied; F69 does not change that criterion. |
| v2 dependency isolation | Satisfied in the audited v2 scope | The audited `src/next` sources remain free of direct `DataService`, `MainWindow`, `PageManager`, and widget-pointer dependencies; F69 keeps the projection Qt-free and resolves generated IDs at the repository boundary. |

Gate 1 and Gate 2 remain Partial; Workspace boundary and audited `src/next`
dependency isolation remain Satisfied. Phase 2 remains In Progress with its exit
gate Open. F62's CreateNew sentinel-conversion observability limitation and
F67's upstream-unreachable production validation branch remain distinct. Next
entry: select F70 from the remaining Phase 2 gaps. Sub Prep remains limited to
the current and following calendar years, 2026-2027.

## Verified F70 Class Transfer preview matching policy - commit `2f3d414c`

The Qt-free Application matching policy
([`matchClassTransferCandidates`](../../src/next/application/class_transfer_matching_policy.h))
now owns preview matching over normalized source values and ordered
destination snapshots. The repository adapter retains Qt simplified/case-fold
normalization and converts typed teacher/class identities to the legacy integer
preview representation. App-less coverage checks teacher-match rules, course
and teacher identity, assigned-but-unloaded teacher handling versus unassigned
fallback, and destination order. Production coverage preserves normalized
matching, the checked-in success/conflict fixture preview IDs, and conflict
no-write behavior. Fixture parity is covered; exhaustive Unicode case-fold
equivalence is not claimed.

Independent and executor fresh Windows x64 Debug Ninja/MSVC 19.51.36257/Qt
6.12 builds each validated 917 handwritten source owners and passed
`ClassMngrNextApplicationClassTransferTests` and
`ClassMngrClassTransferTests` (2/2). No full suite was run.

#### Exit-gate status after F70

This cumulative audit applies the formal exit criteria through F70. Both focused
targets were independently built and passed; no full suite was run.

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior | Partial | F70 adds direct Qt-free Class Transfer matching rules and ordering tests. Broader Domain and Application behavior remains incomplete. |
| Baseline parity | Partial | F70 rechecks checked-in success/conflict preview IDs and conflict no-write behavior after moving matching into Application. Broader baseline fixture parity remains incomplete. |
| Workspace boundary | Satisfied | The formal `WorkspaceGateway::createWorkspace`/`WorkspaceCoordinator` acceptance and focused app-less coverage remain satisfied; F70 does not change that criterion. |
| v2 dependency isolation | Satisfied in the audited v2 scope | The audited `src/next` sources remain free of direct `DataService`, `MainWindow`, `PageManager`, and widget-pointer dependencies; the new matching policy is Qt-free and repository-owned normalization/presentation remain at the adapter. |

Gate 1 and Gate 2 remain Partial; Workspace boundary and audited `src/next`
dependency isolation remain Satisfied. Phase 2 remains In Progress with its exit
gate Open. F70 verifies existing fixture parity without claiming exhaustive
Unicode case-fold coverage. Next entry: select F71 from the remaining Phase 2
gaps. Sub Prep remains limited to the current and following calendar years,
2026-2027.

## Verified F71 typed Class Transfer review decision identities - commit `9b090cb5`

[`class_transfer_projection.h`](../../src/next/application/class_transfer_projection.h)
now uses typed `Domain::ClassId`/`TeacherId` match and issue identities, with
optional typed resolution targets. Both app-less validators preserve action,
membership, duplicate, and missing-target rules. UI and repository adapters retain
legacy integer APIs: positive values become typed IDs, exactly `-1` means absent,
and `0`/`-2` retain existing action-specific errors and invalid-action precedence.

App-less tests assert field categories, no implicit integer conversion, optional
targets, and typed issue identity. UI tests cover class and teacher `0`/`-2` targets
for both available actions. Repository tests cover class Create/Replace and teacher
Create/Keep cases with `0`/`-2`, invalid-action precedence, and no writes. The checked-in
`success_source.json` now exercises a nonempty exact teacher/class match and
dialog-selected Replace: it asserts the destination class ID and teacher profile
are retained, class details/schedule/roster are replaced, and old evaluation is
cleared. The existing Create fixture and checked-in conflict/no-write path remain.

Executor and independent Tester each used a fresh Windows x64 Debug Ninja/MSVC
19.51.36257/Qt 6.12 tree, validated 917 handwritten source owners, and passed
`ClassMngrNextApplicationClassTransferTests` and `ClassMngrClassTransferTests` (2/2).
`git diff --check` passed; no full suite was run.

#### Cumulative exit-gate status after F71

This audit applies the formal exit criteria through F71. Both focused targets passed
independently; no full suite was run.

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior | Partial | F71 adds typed Class Transfer match/issue identities and optional typed targets while preserving validator rules. Broader Domain and Application behavior remains incomplete. |
| Baseline parity | Partial | F71 exercises exact teacher/class matching and successful dialog-selected Replace through the checked-in success fixture, retaining the Create fixture and conflict/no-write coverage; sentinel and invalid-action matrices add adapter evidence. Broader baseline fixture parity remains incomplete. |
| Workspace boundary | Satisfied | The formal `WorkspaceGateway::createWorkspace`/`WorkspaceCoordinator` acceptance and focused app-less coverage remain satisfied; F71 does not change that criterion. |
| v2 dependency isolation | Satisfied in the audited v2 scope | Audited `src/next` sources remain free of direct `DataService`, `MainWindow`, `PageManager`, and widget-pointer dependencies. F71's review contract remains Qt-free; legacy integer conversion stays in UI/repository adapters. |

Gate 1 and Gate 2 remain Partial; Workspace boundary and audited `src/next`
dependency isolation remain Satisfied. Phase 2 remains In Progress and its exit
gate Open. Sub Prep remains limited to the current and following calendar years,
2026-2027.

## Verified F72 Class Transfer teacher replacement parity - commit `aa1af5fe`

The production test uses checked-in `success_source.json` and a seeded matching
teacher whose non-identity profile fields all differ from the fixture. It selects
Teacher ReplaceExisting through `ClassImportDialog` action/target item data and
applies with `DataService::importClasses`. Assertions verify the existing
`TeacherId` is retained, every teacher profile field receives the fixture value,
no duplicate teacher is created, and the imported class references the retained
teacher. F71 class replacement remains covered; Create and checked-in
`conflict_source.json` no-write paths are unchanged. This commit changes tests only.

Executor and independent Tester used separate fresh Windows x64 Debug Ninja/MSVC
19.51.36257/Qt 6.12 trees, each validated 917 handwritten source owners and
passed `ClassMngrNextApplicationClassTransferTests` and
`ClassMngrClassTransferTests` (2/2). `git diff --check` passed; no full suite was
run. F72 adds Gate 2 teacher-replacement fixture parity and no Gate 1 evidence;
both gates remain Partial. Workspace boundary and audited v2 dependency isolation
remain Satisfied. Phase 2 remains In Progress with its exit gate Open. Sub Prep
remains limited to the current and following calendar years, 2026-2027.

#### Cumulative exit-gate status after F72

This audit applies the formal exit criteria through F72. Both focused targets
passed independently; no full suite was run.

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior | Partial | F72 adds no app-less behavior evidence; F71's typed Class Transfer contract evidence remains. Broader Domain and Application behavior is incomplete. |
| Baseline parity | Partial | F72 adds checked-in fixture parity for Teacher ReplaceExisting, verifying retained identity, replaced profile fields, no duplicate teacher, and class linkage; existing class replacement, Create, and conflict/no-write paths remain. Broader baseline fixture parity remains incomplete. |
| Workspace boundary | Satisfied | The formal `WorkspaceGateway::createWorkspace`/`WorkspaceCoordinator` acceptance and focused app-less coverage remain satisfied; F72 changes no workspace behavior. |
| v2 dependency isolation | Satisfied in the audited v2 scope | Audited `src/next` sources remain free of direct `DataService`, `MainWindow`, `PageManager`, and widget-pointer dependencies; F72 changes tests only. |

Gate 1 remains Partial and unchanged; Gate 2 remains Partial with added teacher
replacement fixture evidence. Workspace boundary and audited v2 dependency
isolation remain Satisfied. Phase 2 remains In Progress with its exit gate Open.

## Verified F73 Class Transfer Skip action fixture parity - commit `60bbd015`

`tests/class_transfer_tests.cpp` extends
`permanentConflictFixturePresentsReviewAndRejectsScheduleCollision` using the
checked-in [`conflict_source.json`](../../tests/fixtures/transfers/conflict_source.json).
It retains default schedule-collision rejection/no-write coverage and exercises
class Skip with teacher ReplaceExisting selected through production dialog plan
metadata. The import succeeds with one skipped class and no created/replaced
class; teacher profile, class details and ClassInfo schedules, roster, speaking
evaluation, and record counts remain unchanged. The source commit changes only
this test file.

Executor and independent fresh Tester trees used Windows x64 Debug, Ninja 1.13.2,
MSVC 19.51.36257, and Qt 6.12.0; each validated 917 source owners and passed
`ClassMngrNextApplicationClassTransferTests` and
`ClassMngrClassTransferTests` (2/2). Tester also ran the fixture test directly.
`git diff --check` passed; no full suite was run. F73 adds Gate 2 fixture-backed
Skip-action evidence; Gate 1 is unchanged. Both gates remain Partial, workspace
boundary and audited v2 dependency isolation remain Satisfied, and Phase 2's
exit gate remains Open. Sub Prep remains capped at the current and following
calendar years, 2026-2027.

#### Cumulative exit-gate status after F73

This audit applies the formal exit criteria through F73. Both focused targets
passed independently; no full suite was run.

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior | Partial | F73 is test-only and adds no app-less behavior evidence; F71's typed Class Transfer contract evidence remains. Broader Domain and Application behavior is incomplete. |
| Baseline parity | Partial | F73 adds checked-in conflict-fixture coverage for dialog-selected class Skip alongside teacher ReplaceExisting metadata, while preserving collision rejection/no-write and asserting unchanged teacher/class-related records. F72 teacher replacement and F71 class replacement fixture parity remain; broader baseline fixture parity is incomplete. |
| Workspace boundary | Satisfied | The formal `WorkspaceGateway::createWorkspace`/`WorkspaceCoordinator` acceptance and focused app-less coverage remain satisfied; F73 changes no workspace behavior. |
| v2 dependency isolation | Satisfied in the audited v2 scope | Audited `src/next` sources remain free of direct `DataService`, `MainWindow`, `PageManager`, and widget-pointer dependencies; F73 changes tests only. |

Gate 1 remains Partial and unchanged; Gate 2 remains Partial with added Skip
action fixture evidence. Workspace boundary and audited v2 dependency isolation
remain Satisfied. Phase 2 remains In Progress with its exit gate Open. Sub Prep
remains capped at the current and following calendar years, 2026-2027.

## Verified F74 synthetic Intensive Schedule Import production flow - commit `3e0a8d64`

[`tests/schedule_import_tests.cpp`](../../tests/schedule_import_tests.cpp) adds
`previewsAndAppliesSyntheticIntensiveWorkbookAgainstSeededDatabase`, using the
source-readable authored worksheet
[`schedule_intensive_synthetic_worksheet.xml`](../../tests/fixtures/imports/schedule_intensive_synthetic_worksheet.xml).
The test parses it as Intensive, asserts fixed candidate and preview values,
explicitly applies UpdateExisting, and verifies persisted target Intensive rows
and class identity with no class creation. An untouched Intensive row retains
its ID and value, and regular schedule rows remain unchanged. This is synthetic
production-flow coverage only: the repository has no historical Intensive
workbook or legacy-output oracle, so historical baseline parity is not
established.

Executor and independent fresh Tester trees used Windows x64 Debug Ninja and
Qt 6.12 (Executor MSVC 14.51.36231; Tester MSVC 19.51.36257). CMake validated
917 handwritten source owners; `ClassMngrScheduleImportTests`,
`ClassMngrNextApplicationScheduleImportStateValidationTests`, and
`ClassMngrNextApplicationScheduleImportMatchingProjectionTests` passed 3/3
independently. `git diff --check` passed; no full suite was run. Protected
`cmake/sources.cmake` remains at SHA-256
`9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF`.
F74 adds synthetic production-flow evidence but no historical baseline parity;
Gate 1 is unchanged and Gate 2 remains Partial. Workspace boundary and audited
v2 dependency isolation remain Satisfied. Phase 2's exit gate remains Open.
Sub Prep remains capped at the current and following calendar years, 2026-2027.

#### Cumulative exit-gate status after F74

This audit applies the formal exit criteria through F74. The three focused
CTests passed independently; no full suite was run.

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior | Partial | F74 is test-only and adds no app-less behavior evidence; F71's typed Class Transfer contract evidence remains. Broader Domain and Application behavior is incomplete. |
| Baseline parity | Partial | F74 verifies a synthetic Intensive parse/preview/apply path and persisted-state invariants, but no historical Intensive workbook or legacy-output oracle exists; it does not establish historical baseline parity. F71-F73 Class Transfer fixture checks remain, and broader baseline parity is incomplete. |
| Workspace boundary | Satisfied | The formal `WorkspaceGateway::createWorkspace`/`WorkspaceCoordinator` acceptance and focused app-less coverage remain satisfied; F74 changes no workspace behavior. |
| v2 dependency isolation | Satisfied in the audited v2 scope | Audited `src/next` sources remain free of direct `DataService`, `MainWindow`, `PageManager`, and widget-pointer dependencies; F74 changes tests only. |

Gate 1 remains Partial and unchanged. Gate 2 remains Partial: F74 adds synthetic
production-flow evidence but no historical parity evidence. Workspace boundary
and audited v2 dependency isolation remain Satisfied. Phase 2 remains In
Progress with its exit gate Open. Next selected bounded slice: F75 implements a
Qt-free typed student-name-pair duplicate-grouping policy for roster and
speaking-evaluation duplicate validation, adapted at current callers while
preserving caller-side trimming, incomplete-row handling, and caller-specific
diagnostics. This selection is not implementation evidence; score-import
last-write-wins behavior remains distinct. Sub Prep remains capped at the
current and following calendar years, 2026-2027.
