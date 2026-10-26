# Current Slice Batches

Phase 5 is active on the mandatory Heavy route. Phase 4 completed at F1500;
its native matrix passed before this batch began.

## Batch 1 — active

1. **F1501 — Complete:** Make `ClassMngrNext` a real Widgets
   startup composition with an owned minimal shell/page, typed diagnostics for
   the implemented stages, and coordinated shutdown. Keep `--version`
   side-effect-free and keep legacy startup out of the new bootstrap.
   Independent macOS Debug CTest passed 3/3; see the Phase 5 progress log.
2. **F1502 — Complete:** Define the full Phase 5 stage catalog,
   typed completion/warning/failure events, and monotonic legal transitions.
   Wire actual process/shell/page stages into the current bootstrap, but do not
   synthesize missing resource, settings, language, theme, font, workspace, or
   runtime events and do not emit `StartupReady` before those prerequisites are
   composed. Independent macOS Debug focused CTest passed 3/3.
3. **F1503 — Complete:** Compose the bootstrap-owned Qt startup diagnostics
   sink before resource initialization; log typed stage outcomes and attribute
   sink-construction failure to `CoreServicesCreated`. Independent focused
   CTest passed 3/3.
4. **F1504 — Complete:** Resolve the installed resource package
   from the executable directory, parse metadata only, and retain the Next
   `ResourceLoader` with an explicit `Startup` scope. Do not request payloads or
   fall back to the working directory, source tree, legacy RCCs, or resource
   packs. Resolution/catalog failures must fail `CoreServicesCreated` before
   the shell is built; verify success against the generated catalog with an
   empty payload root and verify the typed failure path. Six focused macOS
   Debug CTests passed 6/6.
5. **F1505 — Complete:** Preserve the existing settings identity,
   path override, preference keys, and migration semantics through a narrow
   Qt Core settings owner shared with the legacy runtime. Before shell creation,
   read startup preferences, install only the selected Next catalog `.qm`,
   apply only the selected packaged theme, and register/retain only Inter and
   Pretendard under the Startup scope. Emit SettingsLoaded → LanguageLoaded →
   ThemeLoaded → RequiredFontsLoaded only after each real operation. Do not use
   legacy LanguageService/ThemeService/FontManager RCC paths, optional fonts,
   or the broad Platform/Network target; do not emit WorkspaceOpened or
   StartupReady in this slice. Failure must be attributed to its stage and
   must not leave a partial translator/font/theme registration or shell.
   Independent macOS Debug verification passed bootstrap QtTest 22/22, focused
   bootstrap/preference CTest 3/3, and launch/filesystem CTest 2/2. Locale
   fallback and regional selection were exercised through a deterministic
   locale-list seam. No full suite ran.
6. **F1506 — Complete:** Preserve legacy workspace choice order:
   first supported command-line path; otherwise the first recent workspace
   when recent auto-open is enabled; use last-file only when the recent list is
   empty; no candidate is a valid no-workspace start. Preserve stale-first-MRU
   behavior by pruning the missing entry and stopping that attempt. Use one
   Next-owned `WorkspaceStore` through a dedicated store-backed adapter and the
   existing Next workspace contracts; do not construct `ApplicationServices`,
   link broad `Platform`/Network, or create a second store. Typed no-workspace
   and open-error warnings permit the empty shell to appear; retain a successful
   `WorkspaceSession` in Next state. Do not record `StartupReady` until the
   runtime, PageHost, and startup catalog are composed. Independent checks must
   cover policy order, session retention, no-workspace/open failure, and the
   no-create-on-missing-path boundary. Independent macOS Debug verification
   passed six focused QtTest cases (8/8 including setup/cleanup), the bootstrap
   and dependency-boundary CTests (2/2), and builds of ClassMngrNext and
   ClassMngr. Link inspection confirmed no Network or broad legacy target.
