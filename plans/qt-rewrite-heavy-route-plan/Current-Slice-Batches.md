# Current Slice Batches

Phase 4 is In progress on the mandatory Heavy route.

## Batch 2 — active

The accepted queue stays focused on the new resource runtime and document
resource lifecycle. Theme/font/icon/campus/report UI consumers remain with
their Phase 5–7 startup, shared-UI, and feature migrations; do not change their
legacy oracle paths in Phase 4.

1. **F1469 — Resolve the installed resource root.** Accepted and committed as
   `d282e7f1` after independent macOS verification; depends on F1467/F1468 and
   does not search the current
   directory or source tree. CMake selects the package root relative to the
   executable directory; tests cover missing roots/catalogs, unrelated current
   directory contents, and relocation. Mixed absolute/relative Linux install
   directories with an install-time prefix override were not tested.
2. **F1470 — Add resource scopes.** Accepted and committed as `f7b2cb22`
   after independent macOS review.
   Every load request names a scope kind and owner, and each move-only result
   retains that scope with its owned value. The four kinds and RAII cleanup are
   covered by focused tests.
3. **F1471 — Add byte-budgeted resource caching.** Accepted and committed as
   `8ca1ddc5` after focused macOS verification. The injected per-instance
   cache uses deterministic LRU and accounted payload bytes; oversized
   text/JSON/image entries skip retention. Document bodies, streams, fonts,
   and translations bypass it. Cache hits still read and hash package bytes.
4. **F1472 — Add resource diagnostics.** Accepted and committed as `9ce7e35c`
   after focused macOS verification. The injected per-loader sink reports each
   logical load once, includes outcome and available cache/accounted-size
   metadata, and requires thread-safe sinks for concurrent calls. It adds no
   global singleton.
5. **F1473 — Load document catalog metadata through the new catalog.** Accepted
   and committed as `2084cb4f`; it remains unwired to Next startup and loads no
   document bodies.
6. **F1474 — Add loader-owned document content handles.** Accepted and
   committed as `34926f4b`. The independent Next port opens only the selected
   PDF body under ViewerSession scope and returns a move-only stream with
   explicit close and no pack fallback; it does not open the optional export
   reference. Legacy `NavigationController` remains on its existing adapter
   until consumer migration because the v2 Resources component is excluded
   from the default install.
7. **F1475 — Verify PDF session release at the new boundary.** Accepted and
   committed as `8a2163d2`. Its isolated QtPdf test verifies document close
   before stream close, explicit handle release, and reopen with a fresh
   ViewerSession generation. It does not wire the port into the production
   viewer. Qt Core/QtPdf cache versions differ; configuration, build, link,
   and focused test succeeded, but cross-platform compatibility remains
   unverified.
8. **F1476 — Scope Office/report inputs to operations.** Accepted and
   committed as `2bbbd23d`. The independent export port validates cataloged
   PPTX/DOCX references and opens only the selected body under Operation scope,
   returning a move-only stream with explicit close and RAII release. It has no
   pack fallback or startup preload and is not wired to legacy report/workspace
   UI; the Speaking Evaluation workspace remains unchanged. F1482 later
   classified three callerless roster workflow files and `dyb.png` as
   `excluded_v2`.
9. **F1477 — Define resource failure behavior.** Accepted and committed as
   `535dc382`. Unknown IDs report NotFound; corrupt selected PDF/PPTX streams
   fail without a handle or fallback and produce one CorruptResource diagnostic
   for the selected body. This test-only slice changes no production/legacy
   code; existing missing, type-mismatch, invalid-catalog, and no-fallback
   coverage remains.
10. **F1478 — Verify type-specific lazy loads.** Accepted as verification-only
    using F1468's existing loader tests; no separate source-code delta was
    needed. The selected examples exercise all seven loader APIs at request
    time; legacy feature consumers remain on pack paths.
11. **F1479 — Verify no-pack startup side effects.** Accepted and committed as
    `295d238d`. The actual `ClassMngrNext` subprocess exits normally with its
    exact version line, leaves isolated HOME/XDG roots uncreated, and leaves
    its fresh working directory without files or directories. The recursive
    scan includes hidden/system entries; Windows profile equivalents are
    defined in the test setup. Source and dependency review confirms no
    resource-port request, `ResourcePackManager`, settings/storage creation,
    external/legacy runtime
    RCC-pack mount, or splash path. F1467's two Calendar QML resources remain
    compiled into the executable with `qt_add_resources`; compiled-in
    resources are not external pack mounts.
    Filesystem behavior is process-tested; architecture claims are source/
    dependency-reviewed because there is no global loader-call counter.
12. **F1480 — Verify relocated package lookup.** Accepted and committed as
    `cc60849d`. A test-only macOS app-bundle probe launched from an unrelated
    working directory resolves the executable-relative root and loads an
    integrity-checked JSON resource. CWD and alternate-root decoys do not
    provide fallback when the selected root is absent. Production startup
    remains unwired to the resolver/loader; Windows/Linux layout coverage is
    still F1483.
13. **F1481 — Audit staged resource completeness.** Accepted as verification-only
    with no separate source-code delta. Fresh macOS staging contains 197
    physical files for 196 `resource_file` records plus the catalog; all 191
    direct SHA checks, five nonempty generated QMs, and two compiled Calendar
    QML keys passed audit. The QML keys are not staged files. Calendar key CTest
    passed 1/1; Windows/Linux remain F1483.
14. **F1482 — Classify callerless resource assets.** Accepted; four tracked
    assets (`dyb.png`, `Class Register.odt`, and the ODS/XLSX roster templates)
    retain exact source provenance as `excluded_v2` and remain in the legacy
    pack oracle. `roster-designs` remains a zero-source absence observation.
    Independent macOS Debug verification passed 32 utility tests, inventory
    `--check`, staged audit, and Calendar key CTest 1/1. The 226 asset files
    total 65,707,627 bytes; all tracked resources total 65,708,813 bytes.
15. **F1483 — Verify platform install roots (active).** Run the resource
    component layout gate on Windows, macOS, and Linux presets/stages; F1482
    verified macOS only.
16. **F1484 — Record Phase 4 removal boundaries.** Check no-pack behavior in
    Next and record legacy pack removal as Phase 12 cutover work.
17. **F1485 — Assemble the Phase 4 exit evidence.** Close only after the
    resource/package/lifetime gates pass; retain Phase 7/10 rendered-parity
    evidence for the migrated UI.

Start Batch 3 discovery when F1484 starts, the second-to-last Batch 2
candidate. If discovery finds possible candidates beyond the 20-slice limit,
record a reminder to search for them with Batch 3.
