# Phase 2 — Domain Model and Application Contracts

Read [00-Start-Here.md](00-Start-Here.md) first for the overall plan, workflow, and phase sequence.

## Mandatory route for new and resumed work

- Use the Heavy route for every task, slice, follow-up, and reopened gate in this phase. `Default route: Heavy` is a binding instruction.
- At each start or resume, explicitly select Heavy and reread this phase plan and [00-Start-Here.md](00-Start-Here.md). Follow the active `AGENTS.md` Heavy-route instructions and `~/.codex/codex_workflow/heavy_route.md`, including their delegation, verification, and deployment-state requirements.
- A new session, handoff, or context reset does not change the route. Do not continue under Light or Medium. If a required Heavy-route step blocks progress, report the blocker before implementation instead of silently switching routes.

## Status

- Status: In progress
- Default route: Heavy
- Depends on: Phase 1
- Blocks: Persistence, bootstrap, shared UI, and feature migration
- Owner: Unassigned
- Last updated: 2026-09-29
- Historical progress log: [03-Phase-2-Progress-Log.md](03-Phase-2-Progress-Log.md)
- Exit gate: Open
- Current note: F133 verifies the Qt-free typed-ID Classes navigation
  snapshot and session-backed read adapter. F134's ScheduleBuilder source-query
  projection is in progress; its ordering is an inferred sequencing choice,
  not a Phase 2 mandate. F120 active-v2 DataService
  isolation and formal workspace-create acceptance remain Satisfied. Gates 1
  and 2 remain Partial. Historical workbook provenance remains a tracked
  risk, not a literal exit criterion. Sub Prep remains January 1 of the
  reference date's year through December 31 of the following year at most;
  2026-2027 is illustrative.

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

## Latest verified progress (F133)

Source/test commit `b111b799` (`Phase2 - integrate classes navigation snapshot`)
adds a Qt-free typed-ID snapshot and an active-session Platform adapter. The
adapter batches class metadata and teacher names in one join, then reads regular
and intensive times with two ordered statements. `ClassesPage` keeps its
existing tab, filter, selection, and refresh behavior. Missing rows or a read
failure leave the requested class names present with blank metadata.

`ClassMngr` and the four focused CTest suites passed 4/4: application snapshot,
platform read port, ClassesPage, and navigation model. Independent Ninja/MSVC
verification was used after VS ZERO_CHECK encountered FileTracker access
denied. `git diff --check` passed. No full suite or baseline comparison ran.

Gate 1 and Gate 2 remain Partial. F120 active-v2 DataService isolation and the
formal workspace-create boundary remain Satisfied. Phase 2 remains In Progress
with its exit gate Open. Historical production-workbook provenance remains a
tracked risk, not a literal exit criterion. Sub Prep remains bounded to January
1 of the reference date's year through December 31 of the following year, at
most; 2026-2027 is illustrative.

### Cumulative exit-gate status after F133

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior (Gate 1) | Partial | Coverage includes workspace lifecycle; Class Transfer schedule-candidate validation (F79); Teacher Import (F80/F81/F85/F87/F89/F90); Schedule Import match-key typing (F84); Calendar policies/use cases (F93/F95/F97/F99/F101-F105/F107/F110); teacher-profile editing (F117/F123); co-teacher assignment (F124); class-notes save (F125); and the Classes navigation snapshot (F133). Broader class-detail/schedule/roster/evaluation editing, backup/recovery, and legacy database import remain planned. |
| Baseline parity (Gate 2) | Partial | Bounded records include F82/F83 post-baseline Schedule Import inputs, F86/F88 baseline-era generated Schedule Import inputs, F91/F92/F94/F96/F98 Teacher Import flows and failure cases, F100/F101/F103/F104/F106/F108/F109 Schedule Import rollback and Calendar repository transitions, F118 common-input Class Transfer conflict behavior, F121 common-input successful replacement state, and F122 hand-authored seeded Schedule Import Skip state parity. They do not cover all validation, conflict, planning, or state-transition behavior; historical production-workbook provenance remains unverified. |
| Workspace boundary | Satisfied | The formal workspace-create criterion remains satisfied. |
| v2 dependency isolation | Satisfied for active `src/next` call paths | F120 removes the Workspace operation edge to `DataService`; the source audit confirms all seven operations route to `DatabaseSession` or the file helper. Legacy facade construction/access remains available. |

Phase 2 remains In Progress with its exit gate Open. Gate 1 and Gate 2 remain
Partial; historical production-workbook provenance remains a tracked risk, not
a literal exit criterion. Sub Prep remains bounded to January 1 of the
reference date's year through December 31 of the following year, at most;
2026-2027 is illustrative.

### F134 implementation in progress: ScheduleBuilder source-query projection

Read the ScheduleBuilder source query into a compact Qt-free input snapshot,
integrated through the session-backed Platform adapter and builder/widget.
This sequence is an inference: feature order places Classes before
Schedule/Imports, while Phase 2 requires both a compact schedule projection and
an import review session without ranking them. The ScheduleBuilder read
projection is the narrower vertical slice. Existing repository code already
batches ordered SQL, so this slice does not claim query-count or database
optimization. Preserve raw ordered day/start/end strings and current parser
quirks. Keep slot state, testing assignments, and the final
ScheduleViewProjection migration separate. Schedule Import review is a broader
later candidate.