7. **F1507 — Complete:** Add a Qt-free `ApplicationRuntime` that
   owns long-lived application state and use-case composition: the F1506
   workspace state/session, selection state, workspace use case, and workspace
   coordinator. It receives the abstract `WorkspaceGateway` port; the bootstrap
   remains the sole root that builds the concrete store adapter and runtime.
   Make the runtime the single session authority rather than keeping a second
   copy in `ApplicationBootstrap`. Do not add feature-page state, PageHost,
   document-catalog policy, update work, broad platform dependencies, or an
   early `StartupReady`. Verify ownership order, startup-session transfer,
   ordinary workspace operations through the retained coordinator, and
   bootstrap shutdown integration. Independent macOS Debug verification
   passed the runtime/bootstrap/dependency CTests 3/3; the runtime tests link
   without Qt libraries. `ClassMngrNext` and the affected runtime test targets
   rebuilt. No full suite or other platform ran.
8. **F1508 — Complete:** Add a Next-only, lazy factory PageHost
   and connect `ApplicationShell` through it; registration must not construct
   pages. Activation creates once, switch-away suspends, returning reuses, and
   explicit release destroys the page and releases its page-scoped handles.
   Factories receive a page resource context; page code owns feature handles
   and clears them on suspension because `ResourceScope` labels requests but
   does not expire handles. Keep `PageManager` out of Next. Register the current
   initial page as a factory and preserve its visible shell behavior. A failed
   factory/activation must not retain a partial page or leak scoped handles.
   Verify lazy construction, lifecycle call order, reuse/release, feature vs
   page handle lifetimes, failure cleanup, and bootstrap initial-page display.
   Independent incremental macOS Debug verification passed PageHost QtTest
   4/4, bootstrap QtTest 30/30, and focused CTest 2/2. Source ownership
   validated 1,653 files. No full suite or other platform ran.
9. **F1509 — Complete:** Load the localized Next
   `DocumentCatalogProjection` from the `document.catalog-metadata` resource
   and retain it in `ApplicationRuntime` before shell/page readiness. Reuse
   `ResourceDocumentCatalogPort` through a narrow target that does not pull
   `ClassMngrNext::Platform` or Network. Catalog failure is a typed
   `InitialPageReady` failure and creates no shell; on success emit
   `InitialPageReady` and then `StartupReady`. Use the existing metadata-only
   test, plus an integrated bootstrap test whose package fixture omits every
   document body and checks that no QtPdf viewer is present. Do not construct
   a viewer, `QPdfDocument`, body session, or feature page during startup.
   Independent incremental macOS Debug verification passed bootstrap QtTest
   32/32 and six focused CTests 6/6. The generated boundary report excludes
   Platform, Network, QtPdf, and broad legacy dependencies. No full suite or
   other platform ran.
10. **F1510 — Complete:** Reuse `StartupUpdateCheckUseCase` and
    the existing startup-check port after successful `StartupReady`, scheduled
    on the first queued event-loop callback so the shell/page is already
    interactive. Do not construct `UpdateService`/network manager or dispatch
    a request during `ApplicationBootstrap::start()`. Preserve build-config,
    settings preference, API URL, and one-shot eligibility checks. Keep the
    Next dependency narrow with dedicated updater/startup-update targets, not
    the Platform umbrella or broad Core/Runtime. No resource-update service
    is called from startup; preserve manual update behavior. Verify zero
    service/port calls before the callback, one eligible dispatch afterward,
    disabled/unconfigured gates, and no resource-update call. Independent
    incremental macOS Debug verification passed the final focused bootstrap
    test, CTest 8/8, and the updater/controller loopback suites 2/2 with
    elevated local-socket permissions. No full suite or other platform ran.
