# Year-to-Date Graph in Class Analytics — Implementation Plan

## Objective

Extend the **Class Shape** card on the Class Analytics page with a compact
**Year to Date Class Average** trend graph that shows the class's performance
at each completed speaking evaluation. This is a line/point chart, not a
histogram: a histogram remains the appropriate visualization for the existing
per-student grade distribution. Also make that existing grade-distribution
histogram explicitly identify the evaluation it represents:

- when the Evaluation selector is set to **All**, show the most recently
  completed evaluation;
- otherwise, show the evaluation selected in the selector.

The label and the histogram data must always refer to the same evaluation.

## Confirmed Behavior and Assumptions

- The app's canonical chronological order is the order returned by
  `SpeakingAnalytics::evaluationNames()`:
  `Winter`, `Speech Contest`, `Summer`, `Fall`.
- A YTD point is eligible only when the evaluation contains at least one
  **fully scored** student (all six rubric criteria scored). Blank matrices and
  students with only partial criterion scores do not contribute to that
  point. Until an explicit evaluation-finalization state exists, this is the
  practical definition of a completed/usable evaluation for the trend.
- The YTD series contains one point per eligible evaluation in canonical
  order. Its value is calculated by first averaging the six rubric scores for
  every fully scored student, then taking the equally weighted mean of those
  student averages. This avoids partial records changing the trend and avoids
  giving a student with fewer scored criteria disproportionate influence.
- Historical YTD points must use the students who were scored in that
  evaluation, without filtering them to the current roster. Enrollment changes
  must not rewrite a prior evaluation's class result. Do not display a student
  count in the graph; label points with the evaluation name and its grade/
  one-decimal average instead.
- Selecting a named evaluation continues to drive the summary cards,
  criterion chart, ranking, and histogram from that evaluation. Selecting
  **All** continues to drive those page-level views from the existing
  cross-evaluation aggregate, except for the Class Shape histogram: it must
  use the most recent canonical evaluation whose **current-roster filtered**
  snapshot has at least one fully scored student, so its label and data remain
  truthful. The existing histogram otherwise retains its current calculation
  and scope; the fully-scored-only average and historical-cohort rules are
  specific to the YTD trend.
- The YTD graph is independent of the selector and always represents the
  available year-to-date sequence. It should still be shown with one point;
  show a clear empty state only when no evaluation has a fully scored student.

## Implementation Steps

1. Add a dashboard-oriented analytics result at the feature-service boundary.

   - Update `src/app/services/feature_services.h` and
     `src/app/services/feature_services.cpp` so the speaking-evaluation
     service can load the selected/aggregate analytics, the evaluation to use
     for the Class Shape histogram, and the chronological YTD points in one
     request.
   - Load the roster once and load each canonical evaluation once. Continue to
     apply the existing current-roster filter to the selected/aggregate
     dashboard and Class Shape histogram, preserving the existing policy that
     departed students do not affect those current-class analytics.
   - Build two views from each named evaluation: the filtered snapshot used by
     the current dashboard/histogram and an unfiltered historical snapshot for
     the YTD calculation. The latter deliberately retains students who were
     scored at that time but no longer appear in today's roster.
   - Derive a YTD point only from an unfiltered snapshot with one or more
     fully scored students, omitting blank or partial-only evaluations from
     the series. Independently, choose the All-selection Class Shape snapshot
     as the last canonical **filtered** snapshot with one or more fully scored
     students; this guarantees that the histogram's caption and data refer to
     the same current-class evaluation.
   - For a named selector value, use its snapshot for the existing dashboard
     and histogram. For **All**, retain the current aggregate calculation for
     the rest of the dashboard, but return the latest eligible filtered named
     snapshot for the histogram.
   - Define a small, named result type rather than overloading the meaning of
     the existing selected snapshot. It should expose: the selected dashboard
     snapshot, `classShapeEvaluationName`, the Class Shape snapshot, and a
     list of `{ evaluationName, classAverage3, classAverageLetter }` YTD
     points. Keep fully-scored-only grade calculations in
     `SpeakingAnalytics`; do not duplicate score parsing or rounding in the
     feature service or page.

2. Extend the pure analytics API with the value objects and helpers the
   feature service needs.

   - In `src/features/classes/services/speaking_analytics.h` and `.cpp`, add
     a lightweight `YearToDatePoint` value type (evaluation name, rounded
     numeric average, and display grade) plus a helper for making a point from
     the fully scored ranks in a `Snapshot`. The helper should return no point
     when `fullyScoredCount` is zero and must not reuse `classAverage3`, which
     intentionally includes partially scored students for the existing
     dashboard.
   - Keep `compute()` responsible only for a single supplied set of matrices;
     the feature service remains responsible for repository loading, roster
     filtering, canonical ordering, selection semantics, and composing the
     dashboard result.
   - Preserve existing `compute()` and aggregate behavior so current summary,
     ranking, and criterion analytics do not change accidentally.

