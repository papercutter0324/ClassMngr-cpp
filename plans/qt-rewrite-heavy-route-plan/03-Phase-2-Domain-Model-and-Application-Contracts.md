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
- Current note: F110 is verified at commit `2656ef9c`; F111 is selected for a
  bounded SettingsService isolation slice. Gates 1 and 2 remain Partial;
  workspace create and audited direct `src/next` isolation are Satisfied.
  Strict transitive ApplicationServices-to-DataService read isolation remains
  unresolved. Exit remains Open. Historical production-workbook provenance is
  a tracked risk, not a literal exit criterion.
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

## Latest verified progress (F110)

Commit `2656ef9c` (`Phase2 - add Calendar event visibility policy`) changes
exactly five paths: new
`src/next/application/calendar_event_visibility_policy.h`,
`cmake/next.cmake`, `src/features/calendar/ui/calendar_event_model.cpp`,
`src/features/calendar/ui/calendar_page_upcoming_events.cpp`, and
`tests/next_application_calendar_event_tests.cpp`. An independent fresh
archive of base `d59f0be4` overlaid only those five files.

The build compiled `ClassMngrNextApplicationCalendarEventTests`,
`ClassMngrCalendarEventCacheTests`, and `ClassMngr`; focused CTest passed 2/2
on Windows x64 Debug with Ninja 1.13.2, MSVC 19.51.36257, Qt 6.12.0, and CMake
4.4.2. A Qt-free app-less 12-case composition matrix pins start-term hide
precedence (only aliases with effective type `Other` hide, and show-all does
not bypass that rule), lazy campus checks, show-all bypassing campus filtering
only, visibility with missing/unmatched metadata or no recognized campus
token, hiding non-current-only tokens, and allowing current tokens including
mixed current/other tokens. Both callers preserve input order; the upcoming-
events active-type gate remains first, and Qt title/campus normalization
remains at the feature boundary. The page translation unit compiled, but no
dedicated CalendarPage visibility CTest asserted runtime page output. No full
suite ran.

### Cumulative exit-gate status after F110

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior (Gate 1) | Partial | F110 adds a composed Calendar visibility predicate; broader app-less behavior remains incomplete. |
| Baseline parity (Gate 2) | Partial | F109 adds synthetic repeat-series creation state-transition parity; broader parity remains open. |
| Workspace boundary | Satisfied | The formal workspace-create criterion remains satisfied. |
| v2 dependency isolation | Satisfied for audited direct `src/next` references | Strict transitive ApplicationServices-to-DataService read isolation remains unresolved. |

Phase 2 remains In Progress with its exit gate Open. Historical
production-workbook provenance remains a tracked risk, not a literal exit
criterion. Broader Calendar UI/contracts, generic settings persistence,
remaining feature-service migrations, document-service migration, and live
MainWindow projection-failure/retranslation integration remain open. Sub Prep
is bounded to January 1 of the reference date's year through December 31 of
the following year, at most; 2026-2027 is illustrative.

### Next selected bounded slice (F111)

Make a non-null-session `SettingsService` authoritative for `load`, `save`, and
`saveAll`, with no DataService fallback, while preserving the legacy
DataService-only constructor and behavior. Cover a non-null closed session
alongside a separately open DataService (no fallback reads or writes), legacy
DataService-only reads/writes, normal open `ApplicationServices` session
preferences, and defaults/errors. Other feature-service fallback families
remain open. This is partial strict-isolation progress, not a global audit
pass.
