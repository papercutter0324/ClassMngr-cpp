# Phase 3 — Persistence Rewrite

Read [00-Start-Here.md](00-Start-Here.md) first for the overall plan, workflow, and phase sequence.

## Mandatory route for new and resumed work

- Use the Heavy route for every task, slice, follow-up, and reopened gate in this phase. `Default route: Heavy` is a binding instruction.
- At each start or resume, explicitly select Heavy and reread this phase plan and [00-Start-Here.md](00-Start-Here.md). Follow the active `AGENTS.md` Heavy-route instructions and `~/.codex/codex_workflow/heavy_route.md`, including their delegation, verification, and deployment-state requirements.
- A new session, handoff, or context reset does not change the route. Do not continue under Light or Medium. If a required Heavy-route step blocks progress, report the blocker before implementation instead of silently switching routes.

## Build and test verification

- For routine slice checks, reuse a configured build tree and build the
  affected test targets and their dependencies. Let the build system recompile
  changed or out-of-date inputs; a fresh build is not required for every slice.
- Use a fresh build tree when changing the build system, toolchain, or
  dependency configuration; when stale artifacts could explain a result; or
  when a phase gate explicitly requires clean-checkout evidence.
- Keep any phase-specific full builds, test suites, platform matrices, and
  packaging checks required by an exit gate. Record whether verification used
  an incremental or fresh build, which targets were built, and which tests
  ran. Describe focused results as focused; do not report them as a full-suite
  pass.

## Status

- Status: Not started
- Default route: Heavy
- Depends on: Phases 1 and 2
- Blocks: Workspace, feature migration, and cutover
- Owner: Unassigned
- Last updated: 2026-09-15
- Current note: Reproduce current data behavior first, then remove the compatibility facade.

### Slice discovery batches

Discover upcoming slices in ordered batches of up to ten (or all remaining
slices if fewer than ten remain). Record each batch as an ordered list under
`Recorded batches` below and work through those slices in order. Begin discovering
and recording the next batch when starting work on the second-last slice in the
current batch. If a discovery pass finds fewer than ten slices, add the exact
standalone line `No other slices were found.` beneath that batch.

Keep the Status `Current note` limited to the latest information relevant to the
current or next slice. Keep only the most recent slice commit in the
`Latest Progress Update` section. When writing a newer update, move the previous
one to this phase's progress log before replacing it.

#### Recorded batches

Add each ordered discovery result here as it is found.

## Progress log

Record this phase's progress here. Add a dated entry when work starts, a
milestone is reached, a blocker appears, or the exit gate passes. Append entries
in date order and include what changed, what remains, evidence or a verification
command, and any new risk or blocker.

Entry format:

### YYYY-MM-DD — <milestone or update>

- Changed:
- Remaining:
- Evidence:
- Risks or blockers:

## Objective

Replace the broad compatibility-heavy data path with one explicit workspace persistence boundary while preserving all supported files and data behavior.

## Target structure

    WorkspaceStore
    ├── Connection lifecycle
    ├── Transaction management
    ├── Schema versioning
    ├── Backup and recovery
    └── Repository factory

    Repositories
    ├── TeacherRepository
    ├── ClassRepository
    ├── ScheduleRepository
    ├── CalendarRepository
    ├── RosterRepository
    ├── SpeakingEvaluationRepository
    ├── CampusRepository
    └── DocumentRepository

## Work packages

### 3.1 Schema behavior

Reproduce the existing schema behavior before changing the schema.

Record:

- Tables.
- Columns.
- Constraints.
- Foreign keys.
- Indexes.
- Default values.
- Migration versions.
- Legacy compatibility columns.
- Transaction boundaries.

Do not combine a major schema redesign with the first UI migration.

### 3.2 Migration engine

Implement explicit version-to-version migrations.

For every supported version:

- Create a fixture.
- Migrate it.
- Verify schema structure.
- Verify all data.
- Verify indexes and constraints.
- Verify application behavior.

Create backups before migration and preserve the original file if migration fails.

### 3.3 Repository APIs

Repositories must:

- Expose application-facing records, not UI types.
- Support bounded queries.
- Support pagination or windowed reads for large data.
- Use transactions intentionally.
- Report structured errors.
- Avoid returning unnecessary columns or related records.
- Avoid copying large collections repeatedly.

For Sub Prep, the repository boundary must support a visible-class summary
projection, teacher data shared by teacher ID, aggregated roster counts, and
on-demand details for the selected class. The complete feature slice is
tracked in [Sub Prep Class Information and Output Memory
Plan](sub-prep-class-information-memory-plan.md).

### 3.4 File formats

Preserve:

- .tps opening and saving.
- Legacy .db import.
- Class-transfer JSON.
- Teacher imports.
- Schedule imports.
- Calendar imports.
- Roster imports and transfers.
- Exports.
- Backups.

Test invalid files, partial files, locked files, interrupted saves, and failed imports.

### 3.5 Compatibility removal

Migrate application use cases one at a time:

1. Add the new repository.
2. Add the new use case.
3. Compare old and new results against the same fixtures.
4. Migrate the UI or workflow.
5. Remove the corresponding old call sites.
6. Delete the obsolete compatibility method.

Remove DataService only when no production code or required test depends on it.

### 3.6 Memory and lifetime

Define ownership for:

- Database connections.
- Prepared statements.
- Repository objects.
- Query result sets.
- Read models.
- Import buffers.
- Large workbook structures.
- Temporary export data.

The persistence layer must not load the complete application dataset by default.

### 3.7 Hotspot-specific persistence boundaries

Implement the [Qt Rewrite Memory Hotspot
Remediation Plan](memory-hotspot-remediation-plan.md) at the persistence
boundary:

- provide visible-ID-filtered class summaries, shared teacher summaries,
  aggregated roster counts, and selected-class detail queries;
- process schedule and calendar workbooks in staged or bounded batches, and
  release raw bytes, worksheet buffers, and parsed compatibility structures
  when their stage completes;
- replace broad import-review lookups with one operation-scoped matching index;
- preserve class-transfer compatibility while avoiding simultaneous raw JSON,
  parsed document, domain package, and dialog copies where the format allows;
- expose query/result sizes and large-buffer lifetimes for memory diagnostics.

The v2 persistence layer must not use the current compatibility cell view or
full-workbook retention as an implicit application contract. Any temporary
adapter requires a named consumer and removal point.

## Deliverables

- WorkspaceStore.
- Explicit schema migration engine.
- Repository implementations.
- File compatibility tests.
- Backup and recovery behavior.
- DataService migration map.
- Large-data query and memory tests.

## Exit gate

All supported files open, save, import, export, migrate, and recover correctly in v2.

No v2 feature calls DataService or reaches directly into database internals.

## Heavy-route requirements

- For every Phase 3 slice, use the heavy route: move the slice through schema,
  persistence, and consuming use-case boundaries, compare it with legacy
  behavior, and remove temporary compatibility code after acceptance.
- Preserve data behavior before optimizing schema design.
- Do not silently discard unknown legacy fields.
- Do not migrate user files in place without backup.
- Do not retain complete duplicate datasets in repository, service, and widget layers.
- Make large reads bounded and observable.
