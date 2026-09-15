# ClassMngr Qt-Rewrite — Start Here

## Collective status

- Overall status: In progress
- Default route: Heavy route
- Branch scope: Qt-Rewrite
- Last updated: 2026-09-16
- Current milestone: Phase 0 — product contract, source archaeology, and baseline
- Current blocker: No external blocker. Phase 0 evidence collection continues
  for large-feature workflows, generated outputs, and cross-platform Release
  baselines.
- Release target: ClassMngr v2 with feature parity, no splash screen, no resource packs, and Windows startup memory below 250 MiB

### Phase status

| Phase | File | Status | Default route | Depends on |
|---|---|---|---|---|
| 0 | 01-Phase-0-Product-Contract-and-Baseline.md | In progress | Heavy | None |
| 1 | 02-Phase-1-Build-System-and-Repository-Structure.md | Not started | Heavy | 0 |
| 2 | 03-Phase-2-Domain-Model-and-Application-Contracts.md | Not started | Heavy | 1 |
| 3 | 04-Phase-3-Persistence-Rewrite.md | Not started | Heavy | 1, 2 |
| 4 | 05-Phase-4-Resource-Loader-and-Packaging.md | Not started | Heavy | 1 |
| 5 | 06-Phase-5-Startup-and-Bootstrap-Rewrite.md | Not started | Heavy | 2, 3, 4 |
| 6 | 07-Phase-6-Shared-UI-Rewrite.md | Not started | Heavy | 5 |
| 7 | 08-Phase-7-Feature-Migration.md | Not started | Heavy | 2–6 |
| 8 | 09-Phase-8-Platform-and-Output-Adapters.md | Not started | Heavy | 2, 3, 7 |
| 9 | 10-Phase-9-Windows-Memory-Hardening.md | Not started | Heavy | 4–8 |
| 10 | 11-Phase-10-Visual-Behavioral-and-Cross-Platform-Parity.md | Not started | Heavy | 7–9 |
| 11 | 12-Phase-11-Test-Restructuring-and-Release-Gates.md | Not started | Heavy | 0–10 |
| 12 | 13-Phase-12-Beta-Cutover-and-Legacy-Removal.md | Not started | Heavy | 10, 11 |
| 13 | 14-Phase-13-Post-Release-Maintenance.md | Not started | Heavy | 12 |

Statuses are intentionally conservative. A phase is not In progress until its work has started in the repository, and it is not Complete until its exit gate has passed.

## Commit message convention

Use the standardized prefix `Phase# - ` for commits related to this rewrite, replacing `#` with the primary phase number. For example: `Phase0 - Add the initial baseline evidence`. For changes spanning multiple phases, use the phase that owns the primary deliverable.

## Phase 0 update - 2026-09-15

- What changed: started the Phase 0 evidence set at commit `75755460`; added
  static source archaeology, feature preservation, file compatibility, resource
  ownership, baseline, and risk documents under `docs/qt-rewrite/`.
- What remains: capture visual references, create representative file fixtures,
  run a fresh packaged Release baseline on every target platform, and complete
  startup/resource tracing.
- Evidence: `ctest --test-dir build/windows-x64-debug -N` currently enumerates
  66 tests, but that build tree contains stale cache options and is not treated
  as authoritative. A clean Phase 0 build directory is being established.
- Risk: historical baseline artifacts describe an older source snapshot and
  must not be used as the rewrite's acceptance baseline.

## Product contract update - 2026-09-16

- The document catalog may load startup metadata, but PDFs displayed through
  QtPdf are loaded only when the user requests them.
- The active viewer session owns the loaded document and must close/release it
  when the document is replaced, the viewer is closed, or the page is left or
  released. Generated and print-output PDFs remain operation-scoped.
- Phase 0 records the startup-negative, on-demand-open, and release-memory
  evidence still required; later phases now carry the same lifecycle contract.

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

The heavy route is not permission to remove features, change user workflows, or redesign the application. The developer-only Memory Usage Monitor and its in-app diagnostics are an explicit scope exception. Otherwise, this is a commitment to replace the underlying ownership and lifecycle model thoroughly enough to meet the memory target.

### Slice-by-slice reminder

Every slice of every phase must use the heavy route. Treat a slice as a bounded
vertical unit of work, not a one-layer patch: define its target v2 boundary,
move and verify its end-to-end behavior, keep the legacy path only as a parity
oracle or an explicitly temporary bridge, and remove that bridge when the
slice is accepted. Do not switch an individual slice to a lightweight or
incremental route without recording an explicit product or architecture
decision in this plan.

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