11. **F1511 — Complete:** Complete shutdown coordination for settings, jobs,
    workspace, resource scopes, external automation, and development diagnostics.
    Acceptance: settings flush and cancellation of the active deferred updater
    happen before their dependencies are released, with settings flush
    observable to focused tests. A workspace-close failure keeps the runtime
    and session available for retry and prevents a close-triggered application
    quit. Successful shutdown releases page/resource ownership and any Next-
    owned automation while diagnostics and QApplication are alive, then quits;
    repeated shutdown performs final cleanup once.
    Independent incremental macOS Debug verification built `ClassMngr`,
    `ClassMngrNext`, `ClassMngrNextBootstrapTests`, and `ClassMngrUpdaterTests`.
    Focused bootstrap CTest passed 1/1, with 7/7 QtTest cases before the final
    page-pointer assertion; focused CTest passed again after that change.
    Updater CTest passed 1/1 after retrying a sandbox-blocked local loopback
    bind with escalated permission. Coverage includes settings-flush retry,
    close veto/retry, update cancellation/order, suppressed completion, direct
    page destruction, idempotence, and F1510 scheduling. No full suite or other
    platform ran. `git diff --check` passed.
    Limitation: `SettingsManager::sync()` has no status result; tests verify
    invocation/order, not the underlying QSettings write status. Retry is
    available while bootstrap remains alive; destroying the bootstrap ends
    ownership even if close still fails.
12. **F1512 — Complete; ready to commit:** Separate legacy startup/performance workflow
    automation from production startup and use a semantic prompt seam.
    Acceptance: `ApplicationBootstrap` and `StartupCoordinator` must not scan
    top-level widgets to discover or activate prompts. Workflow automation
    remains at the Qt presentation/test boundary, communicates semantic prompt
    request/choice values without returning widget pointers, and keeps any UI
    presenter inputs such as `QWidget* parent` out of Application/Domain
    contracts. Isolate direct prompt/form/dialog lookup and control automation
    across Sub Prep, Schedule Import, and Calendar Import behind Qt
    presentation/test adapters. Preserve Next's existing workspace argument
    handling, including recognized value-bearing performance options, and
    ordinary positional workspace selection. Existing `IUserPromptService`
    fake tests cover semantic request/choice; legacy subprocess tests cover
    workflow trace/capture through the Qt adapter. A single combined
    fake/workflow test is conditional on a v2 workflow migration, which is not
    part of this slice. Preserve current workflow behavior while isolating
    widget access; remove splash startup in F1513.
    Implementation: Sub Prep, Schedule Import source/review, and Calendar
    Import Preferences dialog automation now run through Qt presentation/test
    adapters with value-only results. Next's existing recognized
    value-bearing performance options and ordinary positional workspace
    selection remain supported. The Schedule Import workbook fixture now
    writes each worksheet row once across its teacher blocks, with five
    distinct teacher headers; assertions remain at 5 users and 96 candidates.
    Independent verification: build `ClassMngr`, `ClassMngrNext`,
    `ClassMngrNextBootstrapTests`, `ClassMngrNextApplicationRuntimeTests`,
    `ClassMngrDialogServicesTests`, and `ClassMngrStartupPerformanceTests` in
    the incremental macOS Debug tree; all six built successfully. Focused
    CTest passed 4/4 and `git diff --check` passed. Direct QtTest selectors
    passed without skips: representative workflow (3), semantic prompt fake
    and driver (5), startup workspace/CLI (4), Schedule Import cancel and
    apply (3 each; 5 users and 96 candidates), Sub Prep output (3), and
    Calendar Import normal and expected-failure (3 each). Calendar Import's
    local HTTP fixture required loopback access after its first sandboxed bind
    attempt failed. Source review confirms no widget-pointer returns from the
    adapters and no direct dialog discovery in the three scoped workflow
    handlers. This is targeted validation, not a full suite. The legacy splash
    checkpoint remains unchanged. Independent scope, implementation, and
    verification reviews are complete.
13. **F1513 — Discovered:** Remove the legacy splash class/image/progress,
    leases, minimum duration, resource paths, and startup pack initialization
    after the workflow compatibility path is isolated.

Initial Batch 1 discovery found no other slices beyond F1501–F1513.

## Batch 2 — discovered

No other slices were found.
