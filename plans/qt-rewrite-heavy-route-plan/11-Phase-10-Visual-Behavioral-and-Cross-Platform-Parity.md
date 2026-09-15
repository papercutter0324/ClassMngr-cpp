# Phase 10 — Visual, Behavioral, and Cross-Platform Parity

## Status

- Status: Not started
- Default route: Heavy
- Depends on: Phases 7 through 9
- Blocks: Final release qualification
- Owner: Unassigned
- Last updated: 2026-09-16
- Current note: Prove that the rewrite preserved the product, not merely that it compiles.

## Objective

Demonstrate that v2 preserves the current user experience and output across supported platforms while meeting the Windows memory target.

## Visual parity

For every migrated page and dialog, compare:

- Layout geometry.
- Window and splitter behavior.
- Fonts and text metrics.
- Colors and palettes.
- Icons.
- Spacing.
- Selection states.
- Hover states.
- Focus states.
- Disabled states.
- Table headers and delegates.
- Empty states.
- Error and warning states.
- Loading states.
- Print previews.

Compare:

- English.
- Korean.
- Light theme.
- Dark theme.
- Empty workspace.
- Representative workspace.
- Large-data workspace.

Use screenshot comparisons with a documented tolerance for platform rendering differences. Any tolerance must not hide changed layout, missing controls, or incorrect font selection.

## Behavioral parity

Verify:

- Menus.
- Actions.
- Keyboard shortcuts.
- Context menus.
- Enabled states.
- Permission checks.
- Unsaved-change prompts.
- Import matching.
- Conflict resolution.
- Validation.
- Error recovery.
- Undo and redo where supported.
- File dialogs.
- Recent files.
- Save and save-as.
- Backup behavior.
- External URL behavior.
- Application updates.
- Document catalog metadata-only startup, on-demand QtPdf open/loading/error
  behavior, close/release, and reopen behavior.

## Output parity

Compare:

- PDFs.
- Print previews.
- Printed pages.
- Rosters.
- Speaking-evaluation reports.
- Substitute documents.
- PowerPoint output.
- Exported data.

Use content comparisons as well as visual comparisons. A PDF that looks similar but loses data is not acceptable.

## Platform matrix

Run the parity suite on:

- Windows x64.
- Windows ARM64.
- macOS universal.
- Linux GCC build.

Verify:

- Installation.
- Resource paths.
- Translation loading.
- Font loading.
- File dialogs.
- Printing.
- URL launching.
- PowerPoint availability behavior.
- Application updater behavior.
- Window state persistence.
- Memory metrics.

## Deliverables

- Visual regression suite.
- Behavioral parity suite.
- Output comparison suite.
- Cross-platform test report.
- Documented platform rendering tolerances.
- Open issue list for every difference.

## Exit gate

All differences from v1 are either eliminated or explicitly approved as intentional product changes.

No required feature, workflow, output, language, theme, or platform behavior is missing.

## Heavy-route requirements

- Do not accept compilation as parity.
- Do not replace visual assets to hide memory usage.
- Do not approve platform-specific behavior without a documented reason.
- Do not make a broad redesign while validating a rewrite.
