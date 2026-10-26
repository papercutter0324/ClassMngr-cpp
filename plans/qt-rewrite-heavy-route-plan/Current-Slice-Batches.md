# Current Slice Batches

Phase 5 completed on the mandatory Heavy route at implementation head
`6bbff54abc524723e23911d65130b1b198dde2b3`; F1501–F1513 are complete.

## Phase 6 — Batch 1 (active)

F1514 is complete. F1515 is current. Continue in order, commit each accepted
slice, then push the batch. Start Batch 2 discovery when F1526 starts because
it is the second-to-last candidate.

1. **F1514 — Page descriptors and lazy registration (complete):** stable page
   identity and descriptor metadata integrated with `PageHost`; registration
   remains construction-free.
2. **F1515 — Explicit lifecycle state (current):** declared, created, activated,
   suspended, and released states with correct cleanup and recreation.
3. **F1516 — Navigation model:** current location, hierarchy, history,
   selection, permission filtering, initial page, and placeholder shell
   navigation. Legacy sidebar visibility depends on workspace-open state, not
   admin roles; it has no general back/forward history, and its unsaved-change
   guards are route-specific. Preserve conditional initial selection and
   sidebar rollback without migrating feature-page loading.
4. **F1517 — Command bus:** typed command dispatch and results.
5. **F1518 — Action presentation:** derive enabled state from application state
   and permissions.
6. **F1519 — Semantic prompt service:** message/choice/unsaved-change/named-
   action contracts with a value-only Qt automation adapter.
7. **F1520 — File dialog service:** typed request/response adapter and current
   directory, filter, overwrite, default-action, and platform behavior.
8. **F1521 — Notification service:** presentation boundary and lifecycle.
9. **F1522 — Preferences model:** separate read, apply, persist, and notify
   behavior.
10. **F1523 — Theme presenter:** apply and persist theme without changing its
    appearance.
11. **F1524 — Other presentation preferences:** language, font, window,
    splitter, campus, and feature settings through the preferences boundary.
12. **F1525 — Shared model/view primitives:** reusable bounded models,
    delegates, on-demand editors, and refresh/loading states.
13. **F1526 — Representative-table gate evidence:** verify the existing Staff
    Directory model/view consumer demonstrates bounded object creation; keep
    its full feature migration in Phase 7C.
14. **F1527 — Appearance evidence:** capture shell/shared-UI baselines for
    supported themes and languages and verify labels, icons, focus, and
    selection behavior.

The initial source scan found no additional Phase 6 slice candidate. Recheck
for newly exposed work when Batch 2 discovery is due. Do not remove temporary
legacy prompt automation while its workflows remain unmigrated; those owners
are later feature-migration slices.
