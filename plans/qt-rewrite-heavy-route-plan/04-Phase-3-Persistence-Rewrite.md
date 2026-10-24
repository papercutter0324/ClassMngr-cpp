# Phase 3 — Persistence Rewrite

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

## Status

- Status: In progress
- Default route: Heavy
- Depends on: Phases 1 and 2
- Blocks: Workspace, feature migration, and cutover
- Owner: Unassigned
- Last updated: 2026-10-24
- Current note: Batch 3 (F1358–F1377) implementation tip was
  `018b30b04ab713769f9e69b440ba7223310a2b87`; its push receipt documentation
  was later pushed as
  `6597875f6036224767fa8db7331eac7eac856869`, the remote tip before Batch 4
  began. Batch 4 (F1378–F1397) is complete and pushed to `origin/Qt-Rewrite`;
  the verified implementation tip is
  `544b94cf97ab9fce8bb1e174ba1d9e0c9774d1c3`. Batch 5 discovery is complete
  and Batch 5 is active with F1409 (Sub Prep personal Zoom preferences)
  current. F1408 (Sub Prep preferences) is accepted at
  `d8cb586497ce64ca700f1f4deb500bdc1d8375e0`; focused verification passed.
  Its 8 prioritized candidates remain in the current batch tracker. Batch 5
  remains unpushed until its remaining slices are complete. F1403 was
  limited to residual range projection and excluded F1341 availability/by-ID
  work and the existing WorkspaceStore-backed query/cache path.
  F1376–F1408 implementation evidence, Batch 2 and Batch 3 push receipts, and
  earlier UI limitations and attribution notes are in the
  [progress log](04-Phase-3-Progress-Log.md).
  See the [schema baseline](../../docs/qt-rewrite/phase-3-schema-baseline.md),
  [progress log](04-Phase-3-Progress-Log.md), and [current batch
  tracker](Current-Slice-Batches.md).

### Slice batches

Use the shared [current slice-batch tracker](Current-Slice-Batches.md) for live work when Phase 3 is active. It contains only the active
batch and the next batch once discovery starts; completed batches are
not archived there.

## Progress log

Use this section only when a decision, acceptance result, verification
finding, blocker, or exit-gate detail needs a durable record. Routine
slice chronology and closed batch history do not need to be archived;
keep current status in this plan and live batches in the shared tracker.

## Latest Progress Update — F1408 — 2026-10-24

F1408 is accepted at `d8cb586497ce64ca700f1f4deb500bdc1d8375e0`. Only the
`ApplicationServicesSubPrepPreferencesPort` header/implementation and its
dedicated Platform fixture moved from `DatabaseSession` to `WorkspaceStore`.
Reads remain sequential across `subPrep/classMaterials`,
`subPrep/bookReportGrading`, `subPrep/bookReportSpecialInstructions`, then
`subPrep/subComments`, preserving UTF-8 bytes. Missing required values become
empty, missing optional values become `nullopt`, and present-empty optionals
remain present. The first repository read failure is logged and returns
nonrecoverable `Technical` without a partial bundle. An open store without a
repository retains load-default/warn behavior; save without a repository
returns `Technical`. Saving remains one atomic four-setting repository call.
F1409's Zoom keys were not changed. F1392's current-campus port and page-owned
missing-value defaults remain separate: the page supplies the grading default
when grading is absent and the special-instructions default only when grading
is also absent.

The Platform port and Sub Prep page targets built incrementally without
compiler warnings or errors. CTest #448, the dedicated port target, passed
9/9. CTest #147 for the page was interrupted after 147.28 seconds: its first
16 slots passed, then `unavailablePreferenceLoadsPreservePageValues` had run
for 145.482 seconds. It was stopped as the known F1392 baseline timeout; this
run is incomplete, not a natural timeout result, and that slot remains
unverified. The four-slot focused UI invocation passed all slots, including
`freshAndExistingGradingSettingsResolveWithoutDataLoss`,
`unavailablePreferenceSaveHasNoSideEffects`,
`failedPreferenceSaveReportsOnlyExplicitFailureAndPreservesDraft`, and
`repositoryReadFailureBlocksWritesUntilSuccessfulReload`; the last three
safe slots also passed when rerun after correction. Qt offscreen size/raise
and font-alias warnings and the expected injected F1079 read-failure error
were observed. Full suites and cross-platform builds did not run. F1408 is
accepted; F1409 (Sub Prep personal Zoom preferences) is current with 8
prioritized candidates remaining. Earlier baseline findings retain their
separate attribution in the progress log.

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

For the selected roster editor, F1326 adds a bounded read of rows 0–24 while
preserving the existing generic full-roster read behavior for other consumers.
Keep ordered dynamic column names and widths. F1327 adds tail-preserving
window writes, so stored rows beyond the editor window remain available during
visible edits and row transfers. Do not infer a total editor-column cap from
output-specific limits; the editor still reads its complete column schema, so
column-count memory remains a separate, unbounded dimension.

F1327 adds selected-editor window saves that replace only rows 0–24 and retain
tail rows at their existing absolute indices. When the projected column schema
changes, remap tail cell indices transactionally by the roster's normalized
column identity; fail without writing if the mapping is ambiguous. Row
removal and transfer affect only the visible window and do not pull hidden
tail rows across its boundary. A source/target roster transfer remains one
atomic write. Keep generic full-replacement repository operations unchanged
for their other callers.

For Sub Prep, the repository boundary must support a visible-class summary
projection, teacher data shared by teacher ID, aggregated roster counts, and
on-demand details for the selected class. The complete feature slice is
tracked in [Sub Prep Class Information and Output Memory
Plan](sub-prep-class-information-memory-plan.md).

Speaking-evaluation single and batch reads use their existing typed
application ports with `WorkspaceStore`. Keep the ordered detailed read
behavior: the single-read port is also used by roster score-import fallback,
so it must not silently cap rows for the page's 25-row display.
Speaking-evaluation save uses the existing detailed repository transaction
and preserves full-matrix versus dirty-cell update semantics.

F1330 retains all non-testing classes and their regular and intensive
schedule rows for the complete Schedule Builder board. Mode and visible-day
filtering remain in the builder; do not cap classes when the board has no
class-page selection. Intensive slot-state and testing-assignment reads remain
separate from this source read.

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
