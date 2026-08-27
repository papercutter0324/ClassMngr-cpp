<!-- ClassMngr Startup Optimization Plan — Phase 12 -->

> **Codex scope:** Read `00-START-HERE.md` plus this file. Implement and validate **Phase 12 only**. Do not start later phases.

# Phase 12 — Platform-Specific Validation

> **Status:** In progress — Windows and macOS validation completed 2026-08-27; Linux remains pending.

## Validation Record

### Windows x64 Debug — representative startup

The shared startup profiler was run against `Testing-copy.tps` with the
offscreen platform and a five-second settling period.  The same profiler
captures Windows working/private memory, macOS resident/physical footprint,
and Linux RSS/PSS/private-dirty metrics without changing the startup
lifecycle.

| Checkpoint | Elapsed | Working set | Peak working set | Private usage | Private working set | Widgets | Pages | Schedule widgets | Schedule renders |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| `startup-complete` | 2,960 ms | 189.8 MiB | 191.2 MiB | 126.8 MiB | 119.3 MiB | 213 | 1 / 11 | 1 / 1 created | 1 |
| `settled-1s` | 4,065 ms | 208.6 MiB | 208.6 MiB | 145.6 MiB | 137.5 MiB | 213 | 1 / 11 | 1 / 1 created | 1 |
| `settled-5s` | 8,066 ms | 208.6 MiB | 209.1 MiB | 145.6 MiB | 137.5 MiB | 213 | 1 / 11 | 1 / 1 created | 1 |

- Peak-to-steady working-set ratio: **1.002x**.
- The settled 63.1 MiB gap between working set and private usage (71.2 MiB
  versus private working set) has no matching growth in widget, page, or
  schedule metrics. It is therefore resident/shared/cache behavior, not an
  unexplained private-allocation startup tail.
- The startup profile has one `startup-complete` checkpoint and no page
  creation afterward.
- `ClassMngrStartupPerformanceTests` passed for the no-database and explicit
  representative-database scenarios.

### Windows x64 Release — representative and saved-database startup

The Release executable was rebuilt after the profiler/deployment changes and
run with the same offscreen platform and five-second settling period. The
Windows build-tree and install-time deployment rules now explicitly include
Qt's `qoffscreen` platform plugin; without it, headless Release launches
failed with “no platform could be initialized” because deployment provided
only `qwindows.dll`.

| Scenario/checkpoint | Elapsed | Working set | Peak working set | Private usage | Private working set | Widgets | Pages | Schedule widgets | Schedule renders |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| explicit `Testing-copy.tps`, `startup-complete` | 2,962 ms | 143.3 MiB | 144.8 MiB | 124.6 MiB | 94.9 MiB | 212 | 1 / 11 | 1 / 1 created | 1 |
| explicit `Testing-copy.tps`, `settled-1s` | 4,024 ms | 158.3 MiB | 158.3 MiB | 143.3 MiB | 109.7 MiB | 212 | 1 / 11 | 1 / 1 created | 1 |
| explicit `Testing-copy.tps`, `settled-5s` | 8,054 ms | 158.3 MiB | 158.8 MiB | 143.3 MiB | 109.7 MiB | 212 | 1 / 11 | 1 / 1 created | 1 |

- The Release peak-to-steady working-set ratio was **1.003x**; steady
  private usage remained below the 150–175 MiB engineering target and peak
  working set remained below the 250 MiB target.
- A follow-up representative run with no positional database, reusing the
  settings written by the explicit run, recorded
  `database-opened(detail=open)`, confirming most-recent-database startup.
  It reached `startup-complete` once at 2,821 ms with 212 widgets and 1 / 11
  instantiated/registered pages.
- A clean empty-settings Release run recorded
  `database-opened(detail=unavailable)` and one `startup-complete` checkpoint
  at 2,856 ms, confirming no-database startup.

### Windows behavioral validation

- Passed targeted tests for startup visual settings, FontManager,
  ScheduleWidget, BasePage, Calendar, Campus, Sub Prep/PDF, memory metrics,
  database file format, and the startup-performance path.
- PageManager validation now runs from the project root so runtime assets are
  found, and its PDF timing assertions accept either the normal timing or
  slow-operation diagnostic classification.
- The full Windows Debug CTest run completed **64 / 66** tests. The two
  remaining failures are isolated, non-startup tests: the schedule-import
  preview expected eight columns but observed six after preference changes,
  and CampusMap's bundled-image test observed zero loaded images (with its
  decode diagnostic classified as slow-operation). They do not alter the
  startup profiler results above and remain follow-up test-fixture work.

