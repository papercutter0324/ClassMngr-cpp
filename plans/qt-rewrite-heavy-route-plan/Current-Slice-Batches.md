# Current Slice Batches

## Phase 9 — Batch 4: Additional packaged Release lifecycle evidence (Active)

Batch goal: complete the remaining Phase 9 workflow slices and final validation
in current-source packaged Windows x64 Release processes. Use authentic
UIA-bound actions, complete lifecycle snapshots, identified fixtures, and
local validation. For each gated settled startup/normal-use lifecycle window,
target Working Set below 268,435,456 B (256 MiB); values through 471,859,200 B
(450 MiB) inclusive are acceptable but reported as target misses, and values
above the ceiling fail. Keep operation peaks separate under existing budgets
and preserve source/package equivalence, database/UI correctness, cleanup, and
repeated-cycle checks. Commit each slice; push once after Batch 4 closes.
Hosted GitHub validation is not acceptance.

By user direction, P9-17 plateau cleanup and P9-18 cold cleanup remain failed,
Deferred, and unaccepted to [Phase 14](15-Phase-14-Windows-Memory-Leak-Attribution.md).
Preserve current Maps page retention. These deferrals do not change thresholds
or formulas; Phase 9 may close only after Batch 4 and its final gates pass, with
both deferred gates still explicitly unaccepted.

### Ordered candidates

Discovery found these three candidates and no others:

19. **P9-19 — Classes navigation and editor lifecycle**
    The Phase 9 plan requires My Classes and Classes navigation; P9-07 did not
    cover Classes. Use migrated Phase 7 Classes with the identified large
    fixture; measure 96-class navigation without per-class detail trees or
    growing editor stacks.
20. **P9-20 — Larger successful Schedule Import apply** (Conditional)
    Proceed only if a suitable larger valid fixture is available. P9-09's
    successful apply covered 20 candidates, not 96. Measure parse, review,
    apply, cleanup, and persisted associations if feasible.
21. **P9-21 — Repeated workspace Private Bytes trend follow-up**
    Use accepted P9-16 as the baseline for its unresolved secondary diagnostic;
    do not characterize it as a leak.

The full Phase 9 Windows x64 Debug CTest and final packaged Release exit gates
remain separate and open.
