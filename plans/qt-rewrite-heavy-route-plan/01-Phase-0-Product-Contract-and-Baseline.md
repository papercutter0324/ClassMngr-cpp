# Phase 0 — Product Contract, Source Archaeology, and Baseline

## Status

- Status: Complete
- Default route: Heavy
- Depends on: None
- Blocks: None; Phase 1 is unblocked
- Owner: Unassigned
- Last updated: 2026-09-19
- Historical progress log: [01-Phase-0-Progress-Log.md](01-Phase-0-Progress-Log.md)
- Current note: Phase 0 was declared complete by the user on 2026-09-18. Fresh
  packaged Windows x64 and macOS universal matrices pass all 24 routes each;
  combined `--require-exit-gate` validation using the Windows run root and both
  retained macOS archives passed. The user confirmed the retained visual
  references look correct. Linux now has supplemental opt-in automation, but
  its local run failed before startup in the sandbox and hosted evidence is
  pending; this does not alter the official gate or reopen Phase 0. Phase 1 is
  unblocked; details are in the baseline log.

### Phase 0 platform and route gate

- Required packaged Release evidence: Windows x64 and macOS universal.
- Windows ARM64 and Linux are outside the official gate and do not block
  Phase 0. Linux has a supplemental opt-in runner; see the [baseline
  evidence](../../docs/qt-rewrite/phase-0-baseline.md#supplemental-linux-phase-0-automation).
- The exit gate requires all 24 route IDs on each required platform. A valid
  subset per-run result is not gate completion; use `--require-exit-gate` with
  the Windows evidence root and `--macos-evidence-root` to enforce complete
  coverage. See the [runner and validator guide](../../scripts/phase0/README.md).

## Product decision update - 2026-09-16

- Documents displayed through the QtPdf viewer are on-demand content, not
  startup resources.
- Startup may load the document catalog, localized names, and validated asset
  references, but it must not load PDF bodies, render PDF pages, or call
  `QPdfDocument::load()` for catalog entries before the user requests a
  document.
- A requested document is owned by the active viewer session. Closing the
  viewer, replacing the document, leaving the viewer, or releasing the page
  must close the QtPdf document and release its document resource. Reopening a
  document may load it again.
- Generated and print-output PDFs remain separate operation-scoped resources;
  this decision does not require changing their output contract.
- The Windows x64 route matrix and packaged macOS universal baseline now record
  the startup negative case, on-demand document open, and release after viewer
  close. The macOS 24-route matrix remains 0/24 and blocks the combined gate;
  see the [baseline evidence](../../docs/qt-rewrite/phase-0-baseline.md).

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
- Document-catalog, document-loading, document-ready, document-error, and
  document-closed/reopened viewer states.
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
- Multiple campuses and document catalogs with representative PDF/PPTX assets.

Document behavior for:

- .tps creation and opening.
- Legacy .db import.
- Save, save-as, export, and backup.
- Class transfer files.
- Schedule, teacher, calendar, and roster imports.
- Partial and failed imports.
- Corrupt or locked databases.
- Document catalog metadata versus on-demand QtPdf content loading and release.

### 0.4 Packaged startup baseline

Build packaged Release artifacts for the required Phase 0 platforms:

- Windows x64.
- macOS universal.

Windows ARM64 and Linux are unofficial ports deferred to later work and are not
Phase 0 baseline requirements.

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
9. Startup-ready, including confirmation that no catalog PDF is loaded.
10. Five minutes idle with catalog metadata still resident only.
11. Each large feature entered.
12. A representative PDF opened, rendered, navigated, and closed/released.
13. Each large feature left, including the document viewer.
14. Heavy output completed.

On Windows x64 record working set, private bytes, commit, peak working set,
handle count, and thread count. Record equivalent resident and private metrics
on macOS universal. Windows ARM64 and Linux remain deferred ports.

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
- PDF construction, `QPdfDocument::load()`, page rendering, close, and release.
- Table item and cell-widget creation.
- Document catalog creation and metadata-only startup behavior.
- Document viewer content-open count, active-document count, and retained
  bytes across open/close/reopen.
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
- PDF viewer on-demand loading and release.
- PowerPoint automation.
- Korean input and localization.
- Font metrics.
- QML calendar behavior.
- Application update packaging.
- Existing installed resource-pack content.
- Windows-only memory behavior.

### 0.7 Cross-cutting memory hotspot baseline

Use the [Qt Rewrite Memory Hotspot Remediation
Plan](memory-hotspot-remediation-plan.md) to complete the before-state audit
for every identified large-data or widget-heavy workflow. In addition to the
startup baseline, capture the 96-class Sub Prep route, My Classes and Classes
navigation, schedule and calendar imports, schedule-table refresh, speaking
batch review, class transfer, staff directory, PDF/report output, and
workspace page entry/leave.

Each scenario must record its fixture, lifecycle checkpoints, working set,
private bytes, peak values, page/model/widget counts, and process outcome.
Keep the large fixtures as required stress inputs and record normal resident
memory separately from transient operation peaks.

## Deliverables

- Feature preservation matrix.
- Visual reference set.
- File-format compatibility matrix.
- Startup timeline.
- Per-platform memory report.
- Resource inventory.
- Ownership and cache inventory.
- Document viewer loading and release contract with runtime evidence.
- Compatibility-path inventory.
- Risk register.
- Baseline test commands and expected results.

## Exit gate

The team can answer what every user-facing feature does, which resources it needs, which files it reads and writes, and how much memory each major workflow consumes.

The evidence must also prove that opening the application does not load a PDF
into QtPdf, while an explicitly requested document loads successfully and is
released when its viewer session ends.

No v2 feature work begins without a fixture and an acceptance check.

## Heavy-route requirements

- For every Phase 0 slice, use the heavy route: make the evidence slice
  explicit, end to end, and acceptance-ready; keep legacy behavior only as a
  comparison oracle or an explicitly temporary bridge with a removal point.
- Use real packaged Release builds.
- Capture the worst representative workspace, not only an empty database.
- Measure actual Windows Task Manager-visible memory and private allocation.
- Keep screenshots and output files as permanent parity artifacts.
- Treat PDF viewer content as session-scoped; do not preload or retain full
  document bodies merely to populate the catalog.
- Do not rely on prior plan documents or stale build-tree test inventories.

## Supplemental Linux automation - 2026-09-19

The Linux x86_64 packaged Release runner, validator support, and manual
workflow are available for informational evidence. The only local run built
the package and Debug harness but failed all 24 routes before application
startup because sandbox Xvfb could not create `/tmp/.X11-unix`; the hosted run
is pending. The official Windows x64/macOS universal gate remains complete.
