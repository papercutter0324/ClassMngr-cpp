<!-- ClassMngr Startup Optimization Plan — Phase 13 -->

> **Codex scope:** Read `00-START-HERE.md` plus this file. Implement and validate **Phase 13 only**. Do not start later phases.

# Phase 13 — Add Permanent Startup Regression Coverage
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
