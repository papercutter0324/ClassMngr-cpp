# Phase 9 — Windows Memory Hardening

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

- Status: Not started
- Default route: Heavy
- Depends on: Phases 4 through 8
- Blocks: Release qualification and cutover
- Owner: Unassigned
- Last updated: 2026-09-16
- Current note: The less-than-250-MiB Windows target is a hard release gate.

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

Reduce and control resident memory through explicit ownership, lazy loading, bounded caches, compact models, and release verification.

## Remediation-plan ownership

The [Qt Rewrite Memory Hotspot Remediation
Plan](memory-hotspot-remediation-plan.md) is the implementation sequence for
the allocation sources and required fixes below. Phase 9 owns the common
instrumentation, budgets, packaged Release qualification, and rejection of
partial fixes; the feature phases own the underlying data and UI changes.

Every hotspot must show both a bounded peak and cleanup after its owning page
or operation ends. Successful process termination alone is not sufficient.

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
- My Classes and Classes navigation.
- Large calendar import.
- Staff directory and AI batch review.
- Class-transfer export and import.
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
