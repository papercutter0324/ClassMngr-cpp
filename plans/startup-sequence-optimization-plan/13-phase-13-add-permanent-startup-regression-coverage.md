<!-- ClassMngr Startup Optimization Plan — Phase 13 -->

> **Codex scope:** Read `00-START-HERE.md` plus this file. Implement and validate **Phase 13 only**. Do not start later phases.

# Phase 13 — Add Permanent Startup Regression Coverage

## Completed Implementation

- `Testing-copy.tps` is now a synthetic, deterministic representative profile:
  four teachers, eight classes, ten regular schedule entries, four intensive
  schedule entries, intensive slot states, two populated rosters, and saved
  profile/schedule settings. Its readable source is
  `representative-startup-fixture.sql`; do not put user data into this fixture.
- `ClassMngrStartupPerformanceTests` validates the fixture's integrity and
  exact representative-data counts before using it. The performance run copies
  the fixture to its temporary directory and writes a fixed QSettings profile,
  so neither a developer's preferences nor application writes can affect the
  checked-in baseline.
- The representative profiling run now requires `window-shown`,
  `startup-complete`, and `settled-5s` snapshots to contain duration, native
  memory, QWidget, page, ScheduleWidget, and render metrics. `peakMemory`
  summarizes sampled and platform-reported peak values across the run.
- Structural assertions lock the normal My Workspace/Schedule startup route
  to one instantiated page (of 11 registered), one live/created
  `ScheduleWidget`, and one render. The profiler event stream must contain
  only `my-workspace` page construction—Sub Prep, PDF Viewer, and Campus
  Dashboard remain lazy through the five-second checkpoint.
- No fixed timing or memory limit is enabled by default. The test emits each
  platform's native measurements for baseline/trend comparison; optional
  timing limits remain environment-controlled until stable platform-specific
  baselines exist.

## macOS arm64 Validation (2026-08-27)

The rebuilt Release application and `ClassMngrStartupPerformanceTests` passed
with the offscreen platform and the deterministic representative profile.
This is a platform baseline for trend comparison, not a cross-platform CI
limit.

| Checkpoint | Elapsed | Resident | Peak resident | Physical footprint | Widgets | Pages | ScheduleWidgets | Renders |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| `window-shown` | 2.605 s | 100.3 MiB | 100.3 MiB | 38.4 MiB | 229 | 1 / 11 | 1 / 1 created | 1 |
| `startup-complete` | 2.605 s | 100.2 MiB | 100.3 MiB | 38.4 MiB | 224 | 1 / 11 | 1 / 1 created | 1 |
| `settled-5s` | 7.862 s | 102.8 MiB | 102.8 MiB | 40.9 MiB | 224 | 1 / 11 | 1 / 1 created | 1 |

The report's `peakMemory` object recorded a 102.8 MiB platform peak. No page
or schedule construction occurred after `startup-complete`.

## Objective
Prevent future features from gradually restoring eager startup behavior or large transient memory usage.

## Tasks
### 13.1 Preserve a deterministic representative startup database
Include enough realistic data to exercise the normal initial page:

- multiple teachers;

- multiple classes;

- regular schedules;

- intensive schedules where applicable;

- roster data;

- representative saved settings.

### 13.2 Record shared regression metrics
At minimum:
```text
startup duration

QWidget count

instantiated page count

ScheduleWidget count

schedule render count
```
At:
```text
window-shown

startup-complete

+5s
```
### 13.3 Record native memory metrics per platform
Use the platform metrics established in Phase 1.

Do not require identical numeric values between operating systems.

Track each platform against its own baseline and trend.

### 13.4 Track peak as well as steady state
A result such as:
```text
startup-complete memory = 150 MB
```
is not sufficient if:
```text
peak startup memory = 650 MB
```
Both must be reported.

### 13.5 Add structural regression assertions where practical
Examples:

For the normal My Workspace/Schedule route:
```text
ScheduleWidget count at startup-complete == 1
```
and:
```text
SubPrepPage instantiated == false

PdfViewer instantiated == false

CampusDashboard instantiated == false
```
until opened.

Structural assertions may be more stable across CI machines than hard memory thresholds.

### 13.6 Establish numeric thresholds only after stable measurements
Prefer trend/regression detection first.

Once representative machines/CI environments provide stable data, establish platform-specific warning/failure thresholds.

## Acceptance Criteria
A future feature cannot silently make itself part of startup without:

- increasing structural startup metrics;

- increasing memory/timing metrics;

- or failing explicit lazy-instantiation assertions.

---
