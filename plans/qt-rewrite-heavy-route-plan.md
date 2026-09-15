# ClassMngr Qt-Rewrite — Heavy-Route Plan

This file is the index for the detailed rewrite plan.

The plan is scoped to the Qt-Rewrite branch and is based on the current source tree. Existing plan documents in the branch are intentionally ignored.

## Start here

[00-Start-Here.md](qt-rewrite-heavy-route-plan/00-Start-Here.md)

The Start Here file contains:

- Collective status.
- Phase status table.
- Product constraints.
- Heavy-route definition.
- Target architecture.
- Shared memory and resource rules.
- Feature preservation requirements.
- Status update rules.

## Phase files

### Foundation

1. [Phase 0 — Product Contract, Source Archaeology, and Baseline](qt-rewrite-heavy-route-plan/01-Phase-0-Product-Contract-and-Baseline.md)
2. [Phase 1 — Build System and Repository Structure](qt-rewrite-heavy-route-plan/02-Phase-1-Build-System-and-Repository-Structure.md)
3. [Phase 2 — Domain Model and Application Contracts](qt-rewrite-heavy-route-plan/03-Phase-2-Domain-Model-and-Application-Contracts.md)
4. [Phase 3 — Persistence Rewrite](qt-rewrite-heavy-route-plan/04-Phase-3-Persistence-Rewrite.md)
5. [Phase 4 — Resource Loader and Packaging](qt-rewrite-heavy-route-plan/05-Phase-4-Resource-Loader-and-Packaging.md)

### Application structure

6. [Phase 5 — Startup and Bootstrap Rewrite](qt-rewrite-heavy-route-plan/06-Phase-5-Startup-and-Bootstrap-Rewrite.md)
7. [Phase 6 — Shared UI Rewrite](qt-rewrite-heavy-route-plan/07-Phase-6-Shared-UI-Rewrite.md)
8. [Phase 7 — Feature Migration](qt-rewrite-heavy-route-plan/08-Phase-7-Feature-Migration.md)

### Platform, quality, and release

9. [Phase 8 — Platform and Output Adapters](qt-rewrite-heavy-route-plan/09-Phase-8-Platform-and-Output-Adapters.md)
10. [Phase 9 — Windows Memory Hardening](qt-rewrite-heavy-route-plan/10-Phase-9-Windows-Memory-Hardening.md)
11. [Phase 10 — Visual, Behavioral, and Cross-Platform Parity](qt-rewrite-heavy-route-plan/11-Phase-10-Visual-Behavioral-and-Cross-Platform-Parity.md)
12. [Phase 11 — Test Restructuring and Release Gates](qt-rewrite-heavy-route-plan/12-Phase-11-Test-Restructuring-and-Release-Gates.md)
13. [Phase 12 — Beta, Cutover, and Legacy Removal](qt-rewrite-heavy-route-plan/13-Phase-12-Beta-Cutover-and-Legacy-Removal.md)
14. [Phase 13 — Post-Release Maintenance](qt-rewrite-heavy-route-plan/14-Phase-13-Post-Release-Maintenance.md)

## Feature subphases

Phase 7 contains the complete vertical-slice migration sequence:

- Setup and workspace/file workflows.
- My Workspace and personal information.
- Teachers and staff.
- Classes.
- Schedule and import workflows.
- Calendar.
- Rosters.
- Speaking evaluations.
- Campus and documents.
- Substitute preparation and output.

Their individual status is tracked in the Phase 7 file and the collective phase status is tracked in Start Here.

## Critical path

    Phase 0
      ↓
    Build boundaries
      ↓
    Domain contracts ─────┐
      ↓                   │
    Persistence ──────────┼──→ Bootstrap and shell
      ↓                   │          ↓
    Resource loader ──────┘     Shared UI lifecycle
                                      ↓
      Setup/workspace → Teachers → Classes
                                      ↓
                         Schedule and Calendar
                                      ↓
                              Rosters and Speaking
                                      ↓
                      Campus/Documents/Sub Prep/Output
                                      ↓
                         Memory hardening and parity
                                      ↓
                                  Cutover

## Global completion criteria

The rewrite is complete only when:

- Every current feature works in v2.
- Supported .tps files continue to work.
- Legacy .db import continues to work.
- Generated PDFs, printouts, rosters, reports, substitute documents, and PowerPoint output match the baseline.
- English and Korean behavior match.
- Light and dark themes match.
- Menus, shortcuts, navigation, permissions, and dialogs match.
- The splash screen is gone from every startup path.
- No resource packs are mounted or downloaded.
- No separate resource update request exists.
- Application updates still work and include canonical resources.
- Large resources load on demand.
- Resource caches have explicit budgets.
- Windows packaged Release startup memory is below 250 MiB.
- Normal navigation does not produce unbounded memory growth.
- macOS and Linux do not regress materially.
- CI verifies build, test, packaging, visual parity, and memory gates.
- Legacy compatibility code has been deleted rather than merely bypassed.
