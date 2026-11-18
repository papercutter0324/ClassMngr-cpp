# Current Slice Batches

## Phase 9 — Batch 2: Packaged Release scenario gates (Active)

Batch goal: turn the common lifecycle trace into source-bound Windows Release
measurements and enforce normal-use budgets with complete, reviewable failure
artifacts. Do not claim a transient operation budget until its return-to-baseline
allowance has been measured and recorded. If evidence exposes a feature-owned
allocation or cleanup defect, route the fix to that feature boundary.

1. **P9-06 — Empty and representative startup-ready/idle gate** (Gate
   implemented; valid optimized run failed the memory limit)
   - Acceptance: locally packaged Windows Release records startup-ready and
     five-minute idle working set for empty and representative workspaces.
     Enforce the strict <262,144,000 byte normal-use limit. Fresh Initial Setup
     remains advisory. Complete over-budget evidence fails the gate and
     preserves all artifacts; incomplete or invalid evidence is not evaluated.
   - Result: both fixtures opened, each completed 301 samples over about 300
     seconds, and both apps exited normally. Evidence validation passed, but
     all six gated working-set points were 679,813,120–687,775,744 bytes. The
     optimized run therefore establishes a valid budget failure, not a Phase 9
     pass. Details and hashes are in the Phase 9 progress log.
2. **P9-06R — Required Inter font registration memory reduction** (Valid
   optimized Release experiment completed; no meaningful reduction)
   - Acceptance: test file-backed Qt registration of the exact catalog-verified
     bundled Inter.ttc while preserving family/style behavior, license, source
     hash checks, size bound, and registration cleanup. Keep Pretendard
     unchanged. Accept the route as a memory fix only if complete valid
     evidence meets the limit.
   - Verification: elevated local build in the standard Windows x64 Debug tree
     passed for ResourceLoader and Bootstrap; focused CTest passed for both.
     The source-bound optimized Release run completed both 301-sample traces,
     passed evidence validation, and exited normally. All six working-set
     points were 680,534,016–688,103,424 bytes, so the gate validly failed.
     This is within 1 MB of the prior baseline and shows no meaningful
     reduction. Details and provenance are in the Phase 9 progress log.
   - Rationale: in-process checkpoints show roughly 559 MB of working-set
     growth between theme-loaded and required-fonts-loaded; the external
     per-font snapshots are asynchronous and cannot precisely assign that
     increase to one registration. Qt 6.12's Windows DirectWrite source reads
     and retains font bytes for file-backed registration as well; the same-asset
     path change therefore did not reduce memory.
3. **P9-06S — Isolate font startup allocations** (Diagnostic complete; no
   normal-use budget result)
   - Acceptance: fresh-process local Windows x64 Release probe using packaged
     Qt 6.12 and exact catalog-hashed font assets captures synchronous metrics
     around Qt startup, each font registration, family enumeration, startup
     font application, and full system-family enumeration.
   - Result: the probe ran seven fresh processes per scenario in five scenarios
     (no fonts, Inter, Pretendard, and both orders). An independent local
     seven-process-per-scenario run validated all 35 process identities and
     ordered metric records and recomputed 203 operation deltas from raw
     snapshots. Inter registration added a median about 490 MB WS / 494 MB
     private in either order; Pretendard added about 62 MB. This localizes the
     dominant increase to Inter registration but does not establish Qt's
     internal cause. The diagnostic does not pass the normal-use budget. Both
     run IDs and hashes are recorded in the Phase 9 progress log.
4. **P9-06T — Compare Inter faces and Qt font backends** (Diagnostic complete)
   - Acceptance: compare the packaged Inter TTC with six static Inter faces
     under DirectWrite and the same TTC under Qt GDI in fresh x64 Release
     processes; record source/version/hash/license, backend, face resolution,
     and per-registration/cumulative WS/private metrics.
   - Result: seven fresh processes per arm. The TTC under DirectWrite added a
     median 489,861,120 WS / 493,707,264 private bytes. Six original static
     Inter v4.0 TTFs under DirectWrite added 6,287,360 / 6,213,632 bytes
     cumulatively. The TTC under GDI added 405,504 / 0 bytes. All six requested
     styles resolved. The independent Tester validated all 21 processes,
     backends, face results, asset hashes, and deltas; the PowerShell warning
     was fixed by removing the obsolete parameter, not suppressing it.
   - The static files are v4.000 while the bundled TTC is v4.001, so do not
     integrate them as an exact-version replacement yet. GDI changes text
     rendering across the app and remains a fallback candidate. See the Phase
     9 progress log for run IDs and full provenance. This probe is diagnostic
     only and did not run the packaged P9-06 gate.
5. **P9-06U — Test exact-version Inter static faces** (Diagnostic complete)
   - Acceptance: fetch the six original static TTFs from official Inter v4.1,
     verify archive/member SHA-256, metadata, license, and source version
     against the bundled TTC's 4.001 tag. The independent Tester verified the
     six faces' required styles, Unicode coverage, glyph outlines, and
     horizontal metrics, with only head-table checksum/timestamp differences.
     In seven fresh x64 Release processes, median cumulative registration was
     6,328,320 WS / 6,172,672 private bytes for the six static faces, versus
     489,906,176 / 493,772,800 bytes for the TTC under DirectWrite. This is
     diagnostic only; the product package and P9-06 gate were unchanged. Run
     IDs, hashes, and limitations are recorded in the Phase 9 progress log.
