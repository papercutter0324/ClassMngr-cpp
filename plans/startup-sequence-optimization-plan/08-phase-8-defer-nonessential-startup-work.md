<!-- ClassMngr Startup Optimization Plan — Phase 8 -->

> **Codex scope:** Read `00-START-HERE.md` plus this file. Implement and validate **Phase 8 only**. Do not start later phases.

# Phase 8 — Defer Nonessential Startup Work
## Objective
Make the first usable screen available before performing tasks unrelated to that screen.

This phase must reduce actual startup work, not merely postpone it.

## Classification
### Required before first usable window
Likely:

- QApplication;

- preferences;

- locale;

- font;

- theme;

- minimal resource infrastructure;

- core services;

- requested database;

- navigation shell;

- initial page;

- visible initial data.

### Optional after startup-complete
Evaluate:

- automatic update check;

- birthday/notification checks;

- noncritical maintenance;

- nonessential metadata refresh.

### First-use only
Keep feature-specific initialization lazy:

- PDF;

- Calendar/QML;

- Sub Prep;

- Campus;

- analytics;

- speaking evaluations;

- report/template rendering;

- document payloads;

- other heavy feature resources.

## Important Rule
Do not schedule all hidden-feature initialization at `+1s` or `+5s`.

If the feature is not used, its startup cost should never occur.

## Acceptance Criteria
The main window becomes usable before optional work begins.

No second major allocation spike appears shortly after startup because deferred eager initialization was simply moved later.

---
