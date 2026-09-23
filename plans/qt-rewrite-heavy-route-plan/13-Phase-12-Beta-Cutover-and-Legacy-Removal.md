# Phase 12 — Beta, Cutover, and Legacy Removal

## Status

- Status: Not started
- Default route: Heavy
- Depends on: Phases 10 and 11
- Blocks: Post-release maintenance
- Owner: Unassigned
- Last updated: 2026-09-16
- Current note: Cut over only after parity, compatibility, packaging, and memory gates pass.

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

Release v2 safely, preserve rollback ability, then delete the legacy architecture rather than leaving two permanent applications.

## Work packages

### 12.1 Beta package

Produce packaged v2 builds for:

- Windows x64.
- Windows ARM64.
- macOS universal.
- Linux.

The package must contain the executable, Qt deployment, platform plugins, QML if retained, canonical resources, licenses, and metadata.

### 12.2 Dual-run comparison

Run v1 and v2 against the same fixtures and compare:

- Database results.
- Saved files.
- Exported data.
- Generated PDFs.
- Print previews.
- Rosters.
- Speaking reports.
- Substitute documents.
- PowerPoint output.
- Screenshots.
- Startup timing.
- Memory measurements.
- Document-catalog startup, on-demand QtPdf open/close/reopen, and release
  behavior.

### 12.3 Upgrade testing

Test upgrades from representative existing installations:

- Existing .tps files.
- Legacy .db files.
- Existing settings.
- Existing recent files.
- Existing language and theme choices.
- Existing application-update settings.
- Existing old resource-pack directories.

V2 must ignore old resource packs safely and must use its installed canonical resources.

### 12.4 Rollback

Define:

- Beta backup procedure.
- Rollback executable/package.
- Database backup location.
- Failure-report collection.
- User-facing recovery instructions.

Do not make the first v2 release depend on an irreversible database conversion without a tested backup and rollback path.

### 12.5 Cutover

After all gates pass:

1. Make v2 the normal executable and application target.
2. Keep legacy .db import.
3. Keep .tps compatibility.
4. Keep normal application updates.
5. Remove the splash screen from every startup path.
6. Remove resource-pack code, resources, options, and updater.
7. Remove DataService and feature-service fallback paths.
8. Remove obsolete page/controller infrastructure.
9. Remove unused Qt modules and deployment files.
10. Update installers and packaged resource paths.
11. Update documentation and support procedures.
12. Keep v1 available through the agreed rollback window.

As part of legacy removal, delete the temporary startup/performance dialog
compatibility driver, prompt-inspection hooks, and any legacy prompt-ID or
object-name shims that have no remaining v2 consumer. Retain only the Qt
dialog implementation required by the final presentation adapter, and keep
the dialog-policy gate passing without an allowlist entry for application
composition or feature code.

### 12.6 Safe cleanup

Do not delete:

- User databases.
- User documents.
- User exports.
- User-created templates.
- User settings unless a documented migration requires it.

Any removal of old application-owned resource-pack directories must:

- Target only the known application-owned directory.
- Verify canonical resources first.
- Be logged.
- Be reversible where practical.

### 12.7 Memory-path cleanup

After the [Qt Rewrite Memory Hotspot Remediation
Plan](memory-hotspot-remediation-plan.md) passes its Phase 9 and Phase 11
gates, remove the obsolete allocation paths:

- per-class widget-tree and all-class tab builders;
- QTableWidget and cell-widget implementations replaced by model/view;
- broad workbook, compatibility-cell, and duplicate import representations;
- full-package transfer previews and duplicate dialog ownership;
- indefinite page retention and temporary release shims;
- feature-specific compatibility adapters with no remaining consumers.

Keep the large-fixture traces and accepted memory budgets in the cutover
report. Do not remove user files or compatibility formats while removing old
application-owned code paths.

## Deliverables

- Beta packages.
- Dual-run comparison report.
- Upgrade and rollback report.
- Cutover checklist.
- Updated installer.
- Updated documentation.
- Legacy deletion checklist.

## Exit gate

V2 is the default application, all release gates pass, rollback is available, and legacy code can be deleted without removing required user data or file compatibility.

## Heavy-route requirements

- For every Phase 12 slice, use the heavy route: complete the dual-run,
  migration, rollback, and release checks for that slice before cutover, then
  remove only the legacy paths proven obsolete by its evidence.
- Do not cut over because the new shell launches.
- Do not delete v1 before output and migration comparisons pass.
- Do not delete user data during resource cleanup.
- Do not leave resource-pack update code dormant in production.
- Do not leave DataService as an undocumented permanent fallback.
