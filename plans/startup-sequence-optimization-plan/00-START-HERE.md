# ClassMngr Startup Optimization — Shared Codex Context

## Purpose

Optimize ClassMngr startup so the first usable screen is shown with only the work and resources it actually requires.

Windows is the primary diagnostic/regression platform because normal startup has reached roughly **600–700 MB** even though the application can later operate below roughly **150 MB**. The architectural fixes must remain shared across Windows, macOS, and Linux.

## How to Use These Files

For normal implementation work, give Codex only:

```text
00-START-HERE.md
+
the current numbered phase file
```

Complete, test, and measure one phase before moving to the next. Do not load all phase files into context at once.

Use `99-FINAL-ARCHITECTURE-AND-REVIEW.md` only for the final cross-phase review or when a phase specifically needs the end-state architecture.

## Core Startup Rule

> ClassMngr should perform only the work required to display its first usable screen. Hidden pages, hidden feature resources, duplicate rendering, and unchanged global settings must not be initialized or reapplied merely because the application started.

## Shared Architectural Rules

- Keep one shared startup lifecycle across Windows, macOS, and Linux.
- Use platform-specific code only where the OS genuinely requires it, such as native memory metrics or platform integration.
- Registering a page must not construct it.
- Hidden pages must not query or render because the database opened.
- Establish locale, font, theme, and other global visual state before constructing most widgets.
- Do not treat page activation as an automatic full refresh.
- Heavy feature initialization belongs to first use of that feature.
- Deferred work is useful only when it is truly optional; do not move all eager startup work to a timer after the first frame.
- Measure **peak memory**, not only eventual steady-state memory.
- Keep expensive work out of constructors and `showEvent()` unless demonstrably necessary.

## Global Constraints

- Do not use `EmptyWorkingSet()`, `SetProcessWorkingSetSize()`, `HeapCompact()`, `malloc_trim()`, or similar APIs as the production fix.
- Windows-specific memory trimming may be used only as a diagnostic experiment.
- Do not reduce reported memory artificially instead of eliminating unnecessary startup work.
- Do not introduce separate Windows/macOS/Linux page lifecycles or startup architectures.
- Do not change unrelated UI behavior or styling.
- Preserve current cross-platform behavior.
- Preserve existing feature-scoped resource-pack behavior until its dedicated phase.
- Prefer small, reviewable commits.
- Capture before/after measurements for every major phase.
- Do not optimize only final memory; startup peak, widget count, page count, render count, and startup duration matter.

## Execution Rule

For the current phase:

1. Inspect the relevant implementation before changing it.
2. Make the smallest coherent change that completes the phase.
3. Do not preemptively implement later phases.
4. Build and run the relevant tests.
5. Capture the phase's required measurements/structural counts.
6. Record any remaining issue that belongs to a later phase rather than expanding scope.

Phase 1 already contains the captured Windows representative baseline and is marked complete in its phase file.
