# Phase 2 — Domain Model and Application Contracts

> **Phase 2 priority:** Prioritize implementing production/core code. Tests may be
> run for any changed code. Create or update test files only to cover a
> corresponding production/core change; do not add test-only slices.

Read [00-Start-Here.md](00-Start-Here.md) first for the overall plan, workflow, and phase sequence.

## Mandatory route for new and resumed work

- Use the Heavy route for every task, slice, follow-up, and reopened gate in this phase. `Default route: Heavy` is a binding instruction.
- At each start or resume, explicitly select Heavy and reread this phase plan and [00-Start-Here.md](00-Start-Here.md). Follow the active `AGENTS.md` Heavy-route instructions and `~/.codex/codex_workflow/heavy_route.md`, including their delegation, verification, and deployment-state requirements.
- A new session, handoff, or context reset does not change the route. Do not continue under Light or Medium. If a required Heavy-route step blocks progress, report the blocker before implementation instead of silently switching routes.

## Build and test verification

- Use `build/windows-x64-debug` as the standard local Windows x64 Debug
  build folder for slice work. Reuse this same configured folder across
  slices and phases; do not create slice-, task-, or reviewer-specific build
  folders. Build the affected targets and their dependencies, and let the
  build system recompile changed or out-of-date inputs.
- If a fresh build is needed, empty the applicable standard build folder
  before configuring and building in it. For Windows x64 Debug, keep using
  `build/windows-x64-debug`; do not create a new folder for the fresh build.
  Apply the same rule when changing the build system, toolchain, or
  dependency configuration; when stale artifacts could explain a result;
  or when a phase gate explicitly requires clean-checkout evidence.
- A gate requiring another platform or configuration must use that CMake
  preset's standard `build/<preset-name>` folder and reuse it for that
  preset, rather than creating a slice-specific folder.
- Keep any phase-specific full builds, test suites, platform matrices, and
  packaging checks required by an exit gate. Record whether verification
  used an incremental or fresh build, which targets were built, and which
  tests ran. Describe focused results as focused; do not report them as a
  full-suite pass.

## Test scope

- User direction (2026-10-11): tests may be run for any changed code. Create or
  update test files only to cover a corresponding change in production/core
  code. Prefer the smallest focused assertion in an existing test file; create
  a separate test file only when a new production/core contract cannot be
  covered coherently in an existing file. Avoid tests for unchanged behavior,
  redundant cases, and test-only slices.

## Status

- Status: In progress
- Default route: Heavy
- Depends on: Phase 1
- Blocks: Persistence, bootstrap, shared UI, and feature migration
- Owner: Unassigned
- Last updated: 2026-10-21
- Historical progress log: [03-Phase-2-Progress-Log.md](03-Phase-2-Progress-Log.md)
- Exit gate: Open
- Current note: Batch517 (F1277) is complete and pushed at `67967637`; Batch518 discovery is next, with no implementation batch active and Gate 1 and Gate 2 Partial/Phase 2 exit Open.

### Latest Progress Update

Batch517 (F1277) is complete and pushed through closeout commit `67967637d86488442a20970754213d3d52054114` (see the [push receipt](03-Phase-2-Progress-Log.md#batch517-push-receipt-2026-10-21)). Batch518 discovery is next; no implementation batch is active. Gate 1 and Gate 2 remain Partial, and the Phase 2 exit gate remains Open. F629's source-bound Windows x64 requalification remains outstanding separately from Phase 0's 2026-09-18 Complete declaration.

### Slice discovery batches

Discover upcoming slices in ordered batches of up to ten (or all remaining when fewer than ten remain). Begin recording the next batch when work starts on the second-last candidate in the active batch.

Treat this plan as an operational summary, not a history:

- Keep `Current note` to one sentence about the current or next slice and open gate state; update it in place.
- Keep the active batch's complete ordered slice list, including accepted slices, until the batch closes; mark each slice's status and give remaining slices a one-line next action. Keep unresolved deferred candidates here as well.
- Store discovery evidence, full acceptance criteria, verification results, commit details, completed-batch summaries, and prior-batch history only in the chronological [Phase 2 progress log](03-Phase-2-Progress-Log.md).
- When a slice is accepted, record its evidence in the progress log and update its status in this plan. Remove the batch list when the batch closes; replace it in place with the next active batch and do not carry forward history.
- Record a discovery shortfall in the progress log only; do not add a historical marker to this plan.
- Keep `Deferred candidates` limited to unresolved items; remove entries when resolved or superseded.

Gate 1 and Gate 2 remain Partial, and the Phase 2 exit gate remains Open.
F629's source-bound Windows x64 layout trace and requalification remain
outstanding separately from Phase 0's 2026-09-18 Complete declaration.

#### Deferred candidates

- F285 - Deferred before implementation: addTeacher() creates a blank
  teacher, but required-name validation rejects it before the profile read.
  Revisit after the blank-draft creation contract is clarified.
- F579 - The user-directed review trigger is satisfied, but a fresh matched-symbol
  heap capture remains blocked because this process lacks an elevated Administrator
  token; resume when an elevated token is available.
- F614 - Deferred after F612: the measured source/package payload is 22,662
  UTF-8 bytes and the typed source releases before rendering; current evidence
  does not justify a material ownership refactor. Revisit with larger-fixture
  or owner-specific memory evidence.
- F618 - Deferred after a read-only export trace: package, JSON tree, and
  encoded buffer overlap, but no export-specific peak measurement establishes
  a material cost. Revisit with a matched packaged Release export measurement
  before changing serialization or QSaveFile behavior.
- F626 - Deferred: F621/F622 show Sub Prep feature-resource release/refresh on
  a retained page, and F1014 shows class-information re-entry behavior; there
  is still no owner-specific matched packaged Release retained-byte evidence
  or safe PageManager eviction/recreation contract.
- F707 - Deferred from Batch 178: the `sidebarClassNodeLessThan`/`timeOrder`
  helper chain has no observed app callsite; revisit when a live use is found.
- F709 - Deferred from Batch 178 and rechecked before F738: raw SQL values
  become invalid `QTime`s in the repository before projection, so the current
  public Calendar Page fixture cannot inject malformed projection text;
  revisit if a focused query/summary injection seam becomes available.
- F598 - Fresh matched WPR heap capture for the F596 Release route is blocked
  because the current execution identity is not an Administrator; resume when
  a Windows process with an elevated administrator token is available.
- Batch 197 teacher reassignment parity - deferred until the stricter Next
  notes/schedule validation policy is resolved against current tests.
- Class Analytics arbitrary roster-allocation cap and sparse representation
  change - deferred: F794 covers integer representability only; large
  int-representable rows retain existing vector allocation/catch behavior. Do
  not introduce arbitrary 4,096- or 25-row limits.
- Dense Speaking Evaluation row-limit change - deferred while its product
  policy remains unresolved; do not add a new limit before that decision.
- Sidebar Add/post-create wide-ID transition - deferred under F285: blank-draft
  Add is rejected before persistence, so revisit create-path identity propagation
  only after the draft-creation contract is clarified.
- F1103 - Deferred pending explicit parity/policy reconciliation: the Class
  Details save workflow ignores a failed validation-context read and applies
  sentinel/default values; an existing test asserts that fallback. Do not
  change this behavior until the policy is resolved.

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

Earlier verified slices and cumulative exit-gate snapshots are archived in the [Phase 2 progress log](03-Phase-2-Progress-Log.md).
