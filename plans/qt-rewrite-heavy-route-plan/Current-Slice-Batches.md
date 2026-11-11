# Current Slice Batches

## Phase 8a — Batch 2 complete

Batch 2’s 18-slice sub-batch and final one-candidate W16 sub-batch are
complete. The first sub-batch was pushed at `cbb0939c`. W16’s production/test
change has local macOS verification; see the progress log for evidence.

No other slices were found.

Phase 8a remains open because shared-source/C53 and platform gates are
unresolved. The recorded stable runs use different revisions (Mac `3dc37340`,
Linux `7f82f427`, Windows `b799c8ab`); there is no current Linux or Windows
W16 runtime confirmation. See the [Phase 8a plan](09.5-Phase-8a-CI-Failure-and-QWARN-Triage.md)
and [progress log](09.5-Phase-8a-Progress-Log.md).
