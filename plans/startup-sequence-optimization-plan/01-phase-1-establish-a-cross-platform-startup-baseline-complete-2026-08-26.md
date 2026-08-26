<!-- ClassMngr Startup Optimization Plan — Phase 1 -->

> **Codex scope:** Read `00-START-HERE.md` plus this file. Implement and validate **Phase 1 only**. Do not start later phases.

# Phase 1 — Establish a Cross-Platform Startup Baseline — Complete (2026-08-26)
## Captured Windows Representative Baseline
Three representative startup captures using the deterministic `testing-copy.tps`

profile completed successfully. Interactive birthday prompts were suppressed only

for the profiling scenario; the normal post-show startup work remained enabled.

| Run | Startup complete | Peak working set | Working set at +30 s | Private usage at +30 s | Pages | ScheduleWidgets | Schedule renders (startup → +30 s) |

|---:|---:|---:|---:|---:|---:|---:|---:|

| 1 | 6.592 s | 734.6 MiB | 732.5 MiB | 618.2 MiB | 8 / 11 | 3 | 9 → 10 |

| 2 | 6.623 s | 734.8 MiB | 733.0 MiB | 617.9 MiB | 8 / 11 | 3 | 9 → 10 |

| 3 | 6.929 s | 734.3 MiB | 732.4 MiB | 618.5 MiB | 8 / 11 | 3 | 9 → 10 |

The reports include shared checkpoint, widget, page, ScheduleWidget, render,

and deferred-deletion metrics, plus platform-native memory samples. This

baseline confirms the duplicate schedule construction/rendering and eager-page

work targeted by Phases 3–5.

## Objective
Create startup diagnostics that measure the real production startup path on all supported operating systems.

Windows should receive the most detailed memory instrumentation because it currently exposes the largest problem, but startup timing and application-level metrics should be consistent across all platforms.

## Tasks
### 1.1 Define shared startup checkpoints
Create one shared startup profiling mechanism with checkpoints such as:
```text
01 process-start

02 qapplication-created

03 preferences-resolved

04 locale-applied

05 font-applied

06 theme-applied

07 resource-system-initialized

08 splash-shown

09 services-created

10 main-window-shell-created

11 page-manager-initialized

12 controllers-connected

13 database-opened

14 navigation-data-loaded

15 startup-page-created

16 startup-page-loaded

17 window-shown

18 startup-complete

19 settled-1s

20 settled-5s

21 settled-30s
```
The exact names may be adapted to the existing profiling code, but the sequence should clearly show where memory, widget count, and startup duration change.

### 1.2 Record shared application-level metrics
At each important checkpoint record:

- elapsed startup time;

- total `QWidget` count;

- instantiated PageManager page count;

- instantiated `ScheduleWidget` count;

- schedule render count where applicable;

- number of widgets queued through `deleteLater()` during startup where practical.

These metrics should work on all platforms.

### 1.3 Add platform-specific memory metrics behind shared profiling
Use the same profiling interface but implement native memory collection per platform.

#### Windows
Record where available:

- `WorkingSetSize`;

- `PeakWorkingSetSize`;

- `PrivateUsage`;

- `PrivateWorkingSetSize`.

#### macOS
Record the best available equivalents for:

- resident memory;

- physical/private footprint where available;

- peak resident usage if practical.

Use native APIs or existing project-compatible facilities.

#### Linux
Record the best available equivalents for:

- RSS;

- PSS where practical;

- private dirty/private resident memory where practical;

- peak resident memory if available.

Do not block implementation of the architecture on perfect cross-platform metric equivalence. The purpose is to observe trends and regressions consistently.

### 1.4 Instrument page construction
Log when each PageManager page is actually instantiated:
```text
page-created: my-workspace

page-created: schedule

page-created: sub-prep

...
```
Distinguish:
```text
registered pages
```
from:
```text
instantiated pages
```
### 1.5 Instrument schedule rendering
For every schedule render during startup, record:

- ScheduleWidget purpose/owner;

- render start;

- render end;

- table items created;

- cell widgets created;

- cell widgets removed;

- cell widgets queued through `deleteLater()`.

Keep detailed render diagnostics profiling/debug-only.

### 1.6 Define two startup profiling scenarios
#### Minimal Startup
Used to understand framework/application baseline:

- no normal recent database load;

- no automatic update check;

- minimal nonessential startup activity.

#### Representative Startup
Used for regression testing:

- same startup sequence as normal production;

- deterministic or explicitly supplied database;

- normal saved settings;

- normal initial page/tab selection;

- includes normal post-show work unless that work is removed by later phases.

The representative path must not silently disable suspected code.

### 1.7 Capture baseline runs
Run at least three representative startup captures on Windows.

Where development hardware is available, also capture macOS and Linux baselines.

Record:

- peak memory;

- startup-complete memory;

- +5 second memory;

- startup duration;

- QWidget count;

- page count;

- ScheduleWidget count;

- schedule render count.

## Acceptance Criteria
Phase 1 is complete when the profiling output can answer:

- where the largest startup allocation/residency increase occurs;

- which pages/widgets existed at that point;

- how many schedules were constructed and rendered;

- whether startup performs work after the main window is shown;

- whether Windows is showing private commitment growth, working-set residency growth, or both;

- how startup behavior differs across Windows, macOS, and Linux.

Do not begin broad architectural changes without capturing the Windows representative baseline first.

---
