# Current Slice Batches

## Phase 10 — Batch 1: Visual, behavioral, and cross-platform parity (Active)

Batch goal: requalify migrated behavior, appearance, and outputs against the
legacy product on supported platforms using current-source evidence. Record
source, toolchain, fixture, comparator version, and raw artifacts. The
candidates below are ordered discovery targets, not confirmed current-source
defects. Use the existing output comparators where applicable; keep the
Sub Prep W7.5.1 page-break requalification distinct from the broader output
matrix. Search remaining migrated product surfaces during next-batch discovery.

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
