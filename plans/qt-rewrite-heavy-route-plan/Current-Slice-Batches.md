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
4. **P9-06T — Compare Inter faces and Qt font backends**
   (Proposed diagnostic slice)
   - Acceptance: compare the packaged Inter TTC registered under the current
     DirectWrite backend, the six unmodified version-matched v4.0 static TTF
     faces requested by source usage under DirectWrite, and the same TTC under
     Qt's GDI backend. The six exact styles are Regular 400, Medium 500,
     SemiBold 600, Bold 700, BoldItalic 700, and Black 900 upright. First
     verify official source archive, each asset hash, license, and Qt backend.
     Use repeated fresh x64 Release processes; record per-registration and
     cumulative WS/private metrics, family/style resolution, and environment.
     Preserve all artifacts. This diagnostic does not alter product assets or
     pass the normal-use budget. If an arm can plausibly save at least 426 MB,
     implement that candidate separately and run the unchanged packaged P9-06
     gate before claiming a fix.

5. **P9-07 — Normal navigation and five-minute idle** (Proposed)
   - Acceptance: locally packaged Windows Release exercises repeated normal
     navigation, proves the document catalog has no loaded PDF body, and
     enforces the same strict normal-use working-set limit after navigation
     and idle.
6. **P9-08 — 96-class Sub Prep lifecycle and matched cleanup** (Proposed)
   - Acceptance: the locally packaged Windows Release route covers entry,
     summary creation, first selection, refresh, package generation, leave,
     and repeated re-entry. Capture peak and settled process metrics plus
     bounded model/widget/editor counts and a matched pre/post-release memory
     pair. Keep it advisory until the return allowance is defined from
     current-source Windows Release runs.

When P9-07 starts, begin discovery of the next batch because it will be the
second-to-last candidate in this active batch. Each slice requires its assigned
local Windows checks/evidence and its own commit; push once after Batch 2 closes.
GitHub validation is not part of current acceptance. Scenario measurements do
not replace later full-route gates.
