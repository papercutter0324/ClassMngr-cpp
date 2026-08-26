<!-- ClassMngr Startup Optimization Plan — Phase 10 -->

> **Codex scope:** Read `00-START-HERE.md` plus this file. Implement and validate **Phase 10 only**. Do not start later phases.

# Phase 10 — Make Startup Data Queries Efficient
## Objective
Avoid repeated database calls and repeated transformation of data required by the initial shell.

## Tasks
### 10.1 Audit sidebar loading
Review for N+1 patterns similar to:
```text
load teachers

load classes

for each class:

    load classInfo
```
### 10.2 Create purpose-built query/service snapshots where justified
If needed, add a service/repository call returning exactly the sidebar/navigation data needed for startup.

Keep database access out of UI implementation details.

### 10.3 Reuse startup data carefully
If the same teacher/class relationship data is immediately required by:

- sidebar;

- navigation filters;

- initial schedule;

reuse a lightweight immutable snapshot where practical.

Do not create a broad cache layer unless profiling shows a need.

### 10.4 Avoid duplicate query work during activation
A page should not reload data already obtained during the same startup transaction unless its required representation differs materially.

## Acceptance Criteria
Startup navigation/sidebar uses a small, predictable query count independent of class count where practical.

Sidebar construction occurs once.

---
