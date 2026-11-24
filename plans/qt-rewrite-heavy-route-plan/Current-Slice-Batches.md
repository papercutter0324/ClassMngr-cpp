# Current Slice Batches

## Phase 9 — Batch 3: Additional packaged Release lifecycle evidence (Active)

Batch goal: measure the remaining large or repeated Next workflows in current-source packaged Windows x64 Release processes. Use authentic UIA-bound actions, complete lifecycle snapshots, identified fixtures, and local validation. For each gated settled startup/normal-use lifecycle window, target Working Set below 268,435,456 B (256 MiB); values from that target through 471,859,200 B (450 MiB) inclusive are acceptable but reported as target misses, and values above the ceiling fail. Keep operation peaks separate under existing Phase 9 budgets. Preserve source/package equivalence, database/UI correctness, cleanup and return-to-baseline allowances, and repeated-cycle checks. Keep transient budgets advisory until a return-to-baseline allowance is supported by current-source measurements. Commit each slice; push once after P9-18 closes Batch 3. GitHub validation is not acceptance.

16. **P9-16 — Repeated workspace open/close** (Complete)
    Accepted on the primary Working Set after one warm-up and 10 same-process
    reopen cycles. Maximum settled Working Set was 235,921,408 B. Private Bytes
    increased 10.8 MB open and 11.2 MB closed; this secondary diagnostic is
    unresolved and does not establish a leak. See the [accepted run evidence](../../artifacts/phase9/packaged-next-p9-16-repeated-workspace/20261121T023320451Z-9b576dcbfdc84dd0996f279db5ef67e9/manifest.json).
17. **P9-17 — Large roster editing** (Deferred by user direction; unaccepted)
    Row-wise snapshot-to-repository streaming and focused validation are
    complete; the exact-source VS2022/v143 Release receipt is accepted. The
    latest [ten-cycle route](../../artifacts/phase9/packaged-next-p9-17-large-roster/20261123T172727262Z-c5489fce11ad4b8d898dcd935aa4b8b8/manifest.json)
    had a maximum sampled Working Set of 227,966,976 B, PrivateUsage of
    352,763,904 B, and operation peak of 223,801,344 B. All Working Set samples
    met the strict 256 MiB target and cold cleanup passed, but plateau cleanup
    failed. The slice is Deferred by user direction; its cleanup requirement
    remains unaccepted and unresolved. The completed
    full-observer filled-versus-compact-text diagnostic found little effect
    from text length but did not isolate row count or identify a leak owner.
    Independent review of a separate full-versus-3-row comparator contract
    passed five focused tests, but no live arms ran, so it provides no
    cardinality result or leak claim. The P9-17 validator still requires both
    cleanup gates. Isolated exact-source Release build #4 passed all five
    focused P9-17 CTests and was independently accepted for the code/harness
    scope; it does not pass the plateau cleanup gate. See the [Phase 9 progress
    log](10-Phase-9-Progress-Log.md) for build identity and evidence. Runtime
    WPR tracing produced no snapshots or ETL. See [Phase
    14](15-Phase-14-Windows-Memory-Leak-Attribution.md) for deferred
    attribution and revalidation.
18. **P9-18 — Campus map open/close** (Active next slice; last in Batch 3)
    Use the `campus_map` / Maps page with the isolated Bundang fixture
    (`bundang_map.png`, `bundang_building.png`). Both images lazy-load on
    activation and clear on suspension. Run one warm-up, five no-action return
    baseline windows, then ten measured open/suspend cycles. After UIA and
    lifecycle state are confirmed for each settled window, collect ten samples
    at 100 ms intervals. Bind actions, process identity, PID/HWND-bound
    screenshots, image/resource evidence, and read-only app-data snapshots
    together. Derive cold and plateau return gates from the current matched
    baseline and retain the 256/450/512 MiB policy. Exclude Phase 7 campus
    evidence because it is macOS offscreen. P9-18 is active while the separate
    P9-17 cleanup requirement remains unresolved; push once after P9-18 closes
    Batch 3. Fresh isolated exact-source Release build #7 succeeded at
    `artifacts/phase9/p9-18-exact-release-builds/20261124T044000000Z-RETRY7/`
    under VS17 2022 x64/v143. Product source, harness, and executable SHA-256s
    are recorded in the [Phase 9 progress log](10-Phase-9-Progress-Log.md).
    Fourteen focused Python tests, both PowerShell parsers, and all five
    focused CTests passed; no reserved `$PID` was found. The runner validates
    `campus_map/created -> campus_information/suspended -> campus_map/activated`
    first, then exact lowercase transitions. The packaged route has not run:
    the Executor is paused before route execution while the user-requested
    root `build/` cleanup waits on UAC. No P9-18 memory samples or gate result
    exist yet.

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
