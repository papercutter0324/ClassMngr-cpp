# Current Slice Batches

## Phase 8a — Linux local candidate closed; macOS deferred

Batch 2’s 18-slice sub-batch and final one-candidate W16 sub-batch are
complete. The first sub-batch was pushed at `cbb0939c`. W16 has local macOS
verification and is included in the Windows full-suite run below. No other
OS-agnostic slices were found.

Batch 3's clean Windows x64 candidate passed at `bb37cf9aaa4562b4fdf9c9b592df2012928bf5c6`: full Debug build, 618/618 CTest targets, Schedule, Speaking, four Roster layouts, and C53 (12 Sub Prep plus 5 Roster pages, zero changed pixels). The clean provenance and QWARN results are recorded in the [Phase 8a progress log](09.5-Phase-8a-Progress-Log.md).

The local Linux clean candidate at `e44f400c826250e7f4645e0e5a2c7f8df8163e54` passed 619/619 CTest targets, required output evidence, C53, and focused adapter probes. Evidence and limits are recorded in the [Phase 8a progress log](09.5-Phase-8a-Progress-Log.md). The user deferred the native macOS candidate to a later local session/device. No Phase 8a batch is active; hosted Ubuntu evidence and cross-host gate review remain outstanding. See the [Phase 8a plan](09.5-Phase-8a-CI-Failure-and-QWARN-Triage.md) for acceptance.
