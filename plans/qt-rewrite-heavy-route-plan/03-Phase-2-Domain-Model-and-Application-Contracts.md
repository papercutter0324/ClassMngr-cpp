# Phase 2 — Domain Model and Application Contracts

## Status

- Status: In progress
- Default route: Heavy
- Depends on: Phase 1
- Blocks: Persistence, bootstrap, shared UI, and feature migration
- Owner: Unassigned
- Last updated: 2026-09-27
- Historical progress log: [03-Phase-2-Progress-Log.md](03-Phase-2-Progress-Log.md)
- Current note: Typed Domain/Application contracts now cover workspace lifecycle,
  selection state, import/report jobs, document sessions/catalogs, calendar
  projections and mutations, and feature preference boundaries. The Application
  layer remains Qt-free. Broader calendar UI, generic settings persistence,
  remaining feature-service migrations, and document-service migration remain
  open. Invalid-UTF-8 boundary coverage and live UI integration are non-blocking
  gaps; MainWindow projection-failure/retranslation has no integration test.
  Sub Prep remains capped at 2026-2027.

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

## Verified F88 Schedule Import intensive generated-baseline differential - commit 4442726b

`tests/schedule_import_tests.cpp` adds an intensive Schedule Import baseline
comparison using `singleSheetWorkbookData()` and inline worksheet data that
are identical to baseline `48fc5c5c`. The source-generated workbook is 2,359
bytes with SHA-256
`228fc2ce924f2fd4ee340500c92178386868bc83231b076b74e081dd628b93b4`.
The legacy snapshot parser, repository, model, rules, schema manager, helper,
and worksheet were verified against baseline. Legacy and current paths used
the same pinned bytes and seeded database.

Both parsed and persisted intensive-slot transcripts contain 65 rows (62
empty, 2 essay, 1 lunch) and match at SHA-256
`7fdba13a48788441556050e17b6d0b5ca52b2b9df5a6d0c357815a6458f4a5cc`. The
test pins parse and preview matching/suggestion behavior, apply counters,
intensive schedule state, and preservation of regular hours and unrelated
class, teacher, and settings data. This is source-generated synthetic
baseline evidence, not historical production-workbook parity.

Executor and independent fresh-base Tester passed
`ClassMngrScheduleImportTests` (1/1). The focused Windows x64 Debug build used
CMake 4.4.2, Ninja 1.13.2, MSVC 19.51.36231, and Qt 6.12.0. The independent
fresh-base build used MSVC 19.51.36257, validated 925 handwritten source
owners, and also passed CTest 1/1. `git diff --check` passed. Nonfatal
warnings included missing `vswhere.exe`, optional Vulkan headers, zlib
fallback, and long object paths. No full suite was run.

### Cumulative exit-gate status after F88

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior | Partial | F79 adds validated Class Transfer intervals; F80 and F81 add Qt-free Teacher Import update policies; F84 types Schedule Import matching keys; F85 adds Qt-free Teacher Import plan validation; F87 adds the Qt-free GS Team sparse-merge/no-op policy. F88 changes tests only; broader behavior remains incomplete. |
| Baseline parity | Partial | F82 and F83 compare checked inputs added after baseline and provide common-input differential regression. F86 and F88 compare identical inputs generated by helpers/data present at baseline, with deterministic seeds and matching semantic transcripts. These are source-generated synthetic comparisons, not historical production-workbook parity. Broader parity remains incomplete. |
| Workspace boundary | Satisfied | The formal WorkspaceGateway/WorkspaceCoordinator acceptance and focused app-less coverage remain satisfied; F88 changes no workspace behavior. |
| v2 dependency isolation | Satisfied in the audited v2 scope | Audited `src/next` sources remain free of direct DataService, MainWindow, PageManager, and widget-pointer dependencies; F88 changes tests only. |

Gate 1 and Gate 2 remain Partial; workspace boundary and audited
`src/next` dependency isolation remain Satisfied. Phase 2 remains In Progress
with its exit gate Open. Sub Prep remains capped at 2026-2027.

Next selected bounded slice (2026-09-27): F89 adds Qt-free
`teacher_import_match_cardinality.h` to classify match counts as zero, one, or
multiple. Integrate it into all three Teacher Import loops and its app-less
target/tests. Keep `QString` normalization, matching scan/order, GS Team
Korean preference, exact localized errors and rejection, SQL, counters,
transactions, and rollback in the repository. Add repository coverage for
Korean-key ambiguity, which current tests do not explicitly cover; preserve
existing Native English and GS Team ambiguity coverage. Verify the new
app-less classifier target and `ClassMngrTeacherImportTests`. This is selected
work, not implementation evidence. Sub Prep remains capped at 2026-2027.
