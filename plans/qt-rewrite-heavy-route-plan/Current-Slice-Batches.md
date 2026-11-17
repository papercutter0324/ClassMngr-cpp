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
2. **P9-02 — Next page and model lifecycle trace** (Discovered)
   - Acceptance: page creation, activation, suspension, and release are tied to
     named process snapshots and report relevant model row counts and bounded
     table/editor counts without owning feature data.
3. **P9-03 — Resource and cache lifecycle trace** (Discovered)
   - Acceptance: production diagnostics report resource load/cache owners,
     source and decoded bytes, image dimensions/decoded bytes, and font load
     events at the owning lifecycle boundary.
   - Begin discovery of Batch 2 when work starts on this second-to-last
     candidate; keep only the active batch and the next discovered batch here.
4. **P9-04 — QtPdf and output lifecycle trace** (Discovered)
   - Acceptance: document load/status/page/render/close/release/reopen and
     PDF/export object lifetimes are observable in the shared trace, including
     release after leaving or closing the owning view/operation.

Batch completion requires each slice's focused acceptance and commit, then one
push for the batch. The hosted Windows package measurement is required evidence
for the completed batch; it does not replace later full-route scenario gates.
