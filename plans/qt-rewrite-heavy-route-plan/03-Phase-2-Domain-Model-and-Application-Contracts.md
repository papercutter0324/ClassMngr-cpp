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
- Current note: F115 is verified at commit `96c8b8a5`; direct ClassNotes saves
  now fail closed for a closed bound session. Gates 1 and 2 remain Partial;
  workspace create and audited direct `src/next` isolation remain Satisfied.
  Strict transitive isolation remains unresolved. Next is a paired read-only
  re-audit of active `src/next` to `ApplicationServices` calls and Gate 1/Gate 2
  evidence, separating source scans, bound-session method behavior, the
  retained `DataService*` edge, and documented outer-adapter Workspace and
  document-catalog routes. Exit remains Open. Historical production-workbook
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

## Latest verified progress (F115)

Commit `96c8b8a5812ceb8280fafd8fa3c9b6f99a8d409c` changes only
`src/app/services/feature_services.cpp` and
`tests/data_service_lifecycle_tests.cpp`. `ClassService::saveClassNotes()` now
fails closed when a non-null bound session has no repository, without using a
separately open DataService. Lifecycle assertions verify both notes fields
remain unchanged after rejection; DataService-only construction still updates
both fields. F114 already makes the active ClassNotes adapter reject a closed
bound session through `isAvailable()` before calling this method. F115 closes a
latent direct-service fallback, not an observed live port leak.

Independent fresh-snapshot Windows x64 Debug verification with CMake 4.4.2,
Ninja 1.13.2, MSVC 19.51.36257, and Qt 6.12.0 passed the exact registered
`ClassMngrDataServiceLifecycleTests`,
`ClassMngrNextPlatformApplicationServicesClassNotesSavePortTests`, and
`ClassMngrNextFeatureClassNotesPageTests` CTests (3/3, `--no-tests=error`).
`git diff --check` passed; no full suite ran.

### Cumulative exit-gate status after F115

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior (Gate 1) | Partial | F110 adds the composed Calendar visibility predicate; broader app-less behavior remains incomplete. |
| Baseline parity (Gate 2) | Partial | Bounded records include F82/F83 post-baseline Schedule Import inputs, F86/F88 baseline-era generated Schedule Import inputs, F91/F92/F94/F96/F98 Teacher Import flows and failure cases, and F100/F101/F103/F104/F106/F108/F109 Schedule Import rollback and Calendar repository transitions. They do not cover all validation, conflict, planning, or state-transition behavior; historical production-workbook provenance remains unverified. |
| Workspace boundary | Satisfied | The formal workspace-create criterion remains satisfied. |
| v2 dependency isolation | Partial transitive progress | The direct `src/next` source scan remains Satisfied; F111-F115 cover selected bound-session Settings, Calendar, Sub Prep, and ClassNotes methods. The retained `DataService*` compatibility edge and wider `ApplicationServices` usage remain unresolved pending re-audit; strict transitive isolation is unresolved. |

Phase 2 remains In Progress with its exit gate Open. Historical
production-workbook provenance remains a tracked risk, not a literal exit
criterion. Gate 1 and Gate 2 remain Partial. Keep four isolation scopes
separate: (a) the audited direct `src/next` scan finds no direct DataService,
MainWindow, PageManager, or widget-pointer references; (b) F111-F115 provide
method-level bound-session isolation for selected active Settings, Calendar,
Sub Prep, and ClassNotes paths; (c) the retained `DataService*` compatibility
edge and wider `ApplicationServices` use are not resolved by those findings;
(d) Workspace and document-catalog calls remain documented outer-adapter
routes. Strict transitive isolation remains unresolved. Sub Prep is bounded to
January 1 of the reference date's year through December 31 of the following
year, at most; 2026-2027 is illustrative.

### Next bounded work (read-only re-audit)

Pair a read-only audit of active `src/next` to `ApplicationServices` service
calls with a refresh of Gate 1 and Gate 2 evidence. Preserve the four scopes
above: direct source isolation, method-level bound-session runtime isolation,
the retained legacy `DataService*` edge, and documented outer-adapter
Workspace/document-catalog routes. Do not claim strict transitive isolation
until the re-audit resolves it; keep both gates Partial and Phase 2 exit Open.