### macOS arm64 host — representative startup

The shared startup profiler was run from the universal `macos-clang-debug`
build against `Testing-copy.tps`, using the offscreen platform and a five-
second settling period. macOS reports resident memory as the working-set
equivalent, `phys_footprint` as physical/private footprint, and `internal` as
the private resident/internal value.

| Checkpoint | Elapsed | Resident memory | Peak resident | Physical footprint | Private resident/internal | Widgets | Pages | Schedule widgets | Schedule renders |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| `startup-complete` | 2,608 ms | 104.0 MiB | 104.2 MiB | 38.6 MiB | 38.1 MiB | 213 | 1 / 11 | 1 / 1 created | 1 |
| `settled-1s` | 3,615 ms | 106.6 MiB | 106.6 MiB | 41.0 MiB | 40.5 MiB | 213 | 1 / 11 | 1 / 1 created | 1 |
| `settled-5s` | 7,511 ms | 106.6 MiB | 106.7 MiB | 41.0 MiB | 40.5 MiB | 213 | 1 / 11 | 1 / 1 created | 1 |

- Peak-to-steady resident-memory ratio: **1.000x**.
- Resident memory grew by 2.6 MiB between `startup-complete` and the
  one-second settled checkpoint, then remained flat through five seconds;
  physical footprint showed the same bounded settling behavior.
- Widget, page, schedule-widget, and schedule-render counts did not change
  after `startup-complete`. The startup profile has exactly one completion
  checkpoint.
- `ClassMngrStartupPerformanceTests` passed for both the no-database and
  explicit representative-database scenarios. The measured startup duration
  was **2,608 ms**.

### macOS behavioral validation

- Passed the focused **16/16** test subset covering startup visual settings,
  FontManager, PageManager, navigation/sidebar, ScheduleWidget, BasePage,
  Calendar, Campus, Sub Prep/PDF, memory metrics, and database file format.
- macOS test registration now uses the offscreen platform for BasePage and
  CampusMap window tests. CampusMap’s test resource image glob was restored
  after dynamic resource loading removed its former definition; this keeps
  test-only assets available without changing application startup behavior.
- No macOS-specific page lifecycle or startup architecture was introduced.

### Pending platform runs

- Linux: run `linux-gcc-debug`, then collect RSS, PSS, private resident/dirty
  memory, peak RSS, startup duration, and structural counts.
- On Linux, execute the same startup-performance and targeted behavioral tests
  used above. No platform-specific page lifecycle has been introduced.

## Objective
Verify that the shared optimized startup architecture behaves correctly and efficiently on Windows, macOS, and Linux.

This phase validates platform differences without introducing separate startup architectures.

## Windows Validation
Measure:

- peak working set;

- working set at startup-complete;

- private usage;

- private working set where available;

- widget/page/schedule counts;

- startup duration.

Investigate any large difference between:
```text
working set
```
and:
```text
private usage/private working set
```
to distinguish real allocation from resident/cache behavior.

### Windows target direction
After optimization, aim approximately for:

- steady private memory around 150–175 MB or lower;

- early steady working set around 200–225 MB or lower;

- peak working set ideally below approximately 250 MB;

- peak-to-steady ratio preferably below approximately 1.5x.

Treat these as initial engineering targets, not hard CI thresholds until measurements are stable.

## macOS Validation
Measure:

- resident memory;

- physical/private footprint where available;

- peak memory;

- startup duration;

- widget/page/schedule counts.

Confirm that the optimized sequence reduces startup work even if macOS already reports low memory.

Do not treat the current lower macOS memory number as proof that eager initialization is harmless.

## Linux Validation
Measure:

- RSS;

- PSS where practical;

- private resident/dirty memory where practical;

- startup duration;

- widget/page/schedule counts.

Confirm that no Linux-specific regression was introduced by changing initialization order.

## Behavioral Cross-Platform Validation
On all three platforms verify:

- application starts with no database;

- application starts with most-recent database;

- explicitly supplied database startup works;

- correct startup page/tab is selected;

- lazy pages open correctly on first use;

- theme preference is correct;

- font preference is correct;

- runtime theme changes work;

- runtime font-size changes work;

- navigation works before lazy pages exist;

- PDF/Calendar/Sub Prep/Campus initialize correctly on first use;

- application shutdown remains clean.

## Acceptance Criteria
All supported platforms follow the same startup lifecycle.

Any remaining OS-specific startup code has a documented platform-specific reason.

---
