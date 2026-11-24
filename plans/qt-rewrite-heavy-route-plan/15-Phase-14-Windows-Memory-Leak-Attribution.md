# Phase 14 — Windows Memory Leak Attribution

Read [00-Start-Here.md](00-Start-Here.md) first for the overall plan, workflow,
and phase sequence.

## Status

- Status: Not started
- Default route: Heavy
- Depends on: Phase 13; uses Phase 9 evidence
- Blocks: Resolution of the deferred P9-17 plateau and P9-18 cold cleanup
  requirements
- Owner: Unassigned
- Last updated: 2026-11-24
- Current note: This phase remains Not started until Phase 13 is complete.
  P9-17 plateau cleanup and P9-18 cold cleanup are Deferred and unaccepted by
  user direction; Phase 9 remains In progress through Batch 4 and its final
  gates. P9-18 attempt #7 passed all non-memory gates and plateau cleanup but
  failed cold cleanup by 10,809,344 B. Preserve the current Maps page
  retention. UIA and image-lifecycle evidence support first-use retention
  during Map activation/setup but identify no owner. P9-17's plateau gate also
  remains failed. WPR capture did not start, and the full-versus-3-row
  comparator has no live result, so neither identifies an owner. Revalidate
  both deferred gates here under their unchanged contracts; no result is
  accepted by deferral. The stopped MAPSTAGE probe has no attribution result:
  its diagnostic changes were removed, no route or stage JSONL was produced,
  and its receipt is stale after rollback. Five prescribed focused CTests
  passed, but the extra Campus Directory CTest failed after 19.22 seconds with
  no output; prior Python/parse checks were not rerun after rollback. After
  Phase 13, use a fresh diagnostic implementation and build on current source.
  See the [P9-18 route validation](../../artifacts/phase9/packaged-next-p9-18-campus-map/20261124T122412350Z-d59e56771be74f66ad00b21cbaad38e5/validation.json),
  [MAPSTAGE build receipt](../../artifacts/phase9/p9-18-exact-release-builds/20261124T133334000Z-MAPSTAGE/release-build-receipt.json),
  and [Phase 9 progress log](10-Phase-9-Progress-Log.md).

## Progress log location

Write Phase 14 implementation, verification, attribution, and acceptance
history to the [Phase 14 progress log](15-Phase-14-Progress-Log.md). Keep
current status and contracts here.

## Objective

Attribute the PrivateUsage growth and cleanup failures observed in the P9-17
large-roster and P9-18 campus-map routes. Distinguish surviving application
allocations from allocator retention, Qt or resource-cache growth, and route-
specific lifecycle effects. Preserve current Maps page retention. Fix every
confirmed leak in its owning module.

## Sequence and scope

Phase 14 is scheduled after Phase 13 and uses recorded Phase 9 evidence. It
owns revalidation of the user-deferred P9-17 plateau cleanup and P9-18 cold
cleanup gates; both remain failed and unaccepted until they pass their original
contracts. Diagnosis alone does not resolve either requirement. Preserve the
current Maps page retention while investigating P9-18. After any confirmed
leak is fixed, use fresh exact-source packaged Release evidence to revalidate
both routes under their unchanged gates before claiming either requirement
resolved.

The stopped P9-18 MAPSTAGE probe is not reusable diagnostic evidence: its
instrumented changes were removed, the build receipt binds the reverted stage
source, and no route or stage JSONL exists. After Phase 13, implement and
verify a fresh diagnostic on current source, build it with a source-bound
receipt, and capture a route with stage records before drawing ownership
conclusions. Preserve current PageHost/Maps retention throughout.

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
- A fresh exact-source P9-18 packaged Release route passes its existing cold
  cleanup gate and all other applicable route gates under the unchanged
  contract while preserving current Maps page retention. A failure leaves the
  deferred requirement open.

## Heavy-route requirements

- Use the Heavy route for all Phase 14 implementation, diagnosis, and
  revalidation work.
- Preserve the packaged Release workload and database/UI correctness while
  collecting ownership evidence.
- Keep measured facts, hypotheses, and unverified harness results distinct.
