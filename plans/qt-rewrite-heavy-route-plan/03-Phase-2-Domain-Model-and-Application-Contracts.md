# Phase 2 — Domain Model and Application Contracts

Read [00-Start-Here.md](00-Start-Here.md) first for the overall plan, workflow, and phase sequence.

## Status

- Status: In progress
- Default route: Heavy
- Depends on: Phase 1
- Blocks: Persistence, bootstrap, shared UI, and feature migration
- Owner: Unassigned
- Last updated: 2026-09-28
- Historical progress log: [03-Phase-2-Progress-Log.md](03-Phase-2-Progress-Log.md)
- Exit gate: Open
- Current note: F120 is independently verified after the same-file path-alias
  repair at `09201aa5`; all seven Workspace operations route through
  `ApplicationServices` to `DatabaseSession` or a file helper, and active
  `src/next` call paths no longer depend on `DataService`. Workspace create and
  active v2 DataService isolation are Satisfied; Gates 1 and 2 remain Partial.
  F121 is a selected post-baseline Class Transfer common-input parity slice; its
  implementation and verification are pending. Historical workbook provenance
  remains a tracked risk, not a literal exit criterion. Sub Prep remains
  January 1 of the reference date's year through December 31 of the following
  year at most; 2026-2027 is illustrative.

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

## Latest verified progress (F120)

F120's implementation commit `b1288b96166a3beaa5885555e3fe05ab83d59107`
removed the Workspace operation path through `DataService`. Exact-commit
independent verification passed its focused build and three required CTests,
then found a same-file Windows path-alias data-loss edge in file-copy handling.
The repair commit `09201aa5282973044a83b1471c6c8f676a7cb716` protects that
copy case. On a fresh archive of the repair, `ClassMngr` and all three focused
targets built; `ClassMngrDataServiceLifecycleTests`,
`ClassMngrNextPlatformApplicationServicesWorkspacePortTests`, and
`ClassMngrFileControllerWorkspaceLifecycleTests` passed 3/3. A case-variant
probe reported `operationSucceeded=1`, `sourceExists=1`, and
`contentPreserved=1`. Verification used CMake 4.4.2, Ninja 1.13.2, MSVC
19.51.36257, and Qt 6.12.0. Missing optional Vulkan headers and one object-path
warning applied to an unselected target. No full suite ran.

The repair delta is `src/data/database/database_file_operations.cpp` (SHA-256
`57B07272C59D4BD7B09B40D78EE1E69112497AB92DE6A2E38E2B5F4C9F9E0DF6`, blob
`7f20ea653900f661e68e625ec8a9951b89e7c906`) and
`tests/data_service_lifecycle_tests.cpp` (SHA-256
`0A8674C31F015BF0AD10D58B827152FB427AA6E7EADA642F0EA93D268008F182`, blob
`825e530379931eccb0efd68c32738484c9d2b0d0`).

The source audit confirms the seven Workspace operations route from
`ApplicationServices` to `DatabaseSession` or the file helper; they do not call
`m_dataService`, which remains for compatibility construction/access. Active
`src/next` has no direct `DataService` references. Workspace transitive
DataService isolation is Satisfied alongside the formal workspace-create
boundary. Gate 1 and Gate 2 remain Partial. F118 is common-input evidence on a
checked-in post-baseline fixture, not historical production-workbook parity.

### Cumulative exit-gate status after F120

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior (Gate 1) | Partial | Coverage includes workspace lifecycle; Class Transfer schedule-candidate validation (F79); Teacher Import (F80/F81/F85/F87/F89/F90); Schedule Import match-key typing (F84); Calendar policies/use cases (F93/F95/F97/F99/F101-F105/F107/F110); and Teacher profile edit (F117). Broader class/schedule/roster/evaluation editing, backup/recovery, and legacy database import remain planned. |
| Baseline parity (Gate 2) | Partial | Bounded records include F82/F83 post-baseline Schedule Import inputs, F86/F88 baseline-era generated Schedule Import inputs, F91/F92/F94/F96/F98 Teacher Import flows and failure cases, F100/F101/F103/F104/F106/F108/F109 Schedule Import rollback and Calendar repository transitions, and F118 common-input Class Transfer conflict behavior. They do not cover all validation, conflict, planning, or state-transition behavior; historical production-workbook provenance remains unverified. |
| Workspace boundary | Satisfied | The formal workspace-create criterion remains satisfied. |
| v2 dependency isolation | Satisfied for active `src/next` call paths | F120 removes the Workspace operation edge to `DataService`; the source audit confirms all seven operations route to `DatabaseSession` or the file helper. Legacy facade construction/access remains available. |

Phase 2 remains In Progress with its exit gate Open. Gate 1 and Gate 2 remain
Partial; historical production-workbook provenance remains a tracked risk, not
a literal exit criterion. Sub Prep remains bounded to January 1 of the
reference date's year through December 31 of the following year, at most;
2026-2027 is illustrative.

### Next selected slice (F121)

Add a test-only successful Class Transfer replacement common-input comparison
using the same `tests/fixtures/transfers/success_source.json` bytes in clean
current and baseline trees. Pin the fixture SHA-256
`A40CB4079865EB5C48800208A3648360C08B0CEC2F3ED1E3FA383E91BD4050E8` and
baseline commit `48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99`. The deterministic
seeded test must compare normalized preview, review, and plan; replacement
identity/result; and persisted class details, schedule, roster, and evaluation
(including `sqlite_sequence` where supported). Run focused
`ClassMngrClassTransferTests` in both trees. Label the result as post-baseline
common-input evidence, not historical production-workbook parity. F121
implementation is underway and baseline/current verification is pending; Gate 2
remains Partial.
