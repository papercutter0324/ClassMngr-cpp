# Phase 9 — Windows Memory Hardening

## Status

- Status: Not started
- Default route: Heavy
- Depends on: Phases 4 through 8
- Blocks: Release qualification and cutover
- Owner: Unassigned
- Last updated: 2026-09-16
- Current note: The less-than-250-MiB Windows target is a hard release gate.

## Objective

Reduce and control resident memory through explicit ownership, lazy loading, bounded caches, compact models, and release verification.

## Measurement contract

Primary metric:

- Windows working set of a packaged Release build.

Secondary metrics:

- Private bytes.
- Commit size.
- Peak working set.
- Handle count.
- Thread count.

Measure:

- Empty workspace launch.
- Representative workspace launch.
- Startup-ready.
- First page render.
- Five minutes idle.
- Normal navigation.
- Large schedule import.
- Large roster editing.
- Campus map open and close.
- Document catalog startup with no loaded PDF body.
- QtPdf document open, render, close, release, and reopen.
- Speaking report generation.
- PDF preview and close.
- Repeated workspace open and close.
- Repeated language and theme changes.

The final normal-use gate is less than 250 MiB working set at startup-ready and after normal navigation and idle. Heavy export operations may have a separate transient budget, but they must return toward the baseline after completion.

## Main allocation sources to investigate

### 9.1 Resources

- Large embedded resource data.
- Duplicate raw and decoded resources.
- Loading all fonts at startup.
- Full document bodies or rendered QtPdf pages loaded for the catalog or a
  hidden viewer.
- Full-resolution campus maps.
- Report artwork retained after export.
- QML assets constructed before calendar entry.
- Caches without byte budgets.

### 9.2 UI objects

- QTableWidgetItem per-cell allocation.
- Cell widgets and editors retained for every table entry.
- Hidden pages constructed at startup.
- Nested class sections retaining full models.
- Duplicate table models and service collections.
- Persistent dialogs or previews holding large objects.

### 9.3 Application and persistence

- DataService and feature-service duplicate data.
- Broad queries returning unused columns or relationships.
- Copied Qt containers across service boundaries.
- Import workbooks retained after planning.
- PDF and PowerPoint object lifetime.
- Reopened database sessions retaining old repositories.

### 9.4 Windows build and deployment

Verify:

- Packaged Release rather than Debug.
- Release Qt libraries.
- Release CRT configuration.
- Correct architecture.
- No development-only plugins or assets.
- No duplicated resource tree.
- No test fixtures deployed into the application.

## Required instrumentation

ResourceDiagnostics and application diagnostics must report:

- Process memory at named lifecycle checkpoints.
- Page creation and destruction.
- Model row counts.
- Table-item and cell-editor counts.
- Image dimensions and decoded bytes.
- Font load events.
- Resource-cache size and owners.
- QtPdf load/status/page/render/close/release and PDF/export object lifetimes.
- Database result sizes.
- Repeated-navigation growth.

Use Windows-native allocation and process tools to validate application-level measurements. Reports must distinguish working set from private bytes and peak values.

## Required fixes

1. Remove runtime resource packs and large embedded blobs.
2. Load only required UI fonts at startup.
3. Load decorative fonts on demand.
4. Prevent hidden page construction.
5. Replace large table widgets with model/view and delegates.
6. Remove duplicate service and UI collections.
7. Keep catalog metadata resident without caching full PDF bodies; bound image,
   rendered-page, document, map, report, and template caches.
8. Release feature resources on page suspension or release.
9. Decode images at bounded display resolution.
10. Stream large documents and exports where possible.
11. Avoid holding raw bytes and decoded objects longer than necessary.
12. Narrow Qt dependencies in non-UI targets.
13. Ensure no debug Qt or debug CRT is used in packaged Release builds.
14. Add repeated-open and repeated-navigation regression tests.

### Sub Prep large-route gate

The [Sub Prep Class Information and Output Memory
Plan](sub-prep-class-information-memory-plan.md) is a required Phase 9
consumer of this instrumentation. The packaged Release workflow must measure
the 96-class / 8-slot fixture through Sub Prep entry, summary-model creation,
first-detail selection, refresh, package generation, page leave, and repeated
enter/leave cycles. The gate must report bounded class-information widget and
editor counts, peak and settled memory, and memory return after release.

The bounded full-route fixture remains a fast lifecycle/parity check; it cannot
substitute for the large-fixture stress gate.

## Memory budget tests

Add automated tests for:

- Cold launch.
- Representative launch.
- Repeated open and close.
- Repeated navigation.
- Theme changes.
- Language changes.
- Large schedule import.
- Large roster editing.
- Speaking report generation.
- PDF preview and close.
- Campus map open and close.
- Startup with a populated document catalog and no loaded PDF.
- Repeated QtPdf document open, close, release, and reopen.

Each repeated test must specify:

- Starting memory.
- Maximum allowed growth.
- Cleanup event.
- Expected return range.
- Failure artifact.

## Deliverables

- Windows memory baseline and trend report.
- Resource-level memory trace.
- Page and model lifetime trace.
- Bounded caches.
- Model/view replacements for priority tables.
- Release-package validation.
- Automated memory regression tests.

## Exit gate

Windows packaged Release startup-ready memory is below 250 MiB working set for both the empty and representative workspaces.

Normal feature navigation does not create unbounded growth; startup has no
loaded catalog PDF, and large features release their resources after leaving,
including the active QtPdf document.

## Heavy-route requirements

- For every Phase 9 slice, use the heavy route: fix the owning lifecycle or
  allocation boundary, measure the complete affected workflow, and remove any
  temporary mitigation before accepting the slice.
- Do not meet the target by disabling features.
- Do not meet the target by reducing required visual assets or font quality.
- Do not use only binary size as a memory proxy.
- Do not accept a single successful launch as proof.
- Do not defer memory cleanup to a future release.
