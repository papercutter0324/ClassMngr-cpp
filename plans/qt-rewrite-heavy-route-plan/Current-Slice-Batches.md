# Current Slice Batches

## Phase 9 — Batch 3: Additional packaged Release lifecycle evidence (Active)

Batch goal: measure the remaining large or repeated Next workflows in current-source packaged Windows x64 Release processes. Use authentic UIA-bound actions, complete lifecycle snapshots, identified fixtures, and local validation. For each gated settled startup/normal-use lifecycle window, target Working Set below 268,435,456 B (256 MiB); values from that target through 314,572,800 B (300 MiB) are acceptable but reported as target misses, and values above the ceiling fail. Keep operation peaks separate under existing Phase 9 budgets. Preserve source/package equivalence, database/UI correctness, cleanup and return-to-baseline allowances, and repeated-cycle checks. Keep transient budgets advisory until a return-to-baseline allowance is supported by current-source measurements. Commit each slice; push once after P9-18 closes Batch 3. GitHub validation is not acceptance.

16. **P9-16 — Repeated workspace open/close** (Complete)
    Accepted on the primary Working Set after one warm-up and 10 same-process
    reopen cycles. Maximum settled Working Set was 235,921,408 B. Private Bytes
    increased 10.8 MB open and 11.2 MB closed; this secondary diagnostic is
    unresolved and does not establish a leak. See the [accepted run evidence](../../artifacts/phase9/packaged-next-p9-16-repeated-workspace/20261121T023320451Z-9b576dcbfdc84dd0996f279db5ef67e9/manifest.json).
17. **P9-17 — Large roster editing** (Active)
    Latest exact-source route preserved edits and database correctness. Baseline
    Working Set was 246,779,904 B (10 samples, range 0); post-clear and re-entry
    medians were 270,020,608 B and 274,874,368 B. All settled windows were at
    or below 300 MiB, but post-save, post-clear, suspended, and re-entry missed
    the 256 MiB target. Operation peak was 277,614,592 B. Cleanup exceeded the
    empirical 246,779,904 B allowance by 23,240,704 B, so the slice remains
    open. The by-value move fix is under acceptance; do not claim memory
    improvement. No named Phase 0 memory route was found.
18. **P9-18 — Campus map open/close** (Queued; last in Batch 3)
    Measure decoded map retention and release; no named Phase 0 memory route
    was found. Push once after this slice closes Batch 3.

## Phase 9 — Batch 4 discovery (Complete; ordered after Batch 3)

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
