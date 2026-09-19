# Phase 0 Packaged Release Memory Thresholds

Status: Phase 0 baseline and trend contract recorded; the final end-of-rewrite
target is intentionally not a current legacy-route acceptance gate.

The `<250 MiB` value is the target RAM usage for the completed Qt rewrite. It
is not a requirement that the current legacy widget graph already satisfy in
Phase 0. Phase 0 records the distance to that target on real packaged Release
heavy routes. Each later phase must avoid regressions and should reduce the
working set at the checkpoints affected by that phase, with the final gate
applied after the rewrite is complete.

The primary metric is Windows working set from a packaged x64 Release process.
Private bytes, commit, peak working set, handles, and threads remain required
secondary measurements. All comparisons below are strict: a value equal to a
limit fails.

| Scope | Limit | Phase 0 role | End-of-rewrite interpretation |
| --- | ---: | --- | --- |
| Normal resident memory | `< 262,144,000` bytes (`250 MiB`) | Compare every retained packaged Release route and preserve the pass/fail result as baseline and trend evidence; legacy routes are not required to pass yet | Final normal-use gate for startup-ready, first rendered page, normal navigation settled, idle, and post-operation/page-release steady state in empty, representative, and migrated workspaces |
| Transient large-operation memory | `< 536,870,912` bytes (`512 MiB`) | Temporary diagnostic ceiling for bounded heavy operations and regression detection while the rewrite is in progress | A bounded operation must release its owned representations and return toward the final normal resident target; `512 MiB` is not the rewrite's final RAM goal |

The transient ceiling is intentionally distinct from the final normal resident
target. It is a temporary alarm for a bounded operation, not permission to
retain a full page or data graph after the operation ends. The current
before-state artifacts show a highest measured operation working set of
`489,000,960` bytes in the large Sub Prep output route, so `512 MiB` provides a
finite diagnostic boundary with approximately `47.9 MiB` of headroom while
the v2 operation contract is implemented. A future operation that needs a
larger ceiling requires an explicit Phase 0 decision and updated evidence; it
may not silently increase the limit after a failure.

Current legacy classifications:

- The representative packaged workflow remains below the final normal target
  at its five-minute settled sample (`250,036,224` bytes); its recorded peak
  (`259,510,272` bytes) is retained as a close trend boundary.
- The 96-class Sub Prep selected/changed/empty visual routes currently exceed
  the final normal target at their retained peak checkpoints but remain below
  the temporary transient ceiling. Their manifest records the final-target and
  transient-ceiling comparisons as observational booleans. This is expected
  legacy before-state evidence, not a Phase 0 failure or a v2 acceptance
  result.
- The large Sub Prep refresh/re-entry, report, import, transfer, staff, and
  output artifacts remain before-state evidence. A completed v2 route must
  pass the final normal resident target after leaving/releasing the feature
  and stay below any bounded-operation ceiling during the operation.

Every threshold result must retain the exact Release executable, fixture,
checkpoint names, process outcome, and failure/high-memory artifact. Debug
measurements may explain behavior but cannot satisfy this contract.
