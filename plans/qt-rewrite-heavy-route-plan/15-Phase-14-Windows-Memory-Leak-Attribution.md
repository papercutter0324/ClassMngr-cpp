# Phase 14 — Windows Memory Leak Attribution

Read [00-Start-Here.md](00-Start-Here.md) first for the overall plan, workflow,
and phase sequence.

## Status

- Status: Not started
- Default route: Heavy
- Depends on: Phase 13; uses Phase 9 evidence
- Blocks: Resolution of all deferred Phase 9 work and final gates
- Owner: Unassigned
- Last updated: 2026-11-25
- Current note: This phase remains Not started until Phase 13 is complete. The
  user deferred the remaining Phase 9 work here: P9-17 plateau, P9-18 cold
  cleanup, P9-19 plateau, conditional P9-20, P9-21, full Windows x64 Debug
  CTest, and final packaged Release exit gates. None is accepted by deferral.
  P9-19's target, operation-advisory, and cold-cleanup gates passed, but its
  plateau failed; nested cleanup reports passed=false while the report's
  formula and top-level status incorrectly say true. Repair and verify status
  propagation before revalidation. Preserve current Maps page retention and
  all original thresholds and formulas. Revalidate after Phase 13 under the
  unchanged contracts. See the [Phase 9 progress log](10-Phase-9-Progress-Log.md).

## Progress log location

Write Phase 14 implementation, verification, attribution, and acceptance
history to the [Phase 14 progress log](15-Phase-14-Progress-Log.md). Keep
current status and contracts here.

## Objective

Own the Phase 9 work explicitly deferred here after Phase 13: revalidate
P9-17 plateau, P9-18 cold cleanup, and P9-19 plateau; handle conditional P9-20
Schedule Import apply and P9-21 PrivateUsage trend follow-up; and run the full
Phase 9 Windows x64 Debug CTest and final packaged Release exit gates. Distinguish
surviving application allocations from allocator retention, Qt/resource-cache
growth, and route-specific lifecycle effects. Preserve current Maps page
retention and fix every confirmed leak in its owning module.

## Sequence and scope

Phase 14 remains Not started until Phase 13 is complete. By user direction,
it owns the remaining Phase 9 work: P9-17 plateau and P9-18 cold cleanup (both
already deferred and unaccepted), P9-19 plateau cleanup, conditional P9-20
larger successful Schedule Import apply, P9-21 PrivateUsage trend follow-up,
the full Windows x64 Debug CTest, and final packaged Release exit gates. None
is accepted by deferral. Preserve current Maps page retention. P9-19 must not
be revalidated until the validator propagates nested cleanup failures to
settledMemoryAndCleanupFormulas and the top-level report status, with its
tests and retained logs reviewed. After any confirmed leak is fixed, use fresh
exact-source packaged Release evidence to revalidate each applicable route
under its unchanged original gate. No threshold, formula, allowance, or gate is
waived.

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
- After repairing and verifying P9-19 validator status propagation, a fresh
  exact-source packaged Release route passes the original plateau-cleanup
  formula. Nested failures must fail the formula and top-level report; a failure
  leaves P9-19 open.
- If a suitable larger valid Schedule Import fixture exists, complete P9-20
  with parse, review, apply, cleanup, and persisted-association evidence where
  feasible. If no suitable fixture exists, document that limitation and leave
  the slice conditional.
- Complete the P9-21 PrivateUsage trend follow-up against accepted P9-16
  evidence; report the trend as diagnostic and do not label it a leak without
  ownership evidence.
- Run the full Phase 9 Windows x64 Debug CTest and final packaged Release exit
  gates under their original contracts. Any failed or incomplete gate remains
  open; deferral itself is not acceptance.

## Heavy-route requirements

- Use the Heavy route for all Phase 14 implementation, diagnosis, and
  revalidation work.
- Preserve the packaged Release workload and database/UI correctness while
  collecting ownership evidence.
- Keep measured facts, hypotheses, and unverified harness results distinct.
