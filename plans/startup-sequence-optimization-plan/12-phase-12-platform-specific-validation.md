<!-- ClassMngr Startup Optimization Plan — Phase 12 -->

> **Codex scope:** Read `00-START-HERE.md` plus this file. Implement and validate **Phase 12 only**. Do not start later phases.

# Phase 12 — Platform-Specific Validation
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
