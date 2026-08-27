<!-- ClassMngr Startup Optimization Plan — Phase 12 -->

> **Codex scope:** Read `00-START-HERE.md` plus this file. Implement and validate **Phase 12 only**. Do not start later phases.

# Phase 12 — Platform-Specific Validation

> **Status:** In progress — Windows validation started 2026-08-27.

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

### Windows behavioral validation

- Passed targeted tests for startup visual settings, FontManager,
  ScheduleWidget, BasePage, Calendar, Campus, Sub Prep/PDF, memory metrics,
  database file format, and the startup-performance path.
- PageManager validation revealed that its CTest registration needs the
  project-root working directory for runtime assets. The shared test
  registration now sets that directory; it still needs a regenerated CTest
  configuration and rerun.

### Pending platform runs

- macOS: run `macos-clang-debug`, then collect resident memory, physical
  footprint, peak resident memory, startup duration, and structural counts.
- Linux: run `linux-gcc-debug`, then collect RSS, PSS, private resident/dirty
  memory, peak RSS, startup duration, and structural counts.
- On both platforms, execute the same startup-performance and targeted
  behavioral tests used above. No platform-specific page lifecycle has been
  introduced.

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
