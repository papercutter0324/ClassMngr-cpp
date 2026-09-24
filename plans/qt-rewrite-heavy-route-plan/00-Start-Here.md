# ClassMngr Qt-Rewrite — Start Here

## Collective status

- Overall status: In progress
- Default route: Heavy route
- Branch scope: Qt-Rewrite
- Last updated: 2026-09-23
- Current milestone: Phase 2 Application-contract slices are continuing;
  calendar-import planning and the existing-signature read cutover are
  complete. Sub Prep now has session-backed Platform reads for selected-class
  details, print source, and schedule summaries. Next is Work Package D:
  model-backed class list/navigation and a reusable selected-class detail
  view. Page/output wiring, parity, and 96-class Release memory evidence remain
  open. Phase 1 build-system exit evidence remains outstanding.
- Current blocker: official Phase 1 targets are Windows x64 and macOS
  universal. On commit `57f5dff6`, Windows x64 passed 66/66; macOS Debug failed
  after GitHub reported runner communication loss. The user observed the
  updater test, but no job log or JUnit artifact confirms it as the cause.
  Windows and macOS Packaged Release runs passed. Local Windows x64 Debug
  configure/build and CTest passed 66/66 on `4dbe3ca7`; that remains valid
  evidence independent of hosted results. Phase 1 Build Quality has not run.
  Linux and Windows ARM64 are unofficial and deferred; their current failures
  or missing native launch evidence are not Phase 1 blockers. See the Phase 1
  plan's GitHub Actions test-reliability section. Phase 0 is complete; Phase 1
  remains in progress.
- Release target: ClassMngr v2 with feature parity, no splash screen, no resource packs, and Windows startup memory below 250 MiB

### Phase status

| Phase | File | Status | Default route | Depends on |
|---|---|---|---|---|
| 0 | 01-Phase-0-Product-Contract-and-Baseline.md | Complete | Heavy | None |
| 1 | 02-Phase-1-Build-System-and-Repository-Structure.md | In progress | Heavy | 0 |
| 2 | 03-Phase-2-Domain-Model-and-Application-Contracts.md | In progress | Heavy | 1 |
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

## Phase 1 update - 2026-09-18

- Phase 0 is complete at commit `f8bb5954`: the combined Windows x64 and macOS
  universal 24-route exit gate passed, and the user confirmed the visual review.
- Phase 1 slices 1.1-1.3 establish the parallel Qt Core-only `ClassMngrNext`
  bootstrap, source-free layer/feature boundaries, and per-target dependencies
  measured from Ninja data. The legacy runtime retains its full Qt module set.
- A fresh Ninja/MSVC Debug configuration built both executables in 351 steps;
  `ClassMngrNextLaunch` passed 1/1. The `ClassMngrDomain` compile command has
  only QtCore/QtGui include paths, with no Widgets, Sql, or Network paths.
- `ninja -t commands ClassMngr.exe` confirms the full legacy Qt link set,
  including `Qt6::QuickControls2`. `ClassMngrSharedPolicyTests` built after
  importing VS DevCmd, and its targeted CTest passed 1/1; `ClassMngrNext.exe`
  launched with exit code 0.
- Slice 1.4 completed: explicit source manifests and configure-time ownership
  checks cover production, executable, QML, and test sources. A clean Windows
  Ninja/MSVC Debug configure reported 653 handwritten files; the build passed
  for `ClassMngr`, `ClassMngrNext`, and five affected test targets. Six
  targeted CTests passed.
- Slice 1.5 adds the compile database, v2-scoped format/tidy CI, configure-time
  gates, module/resource/package reports and checks, packaged Release workflow
  integration, and a startup/memory-labeled CTest entry point. Local resource
  checks passed for six generated RCCs and seven runtime IDs; the staged
  package report passed. Cross-platform CI and local clang tools were not run.
- Slice 1.6 adds PR-triggered Debug validation for Windows x64, Windows ARM64,
  macOS universal, and Linux. ARM64 cross-builds on x64 without execution;
  existing platform packaging workflows remain the Packaged Release paths.
  Independent static review passed. On a clean snapshot at `6f2f5fb0`, local
  Windows x64 Debug configure/build and CTest passed 66/66, including the v2
  launch and startup performance tests. The local VS 2026/MSVC 19.51 + Qt 6.12
  Windows x64 Release packaged-installer path also succeeded, but is
  supplemental because the exact VS2022 configure failed: no VS2022 instance
  is installed.
- Hosted workflow results have now been queried on source commit `57f5dff6`:
  the official Windows x64 Debug job passed 66/66, and the macOS Debug job
  failed after the hosted runner lost communication with GitHub. The user saw
  `ClassMngrUpdaterTests` running, but no log or test report confirms it as the
  cause. The Windows and macOS Packaged Release runs passed. The local Windows
  x64 Debug pass of 66/66 on `4dbe3ca7` using VS 2026/MSVC 19.51 remains
  independent passing evidence. Phase 1 Build Quality still needs a hosted
  run. Linux and Windows ARM64 are unofficial, deferred builds; their tests
  and native launch are not Phase 1 blockers. Phase 1 remains open for the
  official Windows/macOS checks and quality workflow. No later phase is
  complete.

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
