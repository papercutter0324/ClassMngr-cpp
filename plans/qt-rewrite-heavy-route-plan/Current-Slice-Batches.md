# Current Slice Batches

Phase 4 is In progress on the mandatory Heavy route.

## Batch 1 — accepted, awaiting push

1. **F1464 — Effective-resource inventory.** Refresh the checked-in baseline
   inventory with logical identities, packaged/source distinction, exact
   source bytes and hashes, installed/resident measurement methods, feature
   owner, loading tier/method, lifetime, streamability, and release policy.
   Keep document-catalog metadata separate from document
   bodies. Resolve `roster-designs` absence explicitly. F1464 is accepted;
   its inventory and verification evidence are in the Phase 4 plan.
2. **F1465 — Offline baseline baker.** Produce canonical installed files from
   the pinned repository baseline; validate paths and types and record content
   hashes and provenance. No installed overrides were present in the checked
   application-owned data directory. F1465 is accepted; its verification
   evidence is in the Phase 4 plan.
3. **F1466 — Generated resource catalog.** Generate the per-resource catalog
   from the canonical tree, including IDs, type/category, loading policy, byte
   size, and content hash.
4. **F1467 — Deterministic installed-tree packaging.** Install the canonical
   tree at the agreed Windows, macOS, and Linux package-relative locations and
   validate staged layouts. F1467 is committed as
   `153b85fb7b6314baf145ddd3f9584926111049d2`; macOS staging passed and
   Windows/Linux host staging remains for the cross-platform gate.
5. **F1468 — Typed resource loader.** Accepted after independent macOS review.
   The loader uses explicit catalog/root injection, validates catalog records,
   confines package-file access to the canonical root, and returns owning
   text, JSON, image, font, stylesheet, translation, and stream results. Its
   tests verify document-body laziness, hash integrity, and RAII release.
   The source-ownership gate and QtCore-only `ClassMngrNext` check pass.

F1464 is committed as `473e8325b9f98069c8396291d057a398a9683ebe`; F1465 is
committed as `3423e1de05f129d407004192565a79f06e572579`; F1466 is committed as
`b10e60e854f21e67ed3b3653b8c8b608c7588345`; F1467 is committed as
`153b85fb7b6314baf145ddd3f9584926111049d2`. Batch 2 discovery is complete and
its queue is below. Completed batch history
and push receipts belong in the Phase 4 progress log, not this tracker.

## Batch 2 — discovered, inactive

The accepted queue stays focused on the new resource runtime and document
resource lifecycle. Theme/font/icon/campus/report UI consumers remain with
their Phase 5–7 startup, shared-UI, and feature migrations; do not change their
legacy oracle paths in Phase 4.

1. **F1469 — Resolve the installed resource root.** Add platform-specific
   package-root resolution for Windows, macOS, and Linux; depend on F1467 and
   F1468 and never search the current directory or source tree.
2. **F1470 — Add resource scopes.** Represent startup, page, feature, and
   operation ownership explicitly in load requests and handles.
3. **F1471 — Add byte-budgeted resource caching.** Use explicit capacity and
   eviction; do not cache document bodies or unbounded decoded images.
4. **F1472 — Add resource diagnostics.** Report load time, decoded size, cache
   residency, and owner without adding a process-wide mutable singleton.
5. **F1473 — Load document catalog metadata through the new catalog.** Keep
   metadata startup-safe and document bodies unopened.
6. **F1474 — Add loader-owned document content handles.** Replace the
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
