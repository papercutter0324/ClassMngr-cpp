# Phase 2 — Domain Model and Application Contracts

## Status

- Status: In progress
- Default route: Heavy
- Depends on: Phase 1
- Blocks: Persistence, bootstrap, shared UI, and feature migration
- Owner: Unassigned
- Last updated: 2026-09-27
- Historical progress log: [03-Phase-2-Progress-Log.md](03-Phase-2-Progress-Log.md)
- Exit gate: Open
- Current note: F96 adds baseline/current synthetic Teacher Import database
  rollback parity at the latest-date write stage. F97 is selected to move
  Calendar repeat-series suffix planning into a Qt-free Application contract.
  Gate 1 and Gate 2 remain Partial; the formal workspace criterion and audited
  `src/next` isolation are Satisfied. Historical production-workbook evidence
  and broader migrations remain open. Sub Prep remains capped at 2026-2027.

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

## Verified F96 Teacher Import database rollback parity

F96 selection compared the fixed synthetic rollback scenario in current test
`rollsBackAllTeacherWritesWhenLatestDateSaveFails`; the baseline test lacks
this case. The plan contains one Korean teacher, one Native English teacher,
and one GS Team member, with failure injected during
`teacher_import/latest_source_date` insertion.

F96 compared baseline `48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` with
current `7865963815efa256b797b85af27b9a33107e8f70` using the same external
harness, which compiled each tree's own Teacher Import repository, schema
manager, transaction, and SQL utilities. Independent fresh Release/Ninja builds
and runs passed, and the archived source trees were verified against Git.

Both runs failed at `latest_date_write` and left `teachers`,
`native_english_teachers`, `gs_team`, and the latest-date setting empty. Their
normalized JSON outputs matched at SHA-256
`b53d70bf33fa0e20451a2582eb8e1a6f904e5907196c6fad4696543b95b230f3`. This is
synthetic database rollback evidence only; it does not establish workbook,
historical production-data, or other failure-stage parity.

### Cumulative exit-gate status after F96

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior (Gate 1) | Partial | F95 adds Qt-free Calendar repeat occurrence planning; broader application behavior remains incomplete. |
| Baseline parity (Gate 2) | Partial | F92 and F94 cover generated workbook paths; F96 adds one synthetic database rollback case. Historical production-workbook evidence and broader parity remain open. |
| Workspace boundary | Satisfied | The formal WorkspaceGateway/WorkspaceCoordinator criterion remains satisfied; the broader FileController integration gap remains outside that criterion. |
| v2 dependency isolation | Satisfied in the audited v2 scope | Audited `src/next` sources remain free of direct DataService, MainWindow, PageManager, and widget-pointer dependencies. |

Phase 2 remains In Progress with its exit gate Open. Historical
production-workbook evidence, other rollback failure stages, broader Calendar
UI/contracts, generic settings persistence, remaining feature-service
migrations, document-service migration, invalid-UTF-8 coverage, and live
MainWindow projection-failure/retranslation integration remain open. Sub Prep
remains capped at 2026-2027.

### Next selected bounded slice (F97)

Move repeat-series suffix-edit transformation from
`src/next/platform/application_services_calendar_event_series_edit_port.h`
into a Qt-free Application planner invoked after the platform adapter query.
Reuse `CalendarEventSeriesEditRequest`; pass ordered value-only occurrence
inputs with event IDs and canonical start dates; return planned updates.
Preserve inclusive cutoff and query order, common date offset from selected to
edited start, common requested duration, IDs, title, type, time status, all-day
and time fields, trimmed series ID propagation, and empty-result success.
Invalid source or shifted dates map to the existing Technical/save failure
behavior.

Add app-less tests for suffix inclusion/exclusion, offsets and duration, field,
ID and order propagation, empty input, and date overflow. Retain platform
integration tests and build the Application target, platform target, and
`ClassMngr`. F97 advances Gate 1 but Gate 1 and Gate 2 remain Partial. This is
selected work, not implementation evidence. Phase 2 exit remains Open. Sub
Prep remains capped at 2026-2027.
