# Phase 10 — Visual, Behavioral, and Cross-Platform Parity

Read [00-Start-Here.md](00-Start-Here.md) first for the overall plan, workflow, and phase sequence.

## Mandatory route for new and resumed work

- Use the Heavy route for every task, slice, follow-up, and reopened gate in this phase. `Default route: Heavy` is a binding instruction.
- At each start or resume, explicitly select Heavy and reread this phase plan and [00-Start-Here.md](00-Start-Here.md). Follow the active `AGENTS.md` Heavy-route instructions and `~/.codex/codex_workflow/heavy_route.md`, including their delegation, verification, and deployment-state requirements.
- A new session, handoff, or context reset does not change the route. Do not continue under Light or Medium. If a required Heavy-route step blocks progress, report the blocker before implementation instead of silently switching routes.

## Status

- Status: Not started
- Default route: Heavy
- Depends on: Phases 7 through 9
- Blocks: Final release qualification
- Owner: Unassigned
- Last updated: 2026-09-16
- Current note: Prove that the rewrite preserved the product, not merely that it compiles.

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

Demonstrate that v2 preserves the current user experience and output across supported platforms while meeting the Windows memory target.

## Memory-safe parity validation

The [Qt Rewrite Memory Hotspot Remediation
Plan](memory-hotspot-remediation-plan.md) changes ownership and allocation,
not the product surface. Parity review must therefore include the large-data
workspace and confirm that model/view delegates, lazy class content, bounded
import review, and chunked output preserve the same visible controls,
ordering, typography, localized text, and generated content.

Do not relax visual comparisons by reducing the large fixture, hiding
records, removing output states, or replacing required assets. Any memory
tradeoff that changes the user-visible result is a parity defect requiring an
owning feature decision.

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

- For every Phase 10 slice, use the heavy route: compare the complete slice
  across the required visual, behavioral, output, and platform dimensions, then
  resolve the owning implementation rather than masking the difference.
- Do not accept compilation as parity.
- Do not replace visual assets to hide memory usage.
- Do not approve platform-specific behavior without a documented reason.
- Do not make a broad redesign while validating a rewrite.
