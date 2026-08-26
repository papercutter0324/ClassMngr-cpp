# ClassMngr Startup Optimization — Split Plan

This folder is organized to minimize Codex context.

## Normal Codex Input

Give Codex exactly two files during implementation:

```text
00-START-HERE.md
+
<one current phase file>
```

Do not provide every phase at once.

## Phase Order

1. [`01-phase-1-establish-a-cross-platform-startup-baseline-complete-2026-08-26.md`](./01-phase-1-establish-a-cross-platform-startup-baseline-complete-2026-08-26.md) — Establish a Cross-Platform Startup Baseline — Complete (2026-08-26) — **already complete**
2. [`02-phase-2-resolve-global-startup-settings-once.md`](./02-phase-2-resolve-global-startup-settings-once.md) — Resolve Global Startup Settings Once
3. [`03-phase-3-make-pagemanager-demand-driven.md`](./03-phase-3-make-pagemanager-demand-driven.md) — Make PageManager Demand-Driven
4. [`04-phase-4-eliminate-duplicate-schedulewidget-startup-work.md`](./04-phase-4-eliminate-duplicate-schedulewidget-startup-work.md) — Eliminate Duplicate ScheduleWidget Startup Work
5. [`05-phase-5-replace-startup-refresh-cascades-with-explicit-initial-loading.md`](./05-phase-5-replace-startup-refresh-cascades-with-explicit-initial-loading.md) — Replace Startup Refresh Cascades With Explicit Initial Loading
6. [`06-phase-6-define-a-shared-page-lifecycle.md`](./06-phase-6-define-a-shared-page-lifecycle.md) — Define a Shared Page Lifecycle
7. [`07-phase-7-reduce-schedule-rendering-allocation-churn.md`](./07-phase-7-reduce-schedule-rendering-allocation-churn.md) — Reduce Schedule Rendering Allocation Churn
8. [`08-phase-8-defer-nonessential-startup-work.md`](./08-phase-8-defer-nonessential-startup-work.md) — Defer Nonessential Startup Work
9. [`09-phase-9-refine-resource-pack-initialization.md`](./09-phase-9-refine-resource-pack-initialization.md) — Refine Resource-Pack Initialization
10. [`10-phase-10-make-startup-data-queries-efficient.md`](./10-phase-10-make-startup-data-queries-efficient.md) — Make Startup Data Queries Efficient
11. [`11-phase-11-introduce-explicit-startup-completion.md`](./11-phase-11-introduce-explicit-startup-completion.md) — Introduce Explicit Startup Completion
12. [`12-phase-12-platform-specific-validation.md`](./12-phase-12-platform-specific-validation.md) — Platform-Specific Validation
13. [`13-phase-13-add-permanent-startup-regression-coverage.md`](./13-phase-13-add-permanent-startup-regression-coverage.md) — Add Permanent Startup Regression Coverage

## Final Review

After all numbered phases are complete, use:

- `00-START-HERE.md`
- `99-FINAL-ARCHITECTURE-AND-REVIEW.md`
- only the specific completed phase files needed to investigate any regression

This avoids reloading the entire implementation plan for ordinary work.
