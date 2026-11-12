# Current Slice Batches

## Phase 8a — Batch 3 closed; Batch 4 macOS next

Batch 2’s 18-slice sub-batch and final one-candidate W16 sub-batch are
complete. The first sub-batch was pushed at `cbb0939c`. W16 has local macOS
verification and is included in the Windows full-suite run below. No other
OS-agnostic slices were found.

Batch 3's clean Windows x64 candidate passed at `bb37cf9aaa4562b4fdf9c9b592df2012928bf5c6`: full Debug build, 618/618 CTest targets, Schedule, Speaking, four Roster layouts, and C53 (12 Sub Prep plus 5 Roster pages, zero changed pixels). The clean provenance and QWARN results are recorded in the [Phase 8a progress log](09.5-Phase-8a-Progress-Log.md).

Batch 4 macOS is next. Phase 8a remains open for shared provenance/capture work and clean macOS and Linux candidates. See the [Phase 8a plan](09.5-Phase-8a-CI-Failure-and-QWARN-Triage.md) for acceptance and the ordered remaining work.