3. Implement a reusable custom Year-to-Date chart widget.

   - In `src/features/classes/ui/class_analytics_charts.h` and `.cpp`, add a
     `YearToDateChart` with `setData(const QList<SpeakingAnalytics::YearToDatePoint>&)`.
     Give it an expanding horizontal policy, a compact fixed/minimum height,
     and a useful `sizeHint()` for both the side-by-side and stacked layouts.
   - Paint a line chart using the same custom-QPainter approach and light/dark
     palette helpers as `GradeHistogram`: a stable 1–5 vertical scale with
     grade tick labels (`C`, `B`, `B+`, `A`, `A+`), canonical evaluation names
     along the x-axis, a visible line/markers, and the one-decimal average (or
     grade plus average) at each point when there is room. Do not render a
     student count anywhere in the chart.
   - Handle zero and one data point intentionally: render a muted no-data
     message for zero points, and a single marker/value without attempting to
     draw a misleading line for one point. Elide or abbreviate x-axis labels
     at narrow widths rather than overlapping them.
   - Use semantic object names for the chart and its neighboring labels so UI
     tests can find them without relying on layout positions.

4. Compose both visualizations and their labels in the Class Shape card.

   - Update `src/features/classes/ui/class_analytics_page.h` with pointers for
     the histogram caption, `YearToDateChart`, and its heading/caption.
   - Update `buildUi()` in `src/features/classes/ui/class_analytics_page.cpp`
     to make the Class Shape card a vertically stacked panel: a compact label
     such as `Evaluation: Fall`, the existing histogram, a divider or spacing,
     then `Year to Date` and the new trend chart. Retain the card's existing
     minimum width and let the content layout determine height.
   - In `rebuild()`, request the dashboard result rather than only the current
     snapshot. Clear all new labels/chart data in `clearDisplay()` and populate
     them in `applySnapshot()` (or a dedicated `applyDashboard()` method).
     Populate the histogram from the returned Class Shape snapshot—not the
     aggregate snapshot—and set its caption to the returned evaluation name.
   - On selector changes, refresh the histogram caption and source as above;
     leave the YTD series unchanged. On language changes, retranslate static
     labels while retaining the stored evaluation name and currently loaded
     chart data.
   - Revisit `layoutChartCards()` and the chart widgets' minimum heights after
     the card grows: at wide widths the card must not clip either chart, and
     below the existing 900px breakpoint the cards must stack cleanly with no
     horizontal overflow beyond the page's normal scroll behavior.

5. Add focused tests and run visual verification.

   - Extend `tests/speaking_analytics_tests.cpp` to cover chronological YTD
     point construction, omission of unscored and partial-only evaluations,
     exclusion of partially scored students from an otherwise eligible point,
     equal weighting of fully scored students, and preservation of the
     declared grade-rounding rule. Run the existing
     `ClassMngrSpeakingAnalyticsTests` target.
   - Add feature-service coverage at the smallest existing service-test seam
     for: **All** selecting the last fully scored canonical evaluation for
     Class Shape; a named selection using that same named evaluation; the trend
     retaining historical students while the current dashboard remains roster
     filtered; and no count being exposed as a YTD data label. If no suitable
     seam exists, factor the dashboard composition into a database-free helper
     in `SpeakingAnalytics` and test it there rather than adding database
     fixtures.
   - Add a lightweight widget/render test for `YearToDateChart` (new test
     target and CMake registration only if none can host it) that renders
     zero-, one-, and multi-point data offscreen and verifies no paint errors,
     a non-empty graph for scored data, and legible behavior at the narrow
     card width. Avoid brittle full-image snapshots for themed colors.
   - Manually verify the analytics page in light and dark themes at both sides
     of the 900px layout breakpoint, with no data, one completed evaluation,
     multiple evaluations, **All**, and a named selector. Confirm that the
     histogram label always matches its data and that the YTD line contains
     only completed evaluations in chronological order.

## Files Expected to Change

| File | Planned change |
| --- | --- |
| `src/features/classes/services/speaking_analytics.h` | Add the YTD point value type and any pure helper. |
| `src/features/classes/services/speaking_analytics.cpp` | Implement the pure YTD point/helper behavior. |
| `src/app/services/feature_services.h` | Expose a composed Class Analytics dashboard result. |
| `src/app/services/feature_services.cpp` | Load/filter evaluation matrices once and compose selected, Class Shape, and YTD results. |
| `src/features/classes/ui/class_analytics_charts.h` | Declare `YearToDateChart`. |
| `src/features/classes/ui/class_analytics_charts.cpp` | Paint the responsive YTD line chart. |
| `src/features/classes/ui/class_analytics_page.h` | Store new Class Shape labels/chart pointers and dashboard application helpers. |
| `src/features/classes/ui/class_analytics_page.cpp` | Add both labeled charts to the card and bind them to the composed result. |
| `tests/speaking_analytics_tests.cpp` | Cover trend ordering, data eligibility, and score semantics. |
| Relevant existing service/widget test and its CMake registration, if needed | Cover selection and rendering integration. |

## Acceptance Criteria

- The Class Shape card contains both the existing grade-distribution histogram
  and a Year to Date trend graph.
- With **All** selected, the histogram is labeled with—and plots—the most
  recent canonical evaluation that has at least one fully scored current-roster
  student.
- With a named evaluation selected, the histogram label and plotted
  distribution both correspond to that selected evaluation.
- The YTD graph displays each eligible evaluation once, in canonical order,
  using the average of fully scored students only. Blank, unscored, and
  partial-only evaluations do not appear; historical points retain the
  students who were scored at that time even if they have since left the
  roster.
- The YTD graph displays no student count.
- The new content works in light and dark themes, with single/no-point data,
  and across the existing responsive layout breakpoint.
- Existing Class Analytics calculations, roster filtering, and tests remain
  intact.
