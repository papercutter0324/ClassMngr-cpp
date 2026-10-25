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
6. **F1474 — Add loader-owned document content handles (active).** Replace the
   pack-backed Next document resource adapter used by legacy navigation with a
   loader-owned handle. Preserve selected-document request, file/device
   ownership, and explicit close in the new resource boundary; keep the legacy
   viewer path unchanged until feature migration.
7. **F1475 — Verify PDF session release at the new boundary.** Cover open,
   close, replacement/reopen, and owner release of a QtPdf document and handle
   in an isolated integration test.
8. **F1476 — Scope Office/report inputs to operations.** Preserve temporary
   workspace cleanup and do not preload PowerPoint bodies.
9. **F1477 — Define resource failure behavior.** Cover missing, corrupt, and
   type-mismatched files/catalog entries with no pack fallback.
10. **F1478 — Verify type-specific lazy loads.** Exercise text, JSON, image,
    font, stylesheet, translation, and stream paths without migrating legacy
    feature UI consumers.
11. **F1479 — Verify no-pack startup side effects.** Ensure the Next runtime
    makes no resource request, creates no pack storage/settings, mounts no RCC,
    and has no splash path.
12. **F1480 — Verify relocated package lookup.** Resolve only the selected
    installed root when launched from another working directory.
13. **F1481 — Audit staged resource completeness.** Compare package files and
    generated outputs with the catalog; validate Calendar Qt-resource keys.
14. **F1482 — Resolve unowned/absent inventory entries.** Decide the v2
    disposition of `dyb.png`, unused roster workflow files, and absent
    `roster-designs` without inventing feature owners.
15. **F1483 — Verify platform install roots.** Run the resource component
    layout gate on Windows, macOS, and Linux presets/stages.
16. **F1484 — Record Phase 4 removal boundaries.** Check no-pack behavior in
    Next and record legacy pack removal as Phase 12 cutover work.
17. **F1485 — Assemble the Phase 4 exit evidence.** Close only after the
    resource/package/lifetime gates pass; retain Phase 7/10 rendered-parity
    evidence for the migrated UI.

Start Batch 3 discovery when F1484 starts, the second-to-last Batch 2
candidate. If discovery finds possible candidates beyond the 20-slice limit,
record a reminder to search for them with Batch 3.
