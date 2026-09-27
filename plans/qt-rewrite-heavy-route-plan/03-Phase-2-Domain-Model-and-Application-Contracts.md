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
- Current note: F97 adds a Qt-free Calendar repeat-series edit planner and
  routes the platform port through it. F98 is selected for source-generated
  malformed-UTF-8 Teacher Import workbook parity. Gate 1 and Gate 2 remain
  Partial; the formal workspace criterion and audited `src/next` isolation are
  Satisfied. Historical production-workbook evidence remains open. Sub Prep
  remains capped at 2026-2027.

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

## Verified F97 Calendar repeat-series edit planner - commit `36ebb09a`

Commit `36ebb09a960fa82f633701ec83fba35bcd7f3599` adds
`src/next/application/calendar_event_series_edit_plan.h`, registers it in
`cmake/next.cmake`, and routes
`src/next/platform/application_services_calendar_event_series_edit_port.h`
through the Qt-free planner. The planner preserves query order and event IDs,
common start-date offset and requested duration, request fields, trimmed series
ID, empty-input success, and Technical failures for invalid source or shifted
dates.

Independent fresh Ninja builds produced
`ClassMngrNextApplicationCalendarEventTests`,
`ClassMngrNextPlatformApplicationServicesCalendarEventPortTests`, and
`ClassMngr`. Both focused CTests passed; an independent platform recheck also
passed for empty suffix and no-save failure cases. `git diff --check` passed;
no full suite was run.

### Cumulative exit-gate status after F97

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior (Gate 1) | Partial | F95 adds repeat occurrence planning and F97 adds repeat-series edit planning; broader application behavior remains incomplete. |
| Baseline parity (Gate 2) | Partial | F92 and F94 compare source-generated workbook paths; F96 adds one synthetic database rollback case. Historical production-workbook evidence and broader parity remain open; F97 adds no parity evidence. |
| Workspace boundary | Satisfied | The formal WorkspaceGateway/WorkspaceCoordinator criterion remains satisfied; the broader FileController integration gap remains outside that criterion. |
| v2 dependency isolation | Satisfied in the audited v2 scope | Audited `src/next` sources remain free of direct DataService, MainWindow, PageManager, and widget-pointer dependencies. |

Phase 2 remains In Progress with its exit gate Open. Historical
production-workbook evidence, other rollback failure stages, broader Calendar
UI/contracts, generic settings persistence, remaining feature-service
migrations, document-service migration, and live MainWindow
projection-failure/retranslation integration remain open. Sub Prep remains
capped at 2026-2027.

### Next selected bounded slice (F98)

Add source-generated malformed-UTF-8 Teacher Import workbook parity using
identical bytes from the shared `testWorkbookData`/`storedZip` helpers. In the
`xl/sharedStrings.xml` member, replace the `M` in the second shared-string
marker `<t>M1</t>` with raw `0xFF` at member offset 223, then rebuild the ZIP
CRC. First characterize each revision's behavior, then compare normalized
status, template, source date, discovered sections, counts, and ordered
semantic preview; omit localized names and diagnostics. Add a pinned current
regression and a narrow baseline/current harness that compiles each revision's
own reader, validator, registry, template, and name helper. Use baseline
`48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` and current
`36ebb09a960fa82f633701ec83fba35bcd7f3599`.

This is source-generated malformed-workbook parity only; historical
production-workbook evidence remains open. F98 is selected to advance Gate 2;
Gate 2 and Gate 1 remain Partial. This is selected work, not implementation
evidence.
Phase 2 exit remains Open. Sub Prep remains capped at 2026-2027.
