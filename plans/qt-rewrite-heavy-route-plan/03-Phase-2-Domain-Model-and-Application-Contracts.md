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

- Status: Complete
- Default route: Heavy
- Depends on: Phase 1
- Blocks: Persistence, bootstrap, shared UI, and feature migration
- Owner: Unassigned
- Last updated: 2026-10-22
- Existing progress log: [03-Phase-2-Progress-Log.md](03-Phase-2-Progress-Log.md)
- Exit gate: Satisfied
- Current note: Phase 2 is Complete. The cumulative audit found Gate 1 and
  Gate 2 satisfy their criteria. F1317 (`28f77666`) is the latest accepted
  slice. No batch is active; F629's Windows x64 requalification remains
  separate. See the [progress log](03-Phase-2-Progress-Log.md) for evidence and
  limitations.

### Latest Progress Update

F1317 (`28f77666`) makes Class Details save fail closed when canonical-class
validation context cannot be read or identifies another class. The independent
Tester accepted the focused use-case verification. See the progress log for
the exact target and run results.

### Slice batches

The shared [current slice-batch tracker](Current-Slice-Batches.md) has no active
batch after Phase 2 completion. Do not retain closed-batch details. The [Phase 2
progress log](03-Phase-2-Progress-Log.md) retains useful implementation,
verification, decision, and gate evidence. F629's source-bound Windows x64
layout trace and requalification remain outstanding separately from Phase 2
and Phase 0's 2026-09-18 Complete declaration.

### Deferred candidates

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

These categories are coverage goals, not a requirement for one standalone
Domain aggregate per noun. Add a reusable Domain type when a real consumer
needs shared identity, lifecycle, or an invariant; keep projections, commands,
import review, and preferences in the owning Application contracts.

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
- Importing teachers, schedules, calendars, rosters, and class transfers.
- Editing teachers, classes, schedules, calendar events, rosters, and evaluations.
- Generating reports and substitute documents.
- Listing campus and document metadata.
- Opening document content on demand for a viewer or output operation.
- Performing backups and recovery.
- Checking for application updates after startup.

Workspace opening continues to support existing schema-v6 `.db` files.
Existing schemas below v6 are unsupported, and migration is not offered.

The evidenced backup/recovery behavior is profile-replacement backup and
rollback plus initial-setup cancel restoration. Do not add a user-initiated
backup/restore workflow without baseline product behavior; durable persistence
mechanics remain with Phase 3.

F1316 adds a Qt-free Application use case that creates the Sub Prep generation
plan in the page before its first print-source read, then passes that same plan
to `SubPrepPackageService::generate()` before roster reads or other package
side effects. The typed request/result and structured validation errors use
`Domain::GregorianDate`, typed final class IDs, explicit schedule weekdays,
normalized dates, the separately derived ordered/deduplicated roster-query
weekdays, schedule mode, output actions, requested roster columns, and needed
folder label, replacement choice, and target-readiness fields. Preserve current
date normalization and selection order, modes, columns, and folder-only
behavior. Application types must not depend on Qt, widgets, URLs,
callbacks, concrete paths, or renderer types. Destination handling, staging or
replacement, rendering, printing, and folder opening remain feature/platform
edges for Phases 7/8. Acceptance must cover different vacation-date weekdays
and explicit schedule weekdays, proving each source uses its own scope.
The app-less acceptance cases cover this contract. F1316's focused verification
scope and the remaining macOS PDF-reference limitation are recorded in the
progress log.

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

## Current gate findings and parity decisions

- Gate 1: Satisfied. Domain/Application contracts and test surfaces are
  app-less; explicit Workspace and Domain contract targets passed. The source
  scan found no `MainWindow`, `QApplication`, or `QCoreApplication` references
  in 169 inspected sources. Section 2.1 type categories are coverage goals; no
  consumer-backed production gap remains.
- Gate 2: Satisfied. Teacher Import ambiguity/full-snapshot and Speaking
  Evaluation page-level import differentials passed, alongside existing bounded
  fixtures. These checks are not exhaustive parity proof. Exact scopes and
  limitations are recorded in the progress log.
- The incremental 169-target audit built 168/169 targets; selected registrations
  passed 167/170. One stale Schedule Import test failed compilation after a
  typed-key API change, and three selected tests retain stale expectations that
  teacher ID `2147483648` is invalid. The full suite was not run. These failures
  do not negate the separately passing acceptance targets or literal
  app-less-testability criterion; no changes were made to those stale tests.
- F1103 is resolved by user direction: reject Class Details save when canonical
  class validation-context reading fails or returns a mismatched class ID. F1317
  implements this as a structured failure before conflict lookup/save; malformed
  IDs retain their prior validation result. This is an explicit fail-closed
  exception to pinned legacy behavior, which can save sentinel/default teacher,
  notes, and activities after a context-read failure.
- Preserve the user-approved fail-closed behavior when Teacher Import's SQL
  cursor reports `lastError()` and when Speaking Evaluation cannot read its
  evaluation snapshot before Import Names. The Class Transfer identity-free
  package remains an explicit parity exception; exact parity is not claimed for
  that input.
- Calendar Import's 4,096-event save cap remains removed to match legacy.
- Existing schema versions below v6 are unsupported; migration is not offered.
  Existing profile-replacement and initial-setup lifecycles cover the evidenced
  recovery cases. No user-initiated backup/restore workflow was found, so Phase
  2 does not add one.

Phase 2 is Complete. The [progress log](03-Phase-2-Progress-Log.md) records
F1316/F1317 verification, bounded comparisons, explicit policy exceptions, and
the audit's test limitations. F629's Windows x64 requalification remains
separate from the Phase 2 exit gate.

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

Evidence for earlier verified slices and cumulative exit-gate snapshots remains
in the existing [Phase 2 progress log](03-Phase-2-Progress-Log.md).
