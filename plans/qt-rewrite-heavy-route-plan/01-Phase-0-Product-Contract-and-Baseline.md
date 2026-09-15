# Phase 0 — Product Contract, Source Archaeology, and Baseline

## Status

- Status: In progress
- Default route: Heavy
- Depends on: None
- Blocks: Every implementation phase
- Owner: Unassigned
- Last updated: 2026-09-15
- Current note: Static archaeology is recorded for commit `75755460`; runtime,
  visual, fixture, and packaged-release evidence is still being collected. A
  clean Windows x64 Debug/Ninja build and startup test now pass; the packaged
  Release baseline and the remaining fixture/platform evidence are still open.

## Progress update - 2026-09-15

- What changed: added the initial Phase 0 evidence set in
  `docs/qt-rewrite/` covering source ownership, features, file formats,
  resources, baseline commands, and risks.
- What remains: capture screenshots and generated-output references, create
  representative fixtures, run clean Windows x64/ARM64, macOS universal, and
  Linux Release baselines, and record startup/resource traces.
- Evidence: source snapshot `75755460`; raw resource inventory is 226 files
  totaling 65,729,685 bytes; source/test inventory and the current CTest
  enumeration are recorded in the evidence documents.
- Risk: existing build directories include stale configuration from an earlier
  tree, so they are excluded from acceptance evidence until reconfigured.

## Progress update - 2026-09-15 (fixture and clean build)

- What changed: replaced the deleted historical `Testing-copy.tps` dependency
  with the reproducible fixture `tests/fixtures/workspaces/representative_startup.sql`;
  the startup test now materializes it through `DatabaseSchemaManager`.
- Evidence: clean Windows x64 Ninja Debug configuration and a 352-step focused
  target build passed; `ClassMngrStartupPerformanceTests -v1` passed both tests
  with exit code `0`.
- Preliminary measurement: the representative run reported a peak working set
  of `202,162,176` bytes at startup-complete and `221,011,968` bytes at the
  five-second settled checkpoint in a Debug/offscreen process. The exact byte
  values and scenario are recorded in `docs/qt-rewrite/phase-0-baseline.md`;
  this is not packaged Release acceptance evidence.
- What remains: add the other Phase 0 fixtures, capture visual and generated
  output references, trace startup/resource ownership, and run packaged Release
  baselines on every target platform.
- Risk: Qt 6.12 QML import scanning makes clean configuration slow, and the
  older Visual Studio build directory still loops during regeneration.

## Evidence files

- [Source archaeology](../../docs/qt-rewrite/phase-0-source-archaeology.md)
- [Feature preservation matrix](../../docs/qt-rewrite/phase-0-feature-preservation-matrix.md)
- [File compatibility matrix](../../docs/qt-rewrite/phase-0-file-compatibility-matrix.md)
- [Resource and ownership inventory](../../docs/qt-rewrite/phase-0-resource-and-ownership-inventory.md)
- [Baseline and evidence log](../../docs/qt-rewrite/phase-0-baseline.md)
- [Risk register](../../docs/qt-rewrite/phase-0-risk-register.md)

## Objective

Create a reliable description of current behavior, visual appearance, file compatibility, startup work, resource usage, and memory usage.

## Work packages

### 0.1 Feature inventory

Inventory the current source, menus, actions, sidebar, setup wizard, dialogs, services, controllers, and tests.

Record:

- Every top-level page.
- Every nested class, calendar, workspace, roster, and setup page.
- Every menu and action.
- Every keyboard shortcut.
- Every context menu.
- Every enabled-state and permission rule.
- Every admin and developer path.
- Every import, export, print, report, and external-application workflow.
- Every user-facing setting.

### 0.2 Visual contract

Capture reference images for:

- English and Korean.
- Light and dark themes.
- Empty and populated pages.
- Editing and read-only pages.
- Loading, error, warning, and confirmation states.
- Import review and conflict resolution.
- Initial setup.
- Print previews.
- Generated documents, reports, and rosters.

Record window geometry, splitter positions, table column widths, font sizes, icon sizes, and important spacing values where they are user-configurable.

### 0.3 Data and file contract

Create fixture workspaces for:

- Empty data.
- Small normal data.
- Large realistic data.
- Legacy database import.
- Large schedules.
- Large rosters.
- Large speaking-evaluation datasets.
- Multiple campuses and document catalogs.

Document behavior for:

- .tps creation and opening.
- Legacy .db import.
- Save, save-as, export, and backup.
- Class transfer files.
- Schedule, teacher, calendar, and roster imports.
- Partial and failed imports.
- Corrupt or locked databases.

### 0.4 Packaged startup baseline

Build packaged Release artifacts for:

- Windows x64.
- Windows ARM64.
- macOS universal.
- Linux.

Do not use a debugger, development Qt installation, or profiler for the primary memory report.

Measure:

1. Process creation.
2. QApplication creation.
3. Resource initialization.
4. Settings and language load.
5. Theme and font application.
6. Main window construction.
7. Main window shown.
8. Initial page rendered.
9. Startup-ready.
10. Five minutes idle.
11. Each large feature entered.
12. Each large feature left.
13. Heavy output completed.

On Windows record working set, private bytes, commit, peak working set, handle count, and thread count. Record equivalent resident and private metrics on macOS and Linux.

### 0.5 Startup and resource trace

Trace:

- Page construction.
- Service construction.
- Database connections and query sizes.
- Resource-pack initialization.
- Font loading.
- Translation loading.
- Stylesheet loading.
- Image decoding.
- PDF construction.
- Table item and cell-widget creation.
- Document catalog creation.
- Update checks.

Catalogue resources by:

- Installed size.
- Decoded or resident size.
- Startup necessity.
- Owning feature.
- Expected lifetime.
- Whether the resource can be streamed.

### 0.6 Compatibility and risk register

Create a risk register for:

- Database migration.
- File-format compatibility.
- Print and PDF differences.
- PowerPoint automation.
- Korean input and localization.
- Font metrics.
- QML calendar behavior.
- Application update packaging.
- Existing installed resource-pack content.
- Windows-only memory behavior.

## Deliverables

- Feature preservation matrix.
- Visual reference set.
- File-format compatibility matrix.
- Startup timeline.
- Per-platform memory report.
- Resource inventory.
- Ownership and cache inventory.
- Compatibility-path inventory.
- Risk register.
- Baseline test commands and expected results.

## Exit gate

The team can answer what every user-facing feature does, which resources it needs, which files it reads and writes, and how much memory each major workflow consumes.

No v2 feature work begins without a fixture and an acceptance check.

## Heavy-route requirements

- Use real packaged Release builds.
- Capture the worst representative workspace, not only an empty database.
- Measure actual Windows Task Manager-visible memory and private allocation.
- Keep screenshots and output files as permanent parity artifacts.
- Do not rely on prior plan documents or stale build-tree test inventories.
