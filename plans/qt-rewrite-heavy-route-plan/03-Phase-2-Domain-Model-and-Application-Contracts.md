# Phase 2 — Domain Model and Application Contracts

## Status

- Status: In progress
- Default route: Heavy
- Depends on: Phase 1
- Blocks: Persistence, bootstrap, shared UI, and feature migration
- Owner: Unassigned
- Last updated: 2026-09-28
- Historical progress log: [03-Phase-2-Progress-Log.md](03-Phase-2-Progress-Log.md)
- Exit gate: Open
- Current note: F111 is verified at commit `3152ce36`; non-null-session
  `SettingsService` operations no longer fall back to DataService. Transitive
  isolation has advanced only for this service; `isAvailable()` can still be
  true when a separate DataService is open. F112 is selected for
  session-authoritative CalendarService content reads. Gates 1 and 2 remain
  Partial; workspace create and audited direct `src/next` isolation are
  Satisfied. Strict transitive ApplicationServices-to-DataService isolation
  remains unresolved. Exit remains Open. Historical production-workbook
  provenance is a tracked risk, not a literal exit criterion.
  Sub Prep remains January 1 of the reference date's year through December 31
  of the following year at most; 2026-2027 is illustrative.

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

## Latest verified progress (F111)

Commit `3152ce36` (`Phase2 - isolate session-bound settings reads`) changes
only `src/app/services/feature_services.cpp` and
`tests/data_service_lifecycle_tests.cpp`. An independent fresh archive of
base `5653bf8c032ed553fc60324444428135e5f1b8dc` overlaid only those two paths.

`ClassMngrDataServiceLifecycleTests` and
`ClassMngrNextPlatformApplicationServicesCurrentCampusPreferencesPortTests`
passed focused CTest 2/2 on Windows x64 Debug with CMake 4.4.2, Ninja 1.13.2,
MSVC 19.51, and Qt 6.12. A non-null bound `SettingsService` session now fails
`load`, `save`, and `saveAll` without a repository instead of falling back to
DataService; the sessionless DataService-only path retains read/write behavior.
`loadOrDefault` still routes through `load`. The closed-session/separate-open-
DataService test confirms no content mutation, and the normal preferences
adapter test passed.

`SettingsService::isAvailable()` can still return true when the separate
DataService is open, even though bound-session operations fail; availability
does not establish read isolation. This slice covers SettingsService only;
Calendar, Teacher, Class, Schedule, Roster, and Sub Prep fallbacks remain. No
full suite or baseline parity run.

### Cumulative exit-gate status after F111

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior (Gate 1) | Partial | F110 adds the composed Calendar visibility predicate; broader app-less behavior remains incomplete. |
| Baseline parity (Gate 2) | Partial | F109 adds synthetic repeat-series creation state-transition parity; broader parity remains open. |
| Workspace boundary | Satisfied | The formal workspace-create criterion remains satisfied. |
| v2 dependency isolation | Partial transitive progress | Direct audited `src/next` isolation remains Satisfied; F111 isolates SettingsService operations for non-null sessions, but other service fallbacks and the strict transitive edge remain unresolved. |

Phase 2 remains In Progress with its exit gate Open. Historical
production-workbook provenance remains a tracked risk, not a literal exit
criterion. Broader Calendar UI/contracts, CalendarService isolation,
remaining feature-service migrations, document-service migration, and live
MainWindow projection-failure/retranslation integration remain open. Sub Prep
is bounded to January 1 of the reference date's year through December 31 of
the following year, at most; 2026-2027 is illustrative.

### Next selected bounded slice (F112)

In `src/app/services/feature_services.cpp`, make these six `CalendarService`
content reads authoritative to a non-null session: `eventsForDate`,
`eventsInRange`, `eventDateIntervalsInRange`, `upcomingEvents`, `event`, and
`repeatSeriesFromDate`. If that session lacks the calendar repository, fail or
report unavailable without reading DataService. Preserve all six DataService-
only behaviors for sessionless legacy construction; leave writes and deletes
unchanged.

Add one lifecycle regression with a closed bound `DatabaseSession` and a
separately open, seeded DataService: none of the six dual-bound reads may
expose its event data, while legacy-only `CalendarService` reads still do.
Retain the focused normal ApplicationServices Calendar adapter CTest. This
slice does not close other service-family fallbacks or establish global
isolation.