6. **P9-06V — Register exact-version Inter static faces and rerun the gate**
   (Accepted; strict normal-use memory gate passed)
   - Acceptance: package the six verified Inter v4.1 static TTFs with their
     exact hashes and license; preserve the six required styles, family
     behavior, coverage, font-registration lifetime and cleanup, and leave
     Pretendard unchanged. Build affected targets and run focused Windows x64
     Debug tests. Then run the unchanged locally packaged Windows x64 Release
     P9-06 gate for both fixtures at 301 samples each. All six gated working
     set points must be strictly below 262,144,000 bytes for a pass; preserve
     all artifacts and report valid over-budget evidence as a failure.
   - Result: the generated package contains the six verified v4.1 TTFs and
     Inter OFL license (no TTC alias); Pretendard is unchanged. The focused
     P9 validator suite passed 44/44, and the Phase 4 suite passed 34 tests
     with one expected Windows symlink-privilege skip. Four affected Debug
     CTests passed; the ResourceLoader test's symlink assertion was skipped
     because Windows symlink creation was unavailable. The unchanged Release
     gate completed two valid 301-sample traces and passed all six strict
     checkpoints. Final run ID, hashes, and values are in the progress log.
7. **P9-07 — Normal navigation and five-minute idle** (Accepted; strict packaged gate passed)
   - Acceptance: locally packaged Windows Release exercises repeated normal
     navigation, proves the document catalog has no loaded PDF body, and
     enforces the same strict normal-use working-set limit after navigation
     and idle.
   - Result: final accepted run `20261118T092055381Z-e63554e873664d1eaface390ddc4ec9e`
     completed 10 exact cycles and 40 authentic page transitions per fixture,
     followed by 301 samples over five minutes. All gated working sets were
     below 262,144,000 B; maxima were 220,250,112 B (empty) and 224,305,152 B
     (96-class). Both catalogs reported zero loaded PDF bodies. Independent
     Tester verified the run, source/package/compiler identities, cadence,
     and all gate points. The current tree differs from the passing inventory
     only in test-only `tests/next/calendar_page_tests.cpp`; package, Release
     executable, cache, and production inputs match. Two subsequent UIA click
     timeouts remain invalid/not evaluated. Font-bound paired captures and
     rendered-pixel suffix checks pass for all eight English/Korean contexts;
     focused Debug CTest passed 1/1. The representative-only 30-cycle
     diagnostic shows a small positive but decreasing residual trend; no
     allocator cause or long-term plateau is claimed. Details and run IDs are
     in the Phase 9 progress log.
8. **P9-08 — 96-class Sub Prep lifecycle and matched cleanup** (Active)
   - Acceptance: the locally packaged Windows Release route covers entry,
     summary creation, first selection, refresh, package generation, leave,
     and repeated re-entry. Capture peak and settled process metrics plus
     bounded model/widget/editor counts and a matched pre/post-release memory
     pair. Keep it advisory until the return allowance is defined from
     current-source Windows Release runs.

P9-07 is the second-to-last candidate in Batch 2, so Batch 3 discovery began
when P9-07 started. Each slice requires its assigned local Windows
checks/evidence and its own commit; push once after Batch 2 closes. GitHub
validation is not part of current acceptance. Scenario measurements do not
replace later full-route gates.

## Phase 9 — Batch 3: Additional packaged Release lifecycle evidence (Discovered; queued)

Discovery began when P9-07 started, as required. The Explorer and independent
Investigator reviewed the Phase 9 measurement list, the Phase F memory
remediation routes, and existing Phase 0 evidence routes. No numbered P9-09+
order existed. The following ten remaining candidates are ordered by current
feature boundary and reusable route; recheck for overlap with P9-07/P9-08
before implementation. The full Windows Debug CTest and final packaged
Release exit gates remain separate from these feature slices.

9. **Large Schedule Import lifecycle** — Parse a large fixture, exercise
   conflict review, cancel and apply paths, and verify return/cleanup.
   Existing Phase 0 routes: `lifecycle-schedule-import`,
   `lifecycle-schedule-import-apply`.
10. **Large Calendar Import and open/close** — Measure successful import,
    parser-error cleanup, and calendar entry/leave.
    Existing routes: `lifecycle-calendar-import`,
    `lifecycle-calendar-import-error`.
11. **Class Transfer export/import lifecycle** — Cover export, import, cancel,
    and released state. Existing route: `lifecycle-class-transfer`.
12. **QtPdf viewer lifecycle** — Open, render, close, release, and reopen a
    PDF; record document/page and retained-object state. Existing route:
    `visual-pdf-viewer`.
13. **Speaking/report generation and preview close** — Verify output and
    cleanup, excluding any report/PDF output already accepted in P9-08.
    Existing route: `lifecycle-speaking-evaluation`.
14. **Staff Directory and AI batch review** — Measure staff refresh/re-entry
    and large AI batch review. Staff route exists; AI batch coverage needs an
    explicit route or documented route mapping.
15. **Schedule refresh and language/theme cycles** — Repeat schedule refresh,
    language change, and theme rebuild with bounded retained objects.
    Existing route: `lifecycle-schedule`; locale/theme coverage must be made
    explicit.
16. **Repeated workspace open/close** — Verify matched before/after process
    memory and no growth across repeated cycles.
17. **Large roster editing** — Measure large edit operations, peak use, and
    cleanup; no named Phase 0 memory route was found.
18. **Campus map open/close** — Measure decoded map retention and release;
    no named Phase 0 memory route was found.

Shared acceptance for each candidate: current-source packaged Windows x64
Release evidence, identified large fixtures, process/memory metrics, bounded
widget/editor and model counts where relevant, and a measured return-to-baseline
allowance. Keep transient budgets advisory until that allowance is established.
The source review found no additional named candidates beyond these ten in the
current Phase 9 and Phase F lists; confirm that finding when Batch 3 activates.
