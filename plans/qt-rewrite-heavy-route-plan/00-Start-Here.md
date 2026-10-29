# ClassMngr Qt-Rewrite — Start Here

## Collective status

- Overall status: In progress
- Default route: Heavy route
- Branch scope: Qt-Rewrite
- Last updated: 2026-10-29
- Current milestone: Phase 7 remains in progress. Candidate #40,
  Reports/PDF/PowerPoint/export jobs, is accepted in `f2b24bd7`; Batch 3 and
  its Windows evidence support were pushed at `2cfb1db3`. Batch 4 is active
  with candidates #41–48. Candidate #41's Campus Dashboard boundary is
  accepted for implementation: a packaged read-only catalog, writable
  app-data overrides with import from the one known historical directory, and
  a campus-local admin capability. Campus Staff remains owned by #17–19, and
  the general permission gate by #46.
  See the [Phase 7 plan](08-Phase-7-Feature-Migration.md) and [current batch
  tracker](Current-Slice-Batches.md). The Initial Setup personal-details
  screenshot subgate is closed. The generated synthetic schema-v6 `.db`
  relationship/output check passed; it is not historical user-profile
  evidence. The Windows Release memory collection on the pushed head is
  running ([workflow run 40914914018](https://github.com/papercutter0324/ClassMngr-cpp/actions/runs/40914914018));
  its artifact has not yet been reviewed. Broader 7A visual evidence, memory
  evidence, and the Windows packaged-Next memory gate remain open. The 7A and
  Phase 7 exit gates are not yet passed.
- Phase 0 was declared Complete by the user on 2026-09-18. The separate
  post-F629 source-bound Windows x64 requalification remains outstanding and
  does not reopen that declaration; see the [Phase 0 progress log](01-Phase-0-Progress-Log.md)
  for baseline history. Phase 1 is Complete; see its
  [plan](02-Phase-1-Build-System-and-Repository-Structure.md) and
  [progress log](02-Phase-1-Progress-Log.md) for gate and run evidence.
- Release target: ClassMngr v2 with feature parity, no splash screen, no
  external/legacy packs mounted or downloaded (with F1467 Calendar QML
  resources compiled in), and Windows startup memory below 250 MiB

### Phase status

| Phase | File | Status | Default route | Depends on |
|---|---|---|---|---|
| 0 | 01-Phase-0-Product-Contract-and-Baseline.md | Complete | Heavy | None |
| 1 | 02-Phase-1-Build-System-and-Repository-Structure.md | Complete | Heavy | 0 |
| 2 | 03-Phase-2-Domain-Model-and-Application-Contracts.md | Complete | Heavy | 1 |
| 3 | 04-Phase-3-Persistence-Rewrite.md | Complete | Heavy | 1, 2 |
| 4 | 05-Phase-4-Resource-Loader-and-Packaging.md | Complete | Heavy | 1 |
| 5 | 06-Phase-5-Startup-and-Bootstrap-Rewrite.md | Complete | Heavy | 2, 3, 4 |
| 6 | 07-Phase-6-Shared-UI-Rewrite.md | Complete | Heavy | 5 |
| 7 | 08-Phase-7-Feature-Migration.md | In progress — candidate #41 active | Heavy | 2–6 |
| 8 | 09-Phase-8-Platform-and-Output-Adapters.md | Not started | Heavy | 2, 3, 7 |
| 9 | 10-Phase-9-Windows-Memory-Hardening.md | Not started | Heavy | 4–8 |
| 10 | 11-Phase-10-Visual-Behavioral-and-Cross-Platform-Parity.md | Not started | Heavy | 7–9 |
| 11 | 12-Phase-11-Test-Restructuring-and-Release-Gates.md | Not started | Heavy | 0–10 |
| 12 | 13-Phase-12-Beta-Cutover-and-Legacy-Removal.md | Not started | Heavy | 10, 11 |
| 13 | 14-Phase-13-Post-Release-Maintenance.md | Not started | Heavy | 12 |

Statuses are intentionally conservative. A phase is not In progress until its work has started in the repository, and it is not Complete until its exit gate has passed.

Phase 4–7 implementation, verification, and exit-gate history is in the
[Phase 4 progress log](05-Phase-4-Progress-Log.md),
[Phase 5 progress log](06-Phase-5-Progress-Log.md),
[Phase 6 progress log](07-Phase-6-Progress-Log.md), and
[Phase 7 progress log](08-Phase-7-Progress-Log.md).

## Phase 2 completion

Phase 2 is Complete; Gates 1 and 2 passed. F629's post-change Windows x64
requalification remains separate. See the [Phase 2 progress log](03-Phase-2-Progress-Log.md)
for the audit results, evidence, and limitations.

## Phase notes, slice batches, and progress logs

- Keep each phase plan's `Current note` limited to the latest information that
  is relevant to the current or next slice. Remove stale progress details
  instead of archiving them solely for history; keep durable requirements and
  decisions in their appropriate plan sections.
- Keep only the most recent slice commit in the phase plan's
  `Latest Progress Update` section. Replace it when a newer update is written;
  retain the previous update elsewhere only when it contains evidence or a
  decision that remains useful.
- Keep the `Current milestone` note to the most recent milestone only. Replace
  the entire note when updating it; remove the previous note instead of
  appending or preserving earlier milestones.
- Track only the active slice batch and the next batch once its discovery
  starts in the shared [current slice-batch tracker](Current-Slice-Batches.md).
  Discover up to 20 ordered candidates per batch, or all remaining candidates
  when fewer than 20 remain, and work through them in order. Begin discovering
  the next batch when starting the second-to-last candidate in the active batch;
  for a one-candidate batch, begin when work on that candidate starts. If
  discovery identifies possible candidates beyond the 20-candidate limit,
  record a reminder in the shared tracker to search for them and any others
  when next-batch discovery is due. If a completed discovery finds no other
  candidates, record `No other slices were found.` for that batch. Do not create
  a separate batch file per phase or keep a historical batch log; replace
  closed batch details as the tracker advances.
- Use a sibling file named from the phase plan and ending in `-Progress-Log.md`
  when that phase has one (for example,
  `03-Phase-2-Progress-Log.md`); otherwise use its `Progress log` section.
  Use it only for implementation or verification evidence, decisions, and gate
  findings that remain useful. Routine slice chronology and closed batches do
  not need to be archived. Keep requirements and current phase status in the
  phase plan, and live batch lists and discovery reminders only in the shared
  tracker. No separate batch-history log is needed.

## Git commits and pushes

Git staging, committing, and pushing have often required elevated permissions
in this environment. If the default sandbox denies a requested Git operation,
retry it with elevated permissions, then verify the commit and remote branch tip.

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

## Cross-cutting memory remediation

The audit of large-data and widget-heavy paths is tracked in the
[Qt Rewrite Memory Hotspot Remediation Plan](memory-hotspot-remediation-plan.md).
It is an execution plan across the numbered phases, not a new phase. Phase 0
owns the measurements, Phases 2–8 own the data and lifecycle fixes, Phase 9
owns the packaged Release memory gate, and Phases 11–13 own permanent
regression coverage and removal of temporary paths.

## Commit message convention

Use the standardized prefix `Phase# - ` for commits related to this rewrite, replacing `#` with the primary phase number. For example: `Phase0 - Add the initial baseline evidence`. For changes spanning multiple phases, use the phase that owns the primary deliverable.

## Session handoff notes

Update `agent_docs/latest_session_work.md` only when a handoff is expressly requested by the user.

## Important context

This plan is based on the current Qt-Rewrite source tree. Existing plan documents in the branch are intentionally ignored and do not define scope or architecture.

The current repository contains a large Qt desktop application with:

- A monolithic startup path in src/main.cpp.
- A MainWindow that composes services, pages, actions, menus, and controllers.
- A lazy but resource-pack-coupled PageManager.
- ApplicationServices and feature services that still fall back to a broad DataService compatibility facade.
- A database layer with repositories and a large compatibility surface.
- ResourcePackManager, ResourcePackLease, ResourcePackUpdateService, ResourcePaths, external RCC packs, and resource-pack deployment settings.
- A splash screen and splash asset on the startup path.
- Large table-oriented UI areas that create many Qt objects.
- Embedded or bundled documents, fonts, templates, maps, translations, styles, icons, and report assets.
- Qt Widgets plus a Qt Quick calendar component.

The worktree already contains user-owned changes. Do not overwrite or revert them while implementing this plan. In particular, preserve unrelated CMake changes and existing deletions.

## Product constraints

The rewrite must:

1. Preserve all current user-facing features.
2. Discard the splash screen rather than replacing it with another full-screen startup screen.
3. Add a new typed resource loader for startup and on-demand resources.
4. Preserve the current appearance, layout, typography, themes, language support, navigation, shortcuts, dialogs, and output formats.
5. Reduce normal Windows memory use below 250 MiB.
6. Remove resource packs and their separate automatic update path.
7. Keep application updates, with resources delivered as part of the application release.
8. Preserve .tps files, legacy .db import behavior, class-transfer behavior, exports, backups, and generated outputs.

## What the heavy route means

The default route for every phase is the heavy route:

- Build a parallel ClassMngr v2 application instead of making only incremental edits to the old composition root.
- Create explicit domain, application, persistence, resource, platform, UI, and feature boundaries.
- Migrate complete vertical slices from storage through UI and output.
- Keep the old application as a parity oracle until cutover.
- Delete compatibility code after migration instead of leaving permanent fallback paths.
- Treat memory, visual parity, file compatibility, and cross-platform behavior as release gates.
- Keep Qt as the presentation framework while removing unnecessary coupling from the core.

### Sub-agent check-in cadence

Each sub-agent may be checked in with only once every 10 minutes. Batch questions
and status requests so this cadence is maintained.

The heavy route is not permission to remove features, change user workflows, or redesign the application. The developer-only Memory Usage Monitor and its in-app diagnostics are an explicit scope exception. Otherwise, this is a commitment to replace the underlying ownership and lifecycle model thoroughly enough to meet the memory target.

### Slice-by-slice reminder

Every slice of every phase must use the heavy route. Treat a slice as a bounded
vertical unit of work, not a one-layer patch: define its target v2 boundary,
move and verify its end-to-end behavior, keep the legacy path only as a parity
oracle or an explicitly temporary bridge, and remove that bridge when the
slice is accepted. Do not switch an individual slice to a lightweight or
incremental route without recording an explicit product or architecture
decision in this plan.

Reuse the standard build folder across slices. Do not create a unique subfolder
under `build/` for an individual slice. If a fresh build is needed, empty the
applicable standard build folder before configuring and building in it.

## Target architecture

    ClassMngrNext
    ├── AppShell
    │   ├── NavigationModel
    │   ├── PageHost
    │   ├── CommandBus
    │   ├── DialogService
    │   └── NotificationService
    ├── ApplicationRuntime
    │   ├── Use cases
    │   ├── Application state
    │   ├── Preferences
    │   └── Application update coordinator
    ├── Persistence
    │   ├── WorkspaceStore
    │   ├── Schema migrations
    │   └── Repositories
    ├── Domain
    │   ├── Models
    │   ├── Value objects
    │   ├── Rules
    │   └── Validation
    ├── ResourceSystem
    │   ├── ResourceCatalog
    │   ├── ResourceLoader
    │   ├── ResourceCache
    │   └── ResourceDiagnostics
    ├── Qt presentation adapters
    └── Platform adapters

The dependency direction is:

    UI → Application → Domain
    UI → Application → Persistence
    UI → ResourceSystem
    Platform → Application interfaces

Domain code must not depend on Qt Widgets. UI code must not issue SQL. Feature pages must not know where installed resources are stored. Application code must not depend on widget ownership or visibility.

Prompt inspection, dismissal, default-action activation, and screenshot capture
used by legacy startup/performance workflows belong to a Qt presentation or
test adapter. They must not be added to the permanent domain or application
prompt contract. Any compatibility driver for the legacy executable must be
explicitly temporary and removed when the corresponding v2 startup and shared
UI slices are accepted.

## Shared status rules

Each phase file contains its own Status section. Update it whenever work starts, a milestone is reached, a blocker appears, or the exit gate passes.

Use these statuses:

- Not started: no implementation work has begun.
- In progress: implementation or verification is actively underway.
- Blocked: work cannot continue because a concrete external dependency or unresolved decision prevents it.
- Ready for review: implementation is complete and evidence is being checked.
- Complete: all deliverables and exit criteria have passed.
- Deferred: explicitly moved out of the release scope by a documented decision.

Every status update should include:

- What changed.
- What remains.
- Evidence or test command.
- Any new risk or blocker.
- Date of the update.

## Resource policy

Resources are installed with the application in a deterministic read-only resource tree. There are no independently mounted RCC packs, no runtime resource-pack directory, no resource manifest download, and no resource-pack update request.

The new loader classifies resources as:

- Core: required to create the normal shell.
- Startup: required for the initial page.
- Feature: loaded when a feature is entered.
- Operation: loaded only for a print, export, import, or report operation.

Large documents, maps, report artwork, templates, optional fonts, and PDF content must not be resident merely because the application opened.

### Document viewer lifecycle

The document catalog is startup metadata, not document content. Startup may
load catalog schema, localized names, and validated asset references, but a
PDF displayed through QtPdf is loaded only after the user requests it. The
active viewer session owns that loaded document; closing the viewer, replacing
the document, leaving the viewer, or releasing the page must close the QtPdf
document and release its resource. Reopening may load it again. Generated and
print-output PDFs remain separate operation-scoped resources.

## Memory policy

The primary hard gate is Windows working set for a packaged Release build. Record private bytes, commit, and peak working set as secondary values.

Measure:

- Empty workspace startup.
- Representative workspace startup.
- First page render.
- Five minutes idle.
- Navigation through all normal pages.
- Large schedule, roster, campus, document, and speaking-evaluation workflows.
- Memory after leaving each large feature.
- Repeated open/close and language/theme changes.

No feature may pass solely by disabling functionality or reducing visual fidelity. The solution must improve ownership, loading, caching, and widget allocation.

## Feature migration order

The recommended dependency order is:

1. Setup and workspace/file flows.
2. Teachers and staff.
3. Classes.
4. Schedule and imports.
5. Calendar.
6. Rosters.
7. Speaking evaluations.
8. Campus and documents.
9. Substitute preparation and output.

Each feature is complete only when its domain behavior, persistence, application use cases, UI, resources, output, localization, and tests have moved.

## Non-negotiable review questions

Before accepting any phase, ask:

- Does this preserve every required feature?
- Does this preserve the current appearance?
- Does this introduce a new global owner or unbounded cache?
- Does this load a large resource earlier than necessary?
- Does this create one Qt object per data cell?
- Does this leave a compatibility fallback that should be removed later?
- Is the Windows Release memory impact measured?
- Can the behavior be tested without constructing the entire main window?
- Can the feature be released and recreated without losing persistent state?
