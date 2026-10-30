# Candidate #49 — Workspace Calendar evidence

This package contains the eight Legacy/Next captures of the Calendar tab embedded in My Workspace, plus Calendar-only records extracted from the ignored source manifest at `artifacts/phase7-batch5-c49-workspace-calendar/`. Schedule captures are excluded. The [manifest](manifest.json) retains all eight source capture records and their SHA-256 values, the source scenario count (13), the source manifest hash, and the original locale-divergence and memory-scope notes. Independent source checks passed for 13 PNGs plus the manifest and all 14 source `SHA256SUMS` entries; the copied Calendar PNGs match this manifest 8/8.

All images are 1024×768, captured with Fusion at an offscreen viewport. The pairs cover English and Korean in Light and Dark:

| Locale / theme | Legacy | Next |
|---|---|---|
| English / Light | [PNG](legacy_my_workspace_calendar_en_US_light.png) | [PNG](next_my_workspace_calendar_en_US_light.png) |
| English / Dark | [PNG](legacy_my_workspace_calendar_en_US_dark.png) | [PNG](next_my_workspace_calendar_en_US_dark.png) |
| Korean / Light | [PNG](legacy_my_workspace_calendar_ko_KR_light.png) | [PNG](next_my_workspace_calendar_ko_KR_light.png) |
| Korean / Dark | [PNG](legacy_my_workspace_calendar_ko_KR_dark.png) | [PNG](next_my_workspace_calendar_ko_KR_dark.png) |

## Comparison and lifecycle evidence

The Calendar viewport rectangle is `(44,185,936×495)` and the month-grid rectangle is `(90,239,834×840)`. Only `834×441` grid pixels fall inside the captured viewport. Independent C49 verification reports those visible pixels match in all four Legacy/Next pairs.

In every Next capture, the manifest records one Calendar view and root, two cached events, a 31-day upcoming range, and one live heavy Calendar session. After suspension, the Calendar view count is zero. The Legacy records show five Calendar view objects, two cached events, and a non-loading cache. These are captured lifecycle counters; process-wide memory snapshots do not measure page-level allocation.

## Focused verification and limits

Independent C49 verification reported the focused CTests passing 3/3: [`ClassMngrNextMyWorkspacePageTests`](../../../../../../cmake/tests/next.cmake#L3656) ([source](../../../../../../tests/next/my_workspace_page_tests.cpp)), [`ClassMngrNextCalendarPageTests`](../../../../../../cmake/tests/next.cmake#L8184) ([source](../../../../../../tests/next/calendar_page_tests.cpp)), and [`ClassMngrNextMyWorkspaceVisualCapture`](../../../../../../cmake/tests/next.cmake#L3716) ([source](../../../../../../tests/next/my_workspace_visual_capture.cpp)). The direct offscreen Bootstrap flow [`ClassMngrNextBootstrapTests calendarRoutesFollowWorkspaceReplacementAndCloseFlows`](../../../../../../cmake/tests/next.cmake#L487) ([case source](../../../../../../tests/next/application_bootstrap_tests.cpp#L7090)) also passed, with three QtTest entries including setup and cleanup.

This establishes visible month-grid pixel parity only. In Korean, Next's My Workspace shell remains translated while Legacy's My Workspace shell labels remain English. The Upcoming area is below the captured viewport. No whole-shell pixel-parity claim is made.
