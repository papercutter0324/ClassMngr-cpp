<!-- ClassMngr Startup Optimization Plan — Phase 4 -->

> **Codex scope:** Read `00-START-HERE.md` plus this file. Implement and validate **Phase 4 only**. Do not start later phases.

# Phase 4 — Eliminate Duplicate ScheduleWidget Startup Work — Complete (2026-08-27)
## Objective
Normal startup should create and render exactly one ScheduleWidget when the initial screen is My Workspace/Schedule.

This should be true on Windows, macOS, and Linux.

## Completed Implementation

- Phase 3's demand-driven `PageManager` factories keep standalone Schedule and
  Sub Prep unconstructed until their navigation route is opened. Their
  `ScheduleWidget` instances—and Sub Prep's input, keyboard, and class
  information UI—therefore cannot be created during the My Workspace startup
  route.
- The startup profiler now emits a `schedule-widget-startup-diagnostic` event
  immediately after the `startup-complete` checkpoint. It verifies both the
  live and total-created ScheduleWidget counts are exactly one.
- `ClassMngrPageManagerTests` asserts the initial route contains exactly one
  ScheduleWidget, then verifies the standalone Schedule and Sub Prep widgets
  appear only after their respective pages are opened.
- `ClassMngrStartupPerformanceTests` requires the startup-complete metrics and
  profiler diagnostic to report one live and one created ScheduleWidget.

## Validation

- Windows x64 Debug build completed successfully.
- `ClassMngrPageManagerTests` and `ClassMngrStartupPerformanceTests` passed.
- A representative Windows debug capture using `Testing-copy.tps` recorded:

| Startup complete | Peak working set | Working set | Private usage | Widgets | Pages | ScheduleWidgets | Schedule renders |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 3.031 s | 203.2 MiB | 202.1 MiB | 131.2 MiB | 243 | 1 / 11 | 1 live / 1 created | 4 |

The four render passes belong to the one visible My Workspace schedule widget;
this phase neither shares QWidget instances nor adds a shared schedule-data
layer.

## Current Problem
Multiple eager features can create their own ScheduleWidget:
```text
MyWorkspacePage

└─ ScheduleWidget

standalone SchedulePage

└─ ScheduleWidget

SubPrepPage

└─ read-only ScheduleWidget
```
Each may perform its own schedule loading/rendering.

## Tasks
### 4.1 Keep the initial My Workspace schedule only
If My Workspace/Schedule is the initial destination, create only that schedule.

### 4.2 Make standalone SchedulePage lazy
Its ScheduleWidget should not exist until the user opens that page.

### 4.3 Make SubPrepPage lazy
Do not construct:

- its large input UI;

- keyboard-related UI;

- class information UI;

- read-only ScheduleWidget;

until Sub Prep is opened.

### 4.4 Add a ScheduleWidget startup assertion/diagnostic
During representative startup, verify:
```text
ScheduleWidget instances at startup-complete = 1
```
for the normal My Workspace/Schedule startup route.

### 4.5 Consider a shared schedule data layer later
Do not share actual QWidget instances.

If profiling shows repeated database/data transformation cost after multiple schedule views are opened, consider:
```text
ScheduleDataProvider

→ immutable/shared schedule snapshot

→ multiple schedule presentations
```
This is optional and should not block Phase 4.

## Acceptance Criteria
Normal startup creates one ScheduleWidget and renders one visible schedule.

Additional schedule views are created only on first use.

---
