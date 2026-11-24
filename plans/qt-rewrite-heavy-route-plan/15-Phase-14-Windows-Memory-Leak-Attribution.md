# Phase 14 — Windows Memory Leak Attribution

Read [00-Start-Here.md](00-Start-Here.md) first for the overall plan, workflow,
and phase sequence.

## Status

- Status: Not started
- Default route: Heavy
- Depends on: Phase 13; uses Phase 9 evidence
- Blocks: Resolution of the deferred P9-17 cleanup requirement
- Owner: Unassigned
- Last updated: 2026-11-24
- Current note: This phase follows Phase 13. P9-17 is Deferred and unaccepted
  after its plateau cleanup gate failed; Phase 9 remains In progress and P9-18
  may proceed. WPR capture did not start, so there are no allocation stacks,
  snapshots, or ETL. The full-versus-3-row comparator contract passed five
  independent focused tests, but no live arms ran, so there is no cardinality
  result or leak claim. P9-18 build #7 succeeded, but its packaged route has
  not run while the Executor waits for the user-requested root `build/` cleanup
  at UAC; no memory samples or gate result exist. See the [Phase 9 progress
  log](10-Phase-9-Progress-Log.md).

## Progress log location

Write Phase 14 implementation, verification, attribution, and acceptance
history to the [Phase 14 progress log](15-Phase-14-Progress-Log.md). Keep
current status and contracts here.

## Objective

Attribute the PrivateUsage growth and cleanup failure observed in the P9-17
large-roster route. Distinguish surviving application allocations from
allocator retention, Qt or resource-cache growth, and roster model or data
cardinality. Fix every confirmed leak in its owning module.

## Sequence and scope

Phase 14 is scheduled after Phase 13 and uses the recorded Phase 9/P9-17
evidence. It does not accept P9-17 or close its cleanup requirement by
diagnosis alone. After any confirmed leak is fixed, revalidate the P9-17
cleanup requirement under its unchanged gate before claiming it resolved.

Use an exact-source packaged Windows x64 Release build and repeatable lifecycle
evidence. Establish that heap tracing or equivalent ownership capture is
actually running and bound to the process and route before interpreting its
results. If WPR/UAC capture remains unavailable, use a reviewed alternative;
process counters alone cannot identify an allocation owner.

Compare surviving/live allocations and ownership across route cycles with
Working Set and PrivateUsage. Record Qt/resource cache counts and bytes, roster
row/model/snapshot cardinality, and allocation/release behavior at matching
lifecycle points. Use controlled cases that keep geometry, names, probes, and
actions fixed when isolating one variable. The filled-versus-compact-text
diagnostic indicates text length has little effect but does not isolate row
count or prove leak ownership. The full-versus-3-row comparator contract
received independent focused review and passed five focused tests, but its live
arms have not run; it has no cardinality result and does not support a leak
claim. Run matched live arms before using that comparison's results.

Keep the Phase 9 policy unchanged: strict 268,435,456 B (256 MiB) Working Set
target, inclusive 471,859,200 B (450 MiB) ceiling, separate 536,870,912 B
(512 MiB) operation advisory, and existing cold and plateau cleanup gates.
Phase 14 may fix a confirmed owner; it may not alter thresholds, allowances,
or cleanup criteria.

## Exit evidence

- A usable allocation capture or reviewed equivalent is tied to the exact
  source, executable, process, and workload; state clearly if any capture
  method failed. Do not claim allocation stacks without captured stacks.
- Reproducible evidence distinguishes live leaks, allocator retention,
  Qt/resource cache growth, and roster model/cardinality effects, and names
  the confirmed owner or records which hypotheses remain unresolved.
- Every confirmed application leak is fixed in its owning module and covered
  by appropriate regression evidence.
- A fresh exact-source P9-17 packaged Release route passes the existing cold
  and plateau cleanup gates under the unchanged contract before the cleanup
  requirement is declared resolved. A failure leaves that requirement open.

## Heavy-route requirements

- Use the Heavy route for all Phase 14 implementation, diagnosis, and
  revalidation work.
- Preserve the packaged Release workload and database/UI correctness while
  collecting ownership evidence.
- Keep measured facts, hypotheses, and unverified harness results distinct.
