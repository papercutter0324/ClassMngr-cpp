<!-- ClassMngr Startup Optimization Plan — Phase 11 -->

> **Codex scope:** Read `00-START-HERE.md` plus this file. Implement and validate **Phase 11 only**. Do not start later phases.

# Phase 11 — Introduce Explicit Startup Completion
## Objective
Define one clear point where initialization ends and normal application operation begins.

## Tasks
### 11.1 Add a shared `startup-complete` transition
Use an existing coordinator/controller if suitable, or add a small startup coordinator if necessary.

At this point:

- MainWindow is visible and interactive;

- startup page is loaded;

- splash is closed/destroyed;

- splash resource lease is released;

- startup-only temporary objects are released;

- profiling captures final startup metrics;

- normal event-loop behavior is active;

- optional post-startup tasks may begin.

### 11.2 Audit post-show timers
Review startup-related:
```cpp
QTimer::singleShot(0, ...)
```
and similar mechanisms.

Classify each as:

- required before startup completion;

- optional after startup completion;

- redundant and removable.

### 11.3 Avoid hidden startup tails
After `startup-complete`, there should not be a cascade of hidden page initialization, global restyling, or refresh-all behavior.

## Acceptance Criteria
Profiling shows one explicit:
```text
startup-complete
```
checkpoint.

Memory and widget counts do not immediately jump afterward because additional hidden startup work was still pending.

---
