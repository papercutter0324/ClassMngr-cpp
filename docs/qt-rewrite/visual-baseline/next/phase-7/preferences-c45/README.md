# Next Preferences C45 Review

Production `Edit → Preferences…` opens a Next-owned dialog with **General**,
**Appearance**, **Documents**, and **AI Comments** tabs. Schedule, Calendar, and
Navigation preferences remain on their page surfaces; Admin remains separate.
The [dialog](../../../../../../src/next/ui/preferences_dialog.cpp) and
[production shell route](../../../../../../src/next/ui/application_shell.cpp)
own this UI.

Visible labels are `Mode:`, `Automatically Check for Updates`, `Show Sidebar
Tooltips`, `Animate Overflowing Sidebar Text`, `Workbook Timeout:`, `Theme:`,
`Language:`, `Font Size:`, `PDF Page Spacing:`, `Background Color:`, `Preferred
AI Website:`, and `Comment Voice:`. The four tabs and their field/option labels
retranslate live; both Custom Website summaries are localized. macOS also shows
`Show Data Access Notice Before Export`.

The production route edits preferences, saves, reopens the dialog, and checks
persistence. It applies theme, language, font, sidebar, and active-page save-mode
effects; it also checks document and AI failure rollback, cancellation, and
timeout. A production Classes Navigator route switches English → Korean →
English and verifies the sidebar, page title, Schedule label, and persistence.
With automatic startup checks disabled, the result is `NotEligible` with zero
updater calls; enabled yields `Started` with one call. There is no
user-initiated updater. The macOS PowerPoint notice UI and preferences port ran
on this host. See the [production route and capture tests](../../../../../../tests/next/application_bootstrap_tests.cpp).

## Visual captures

Four production-menu captures show the Appearance tab at 680 × 620 px,
`QT_SCALE_FACTOR=1` (Qt offscreen). Fresh captures byte-match these packaged
PNGs. Review found translated labels, light/dark theme contrast, and no clipping;
the Normal font-size option and Close button are visible in each capture.

| Locale | Theme | Capture | Review |
| --- | --- | --- | --- |
| English | Light | [english-light.png](english-light.png) | Light controls and English labels visible. |
| English | Dark | [english-dark.png](english-dark.png) | Dark controls and English labels visible. |
| Korean | Light | [korean-light.png](korean-light.png) | Light controls and Korean labels visible. |
| Korean | Dark | [korean-dark.png](korean-dark.png) | Dark controls and Korean labels visible. |

## Focused verification

- Production menu/capture suite: 7 Qt cases; Classes Navigator language route:
  4 Qt cases.
- Five page/regression CTests: 5/5. Focused preference, startup, port,
  presenter, and PowerPoint checks: 22/22. This group passed after local loopback
  access was enabled; the default sandbox returned `EPERM` for one controller
  suite.
- Startup Performance and Phase 7 blank-fixture targets both ran. Resource
  inventory `--check` passed with 194 resources (187 direct, 7 generated);
  `git diff --check` passed.
- The Calendar dependency correction is test-target-only:
  `ClassMngrNextCalendarPageTests` adds workspace/Sub Prep compatibility objects
  and links `ClassMngrSubPrepShared`; it does not edit Gateway code. See
  [Next test CMake](../../../../../../cmake/tests/next.cmake).

These are focused acceptance results. The full project suite, Windows run, and
Phase 9 gates were not completed; this review does not claim Phase 7 completion.
