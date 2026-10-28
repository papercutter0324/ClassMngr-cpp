# Class Analytics

Candidate #27 was accepted at source commit `8f38ae44`.

## Capture set

The paired captures use the [96-class fixture](../../../release/large-speaking-evaluation-boundary/generated-large-speaking-evaluation.tps), selected class ID `1`, and a 1024×768 viewport. They show All and populated Winter selector states for Legacy and Next in English (`en_US`) and Korean (`ko_KR`), light and dark themes. The [manifest](classes_analytics_paired_capture.json) records eight pairs and 16 PNGs.

| Locale / theme | Legacy All | Next All | Legacy Winter | Next Winter |
|---|---|---|---|---|
| en_US / light | [PNG](classes_analytics_en_US_light_legacy_all.png) | [PNG](classes_analytics_en_US_light_next_all.png) | [PNG](classes_analytics_en_US_light_legacy_winter.png) | [PNG](classes_analytics_en_US_light_next_winter.png) |
| en_US / dark | [PNG](classes_analytics_en_US_dark_legacy_all.png) | [PNG](classes_analytics_en_US_dark_next_all.png) | [PNG](classes_analytics_en_US_dark_legacy_winter.png) | [PNG](classes_analytics_en_US_dark_next_winter.png) |
| ko_KR / light | [PNG](classes_analytics_ko_KR_light_legacy_all.png) | [PNG](classes_analytics_ko_KR_light_next_all.png) | [PNG](classes_analytics_ko_KR_light_legacy_winter.png) | [PNG](classes_analytics_ko_KR_light_next_winter.png) |
| ko_KR / dark | [PNG](classes_analytics_ko_KR_dark_legacy_all.png) | [PNG](classes_analytics_ko_KR_dark_next_all.png) | [PNG](classes_analytics_ko_KR_dark_legacy_winter.png) | [PNG](classes_analytics_ko_KR_dark_next_winter.png) |

Independent Tester inspection covered the English/light and Korean/dark pairs. Images show the top-left 1024×768 viewport. The Legacy Classes shell and Next navigator shell differ, so this package records the compared states and makes no visual-parity claim.

## Analytics, state, and memory scope

Next reads through the path-scoped `WorkspaceStore` and typed `ClassAnalyticsDashboardReadQuery`. The selector offers All, Winter, Speech Contest, Summer, and Fall. The dashboard shows average, scored/roster counts, strongest and focus areas, six criterion distributions, a class-shape histogram, a year-to-date chart, and model/view-backed rankings. All and Winter each return the fixture's 25 Winter rows; the read has no arbitrary per-class row maximum.

Legacy has 677 active page widgets and one instantiated Analytics editor. Next has 231 route widgets before Analytics opens and 274 while it is active; the active page has one selected editor root with 226 descendants and one ranking model. It has zero class-scoped widgets. The selected-class rows clear on section exit, and workspace close clears the analytics state.

The manifest's sequential macOS whole-process observations span:

| Route | Working set, bytes | Private usage, bytes |
|---|---:|---:|
| Legacy | 39,960,576–104,235,008 | 12,272,360–44,090,328 |
| Next | 91,242,496–104,251,392 | 33,588,184–44,090,328 |

These are process-wide snapshots across load and cleanup states, not page allocations or a memory budget. The manifest retains each observation.

## Term defaults and parser boundary

Next retains the shared `classes_navigation_evaluation_default_policy` semantics. Term-to-schedule mapping is explicit, and the old 2200 cutoff is removed. Local term-schedule parsing remains because no shared Next parser exists; depending on Calendar would cross module boundaries. This duplicates Calendar's interpretation and requires ongoing parity coverage. Future Next consumers should extract a shared pure parser. Legacy Speaking Evaluation already consumes the default preference and remains in candidate #38 scope.

## Verification and limits

Affected targets built. The focused CTest selection passed 28/28, including Navigator/WorkspaceStore, Analytics capture/query/policies, Bootstrap/PageHost, the dependency boundary, and Legacy Analytics/default/calendar regressions. The WorkspaceStore Qt suite passed 23/23 cases.

New Legacy-comparison tests cover custom term boundaries, elementary and middle-school profiles, dates through 2201, missing profile, invalid week count, and invalid weekday. Existing path tests cover current populated/empty defaults, previous-term fallback, Spring-to-Speech Contest mapping, explicit retention, and malformed JSON. `git diff --check`, resource inventory `--check`, and the source/dependency boundary scan passed.

No full-suite, Windows, visual-parity, or memory-budget result is claimed. The 7A screenshot/memory evidence and 7A/Phase 7 exit gates remain open.
