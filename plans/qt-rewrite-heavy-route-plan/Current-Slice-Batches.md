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
    P9-07 already covered My Classes on Windows packaged Release; P9-19 covers
    Classes only and returns to My Workspace. Use an exact-source packaged
    Windows x64 VS2022/v143 Release build and a fresh per-run copy of
    `tests/fixtures/workspaces/large_startup.sql` (SHA-256
    `1b2f09be03cce809e239112b3f7b4f2af405bad83a420db0c6ffe5ef0eb597e3`):
    classes 1–96 with eight schedules each (768 total). Reuse the existing
    P9-16 preparation pins, including base workspace SHA-256
    `85c23437621fe67361b38157bb53acdb19e78573ebaa0297fad769b71ad0c9e9`, but
    create a fresh workspace copy; P9-16 package artifacts are not P9-19
    source/package evidence. Capture before/after SQLite snapshots and run
    integrity and read-only checks.

    Route: one warm-up full route, five no-action My Workspace baseline
    windows, then ten measured cycles of entering Classes, deterministically
    selecting all 96 fixture classes by UIA, and returning to My Workspace.
    Bind exact route states, UIA actions/selections, screenshots, and process
    metrics to PID/HWND. Verify the 96-row class list, every selected item,
    the full app-origin lifecycle sequence, settled-window screenshots and
    metrics, normal exit, and absence of temp files and SQLite sidecars.

    Preserve the Phase 9 policy: strict Working Set target below 268,435,456 B
    (256 MiB), inclusive 471,859,200 B (450 MiB) ceiling, and separate
    536,870,912 B (512 MiB) operation advisory. Use the existing two-second
    settle and ten 100 ms samples at settled startup, post-sweep Classes, and
    return windows where applicable; operation sampling remains 50 ms. Keep
    cleanup formulas unchanged: compare pre-route baseline with first return
    for cold cleanup, and use five matched baseline windows plus ten returns
    for plateau cleanup.

    During the 96 selections, a compact app-side checkpoint/summary must show
    exactly one `SelectedClassEditor`, fewer than 250 non-roster widgets, zero
    per-class `class_id` child widgets, and no editor-instance growth across
    the sweep or cycles. Preserve direct page-test checks. Required local
    verification: focused P9-19 validator/fixture tests and current Windows
    ClassesNavigator Page and visual-capture tests; also run PageHost and
    Bootstrap tests if diagnostics wiring changes. Run the exact-source
    Release route; GitHub validation is not acceptance.
20. **P9-20 — Larger successful Schedule Import apply** (Conditional)
    Proceed only if a suitable larger valid fixture is available. P9-09's
    successful apply covered 20 candidates, not 96. Measure parse, review,
    apply, cleanup, and persisted associations if feasible.
21. **P9-21 — Repeated workspace Private Bytes trend follow-up**
    Use accepted P9-16 as the baseline for its unresolved secondary diagnostic;
    do not characterize it as a leak.

The full Phase 9 Windows x64 Debug CTest and final packaged Release exit gates
remain separate and open.
