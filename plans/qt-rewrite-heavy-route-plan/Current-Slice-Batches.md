# Current Slice Batches

Phase 5 completed on the mandatory Heavy route at implementation head
`6bbff54abc524723e23911d65130b1b198dde2b3`; F1501–F1513 are complete.

## Phase 6 — Batch 1 (active)

F1514–F1524 are complete. F1525 is current. Continue in order, commit each
accepted slice, then push the batch. Start Batch 2 discovery when F1526 starts
because it is the second-to-last candidate.

1. **F1514 — Page descriptors and lazy registration (complete):** stable page
   identity and descriptor metadata integrated with `PageHost`; registration
   remains construction-free.
2. **F1515 — Explicit lifecycle state (complete):** declared, created, activated,
   suspended, and released states with correct cleanup and recreation.
3. **F1516 — Navigation model (complete):** current location, hierarchy, history,
   selection, permission filtering, initial page, and placeholder shell
   navigation. Keep the ordered group/page tree separate from `PageDescriptor`;
   page nodes reference descriptor IDs, so labels, icons, and requirements have
   one source. Model workspace availability separately from explicit permission
   grants; do not infer permissions from the legacy admin flag. Use workspace
   state for visibility and the initial route (`campus_information` when closed,
   `my_workspace` when open). Add page-ID history and a value-only navigation
   guard; commit location, selection, and history only after successful
   activation. Legacy sidebar visibility depends on workspace-open state, not
   admin roles; it has no general back/forward history, and its unsaved-change
   guards are route-specific. Preserve rollback without migrating feature-page
   loading. The legacy sidebar assigns no node icons, so keep them absent unless
   a descriptor supplies a resource ID. Defer dynamic document nodes, teacher
   rows/IDs, external URL actions, and feature data/loading to their migration
   slices.
4. **F1517 — Command bus (complete):** widget-free application `CommandBus`
   with typed registration/dispatch keyed by C++ command type, a typed
   `canExecute` predicate, and handlers returning `Domain::Result<void>`.
   Keep missing/duplicate/unavailable bus errors distinct from operation
   errors; recheck availability during synchronous dispatch. Checked state and
   navigation remain outside the bus; feature command handlers migrate later.
5. **F1518 — Action presentation (complete):** derive enabled state for the
   existing Back/Forward shell actions from navigation history, workspace
   visibility, and explicit permission state. Disable an action when its
   history target is no longer visible or permitted, and refresh after route
   or access-context changes. Keep a value-only state rule with a thin shell
   presenter; no generic QAction registry or checked state is needed until
   feature actions migrate. CommandBus and navigation remain separate.
6. **F1519 — Semantic prompt service (complete):** Qt-free typed contracts for
   messages, confirmations, unsaved changes, and named actions, with explicit
   user responses separate from presenter errors. A Next Qt presenter preserves
   default/Escape mappings; its separate automation API uses stable prompt IDs
   and value-only snapshots/commands. Legacy callers remain unmigrated.
7. **F1520 — File dialog service (complete):** Qt-free typed open-one,
   open-many, save, and directory requests/responses over UTF-8 paths, with a
   separate Qt presenter. Preserve explicit/remembered/default directory
   precedence, filters, suggested save name/suffix, overwrite policy,
   Open/Save/Choose Directory accept behavior, path normalization, and
   per-purpose directory memory. Cancellation is a normal empty selection;
   preference-write failures accompany accepted selections as warnings.
   PlatformDefault and the optional Qt-only open-after-save accessory remain.
8. **F1521 — Notification service (complete):** Qt-free message/severity port
   backed by a shell-owned QStatusBar presenter. Notices replace the current
   message, clear explicitly, and expire after a finite five-second default;
   the status bar stays hidden while idle. Prompts remain for decisions and job
   progress remains separate; no feature callers were migrated.
9. **F1522 — Preferences model (complete):** Qt-free coordinator around the
   existing validated `UserPreferencesState`, with distinct read, in-memory
   apply, whole-snapshot persist, dirty tracking, and synchronous observer
   notifications. Failed reads/writes preserve model state; writes are retried
   while dirty. Changed mutations during observer delivery return `Conflict`;
   no-op apply remains a no-op. Existing storage adapters and consumers remain
   unchanged.
10. **F1523 — Theme presenter (complete):** Next startup and live theme changes
    use the existing light/dark QSS through a resource-owning presenter. Stored
    theme values map explicitly; startup does not rewrite storage, live changes
    persist before style replacement, and failures keep the current theme.
    Palette and QSS are unchanged. SystemDefault samples the color scheme when
    applied; later OS color-scheme changes are not tracked, matching current
    Next startup behavior.
11. **F1524 — Other presentation preferences (complete):** add typed font size
    to the shared preferences state, compose the existing preference ports into
    a complete SettingsManager snapshot reader/writer, and use one model for
    startup language/theme/font selection and future page access. Snapshot
    writes are sequential and non-atomic; reported failures keep the model dirty
    for full-snapshot retry, while legacy void writers cannot report errors.
    Global window/splitter persistence remains deferred because it has no active
    behavior to preserve; workspace current-campus data and keyed dialog
    geometry remain separate.
12. **F1525 — Shared model/view primitives (current):** reusable bounded models,
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
