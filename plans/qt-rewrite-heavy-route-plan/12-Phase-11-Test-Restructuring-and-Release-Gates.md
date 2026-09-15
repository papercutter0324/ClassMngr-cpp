# Phase 11 — Test Restructuring and Release Gates

## Status

- Status: Not started
- Default route: Heavy
- Depends on: Phases 0 through 10
- Blocks: Beta, cutover, and legacy removal
- Owner: Unassigned
- Last updated: 2026-09-16
- Current note: Convert the current test collection into permanent parity and regression gates.

## Objective

Build a test system that can prove data compatibility, visual parity, resource behavior, memory behavior, and safe release packaging.

## Test layers

### 11.1 Domain

- Value types.
- Validation rules.
- Schedule conflict rules.
- Import matching.
- Roster rules.
- Speaking-evaluation calculations.
- Calendar filtering.

### 11.2 Persistence

- Schema creation.
- Every migration.
- Constraints and foreign keys.
- Transactions.
- Save and save-as.
- Backup and recovery.
- Legacy .db import.
- .tps compatibility.
- Class-transfer compatibility.
- Corrupt and locked files.

### 11.3 Application

- Use cases.
- Structured errors.
- Unsaved state.
- Import jobs.
- Export jobs.
- Cancellation.
- Permission behavior.
- Update coordination.

### 11.4 Resources

- Catalog lookup.
- Missing resource behavior.
- Corrupt resource behavior.
- Type mismatch.
- Translation loading.
- Font loading.
- Image decoding.
- Stream loading.
- Document catalog metadata-only initialization and on-demand QtPdf content.
- QtPdf document close/release and viewer-session ownership.
- Cache budgets.
- Scope release.
- No resource update fallback.

### 11.5 UI

- View models.
- Navigation.
- Command enabled state.
- Dialog behavior.
- Theme and language switching.
- Page creation and release.
- Document viewer loading, error, close, release, and reopen behavior.
- Large-table model behavior.

### 11.6 Integration and output

- Schedule import.
- Calendar import.
- Teacher import.
- Roster import.
- Report generation.
- PDF viewer open/copy/save/print behavior.
- PDF generation.
- Printing.
- PowerPoint automation.
- File dialogs.
- Application update behavior.

### 11.7 Visual

- Screenshot comparisons.
- Theme comparisons.
- Language comparisons.
- Print preview comparisons.
- PDF render comparisons.

### 11.8 Performance and memory

- Startup checkpoints.
- Packaged Release memory.
- Repeated page navigation.
- Repeated workspace open and close.
- Large schedule.
- Large roster.
- Large document.
- Startup with a document catalog but no loaded PDF.
- Repeated QtPdf viewer open/close/release/reopen.
- Large report.
- Feature resource release.

## Failure injection

Test:

- Missing resources.
- Corrupt resources.
- Missing fonts.
- Invalid translations.
- Locked databases.
- Interrupted saves.
- Failed imports.
- Failed PDF generation.
- Missing or corrupt PDF when opened on demand.
- Missing PowerPoint.
- Network failure during application update checking.
- Cancellation during large operations.

Resource failures must report a useful error and must never trigger a resource-pack download attempt.

## Release gates

The release candidate must pass:

- Clean builds.
- All unit tests.
- All integration tests.
- Migration tests.
- Output comparisons.
- Visual comparisons.
- Localization tests.
- Cross-platform tests.
- Packaged deployment tests.
- Windows memory gates.
- Repeated-operation leak-growth gates.
- Startup PDF non-loading and viewer-session document-release gates.

## Deliverables

- Reorganized test targets.
- Parity fixtures.
- Visual baselines.
- Migration fixture set.
- Memory regression suite.
- Packaged Release CI gate.
- Release-candidate report template.

## Exit gate

CI can reject a release candidate for a functional regression, visual regression, file incompatibility, missing resource, packaging error, or Windows memory regression.

## Heavy-route requirements

- For every Phase 11 slice, use the heavy route: add the deterministic tests
  and release evidence for the complete affected behavior, enforce the gate in
  CI, and keep comparison coverage until the slice is accepted.
- Keep the old implementation available as a comparison oracle until cutover.
- Prefer deterministic fixture tests over manual verification.
- Test failure and recovery paths, not only successful paths.
- Fail the build when a gate is exceeded.
