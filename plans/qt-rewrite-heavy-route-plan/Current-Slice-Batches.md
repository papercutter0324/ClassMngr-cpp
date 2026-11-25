# Current Slice Batches

## Phase 10 — Batch 1: Visual, behavioral, and cross-platform parity (Paused after Candidate 14)

Batch goal: requalify migrated behavior, appearance, and outputs against the
legacy product on supported platforms using current-source evidence. Record
source, toolchain, fixture, comparator version, and raw artifacts. The
candidates below are ordered discovery targets, not confirmed current-source
defects. Use the existing output comparators where applicable; keep the
Sub Prep W7.5.1 page-break requalification distinct from the broader output
matrix. Search remaining migrated product surfaces during next-batch discovery.

### Candidate status — 2026-11-25

Phase 10 remains In Progress and its cross-platform exit gate remains open.
“Linux complete” below records this host's bounded evidence; it does not close
the cross-platform Phase 10 requirement.

| Candidate | Status |
| --- | --- |
| 1. Focused Phase 7 verification | Linux focused subset passed 19/19; full Phase 7 was not rerun by scope. |
| 2. Shell command parity | Focused Linux checks complete; other-host parity remains open. |
| 3. Speaking Evaluation redo | Focused Linux checks complete; other-host parity remains open. |
| 4. Sub Prep page-break | Current-source Linux C53 pair passed at 12 pages each; page-break/product disposition remains open. |
| 5. Output matrix | Linux matrix complete; Windows x64/ARM64 and macOS remain open. |
| 6. Native printing | Linux dialog/PDF checks complete; desktop UI, physical printer, Windows/macOS/ARM64 remain open. |
| 7. PowerPoint automation | Linux service/fallback checks complete; live Office/COM/PowerPoint hosts remain open. |
| 8. Updater | Linux focused checks complete; Windows/macOS branches remain open. |
| 9. Native file workflows | Linux fix and checks complete; native OS pickers and platform-specific paths remain open. |
| 10. External launches | Linux normalized-path fix and checks complete; native OS handlers and other hosts remain open. |
| 11. Installed runtime | Partial Linux evidence; a full installed Next application/runtime was not staged. Cross-platform remains open. |
| 12. My Classes visuals | Linux disposition complete; empty-state/forced-overflow captures and Windows/macOS remain open. |
| 13. Schedule display | Linux disposition complete; cross-platform rendering and interactive hosts remain open. |
| 14. Schedule Import preview | Linux disposition complete; Windows/macOS and live tooltip/screen-reader behavior remain open. |
| 15. Teacher Profile | Not started. |
| 16. My Details and Signature | Not started. |
| 17. Staff Directory | Not started. |
| 18. Upcoming Birthdays | Not started. |
| 19. My Workspace Calendar | Not started; beginning this candidate triggers next-batch discovery. |
| 20. Classes states/localization | Not started. |

### Ordered candidates

1. **Focused Linux verification of migrated Phase 7 behavior** — complete the
   Phase 7.5 transfer only; do not rerun the full Phase 7 suite.
2. **Application shell command parity** — compare legacy Exit, Undo/Redo,
   Cut/Copy/Paste, and About with the Next shell; verify shortcuts and enabled
   behavior and confirm the product contract. Compare
   src/app/menu_builder.cpp with src/next/ui/application_shell.cpp.
3. **Speaking Evaluation redo parity** — verify the redo control and shortcut
   against behavior and tests in src/next/ui/speaking_evaluation_page.cpp and
   tests/next/speaking_evaluation_page_tests.cpp.
4. **Sub Prep W7.5.1 page-break output requalification** — capture current
   source-bound output and resolve the Teachers 22–24 difference; prior
   evidence is not source-bound.
5. **Current-source output comparison matrix** — use existing comparators for
   Schedule, Roster, Speaking reports, and Sub Prep C53 across platforms;
   record source, toolchain, fixture, and raw artifacts without duplicating 4.
6. **Native printer and print-preview parity** — compare the complete native
   print and preview workflows across supported platforms.
7. **PowerPoint automation behavior** — verify automation availability and
   fallback behavior on supported hosts.
8. **Updater runtime behavior** — verify installer/folder actions and
   platform-specific branches.
9. **Native file workflows** — compare file dialogs, Save As, recent files,
   and output-folder selection.
10. **External launch behavior** — compare external URL and folder launches
    across platforms.
11. **Installed-runtime matrix** — verify resource paths, translations, font
    loading, and window-state persistence from installed builds.
12. **My Classes visual disposition** — review documented layout and
    typography differences with fresh source-bound captures.
13. **Schedule display visual disposition** — review display toolbar,
    typography, wrapping/elision, and overflow indicators.
14. **Schedule Import preview disposition** — review normal preview clipping
    and conflict-status detail behavior.
15. **Teacher Profile visual disposition** — review residual geometry and the
    birthday-clear affordance.
16. **My Details and Signature capture review** — complete missing locale/theme
    captures and review the accepted vertical reveal.
17. **Staff Directory geometry review** — compare table height and geometry
    across current modes and themes.
18. **Upcoming Birthdays missing captures** — capture English/dark and
    Korean/light for populated and empty states.
19. **My Workspace Calendar disposition** — capture lower Upcoming content
    and disposition the reported Korean shell-label difference.
20. **Classes state and localization disposition** — capture full-window
    empty, delete, read-error, modal-inline, and localization states.

Begin next-batch discovery when starting candidate 19. Search remaining
migrated product surfaces and any candidates beyond this 20-item limit at that
time. Historical visual findings under
docs/qt-rewrite/visual-baseline/next/phase-7/ and behavior/output evidence in
the Phase 7, 7.5, 8, and 8a progress logs are references for discovery only;
requalify against current source before calling a difference a defect.
