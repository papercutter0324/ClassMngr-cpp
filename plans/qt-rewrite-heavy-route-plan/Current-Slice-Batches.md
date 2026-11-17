# Current Slice Batches

## Phase 9 — Batch 1: Common lifecycle measurement instrumentation (Active)

Batch goal: join the existing process snapshot provider and Next-owned startup,
page, resource, and document lifecycle boundaries into reviewable memory
evidence. Instrumentation slices must preserve application behavior and clearly
label unavailable or platform-specific values. If evidence exposes a feature
allocation or cleanup defect, route that fix to its owning feature boundary.

1. **P9-01 — Next startup process-memory checkpoints** (Locally accepted;
   hosted verification pending)
   - Acceptance: every named Next startup-stage event writes a structured
     checkpoint record with the stage, outcome, sample availability and
     platform, and all available process snapshot counters with units. Track
     counter-level availability: absent counters serialize as explicit JSON
     nulls, never measured zero. A focused test proves deterministic
     serialization using an
     injected snapshot provider. Reuse the existing provider through a narrow
     QtCore-level target shared by Legacy and Next; do not link broad legacy
     Core into Next. No startup ordering or feature behavior changes.
   - Verification: independent focused macOS Debug build/test passed on the
     standard preset; batch-level hosted Windows/Linux Debug and packaged
     Windows Release builds remain pending after the batch push. Windows
     memory values remain measurement evidence, not a Phase 9 exit-gate claim
     until scenario coverage and thresholds pass.
2. **P9-02 — Next page and model lifecycle trace** (Locally accepted;
   hosted verification pending)
   - Acceptance: page creation, activation, suspension, and release are tied to
     named process snapshots and report relevant model row counts and bounded
     table/editor counts without owning feature data. Include an explicit
     release/recreate pass for the representative Classes page and verify the
     retained model/editor state is cleared and rebuilt as expected. Record
     actual QObject destruction, including shell shutdown, with page-object
     metrics unavailable at the destruction event. Bound object traversal,
     view-cell scans, and per-model JSON detail.
3. **P9-03 — Resource and cache lifecycle trace** (Locally accepted;
   hosted verification pending)
   - Acceptance: production `ResourceLoader` events report request-scope and
     catalog owners, resource kind/type, outcomes, elapsed time, source and
     decoded payload bytes, decoded image dimensions, and font family names.
     Include cache hit/entry owner/kind/accounted bytes/capacity when configured;
     report cache data as unavailable when absent and do not add a production
     cache. Cover `resolveFile()`. Keep the sink exception-safe and thread-safe,
     and label payload accounting as distinct from resident allocation.
   - Begin discovery of Batch 2 when this slice starts, before work turns to
     the broader scenario gates; keep only the active batch and the next
     discovered batch here.
4. **P9-04 — QtPdf and output lifecycle trace** (Locally accepted;
   hosted verification pending)
   - Acceptance: document load/status/page/render/close/release/reopen and
     PDF/export object lifetimes are observable in the shared trace, including
     release after leaving or closing the owning view/operation.
   - Verification: independent macOS Debug build and focused CTest passed
     5/5, including a 40-selection check proving 32 page-detail events and 8
     suppressed details. `git diff --check` passed. QtPdf view records page
     selection rather than internal render completion, which QtPdf does not
     expose at this boundary. Hosted Windows/Linux checks remain pending.
5. **P9-05 — Source-bound Windows Release evidence** (In progress)
   - Acceptance: push-triggered packaging checks out the exact commit that
     triggered the workflow, reports the candidate SHA, and fails before
     packaging if the checked-out SHA differs. Manual dispatch may continue to
     select an explicit source ref.

## Phase 9 — Batch 2: Packaged Release scenario gates (Discovered)

Batch goal: turn the common lifecycle trace into source-bound Windows Release
measurements and enforce normal-use budgets with complete, reviewable failure
artifacts. Do not claim a transient operation budget until its return-to-baseline
allowance has been measured and recorded.

1. **P9-06 — Empty and representative startup-ready/idle gate** (Proposed)
   - Acceptance: packaged Windows Release records startup-ready and five-minute
     idle working set for empty and representative workspaces. Enforce the
     strict `<262,144,000` byte normal-use limit. Keep fresh Initial Setup
     advisory. Complete over-budget evidence fails the gate and preserves all
     artifacts; incomplete or invalid evidence is not evaluated.
2. **P9-07 — Normal navigation and five-minute idle** (Proposed)
   - Acceptance: packaged Release exercises repeated normal navigation, proves
     the document catalog has no loaded PDF body, and enforces the same strict
     normal-use working-set limit after navigation and idle.
3. **P9-08 — 96-class Sub Prep lifecycle and matched cleanup** (Proposed)
   - Acceptance: the packaged route covers entry, summary creation, first
     selection, refresh, package generation, leave, and repeated re-entry.
     Capture peak and settled process metrics plus bounded model/widget/editor
     counts and a matched pre/post-release memory pair. Keep it advisory until
     the return allowance is defined from current-source Windows Release runs.

Batch completion requires each slice's focused acceptance and commit, then one
push for the batch. The hosted Windows package measurement is required evidence
for the completed batch; it does not replace later full-route scenario gates.
