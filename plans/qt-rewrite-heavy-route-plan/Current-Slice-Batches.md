# Current Slice Batches

This is the single live tracker for slice batches across the Qt rewrite. Keep
only the active batch and the next batch after its discovery starts. Replace
closed batches as work advances; do not maintain phase-specific batch files or
an archive of completed batches. Keep slice requirements in the relevant phase
plan, and record implementation and verification evidence in that phase's
existing progress log when useful.

## Active phase: Phase 2 — Domain Model and Application Contracts

### Current batch: Batch543 — F1316 in progress

- F1316 is the only accepted candidate currently recorded in Batch543. Its
  implementation is in progress; acceptance criteria are in the [Phase 2
  plan](03-Phase-2-Domain-Model-and-Application-Contracts.md#deferred-candidates).
- The Batch543 candidate scan and cumulative Gate 1/2 audits remain open.
- Gate 1 and Gate 2 remain Partial; the Phase 2 exit gate remains Open. F629's
  source-bound Windows x64 evidence remains separately outstanding.

### Upcoming batch: Batch544 — discovery complete; no candidates

- Discovery began when F1316 work started, as Batch543 contains one candidate.
- The feature and independent Application-contract lanes completed at source
  tip `3a5a5e13`; neither found another ready slice.
- No other slices were found.
