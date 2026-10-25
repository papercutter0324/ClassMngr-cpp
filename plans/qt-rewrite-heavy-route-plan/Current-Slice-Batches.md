# Current Slice Batches

Phase 4 is In progress on the mandatory Heavy route.

## Batch 1 — active

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
   validate staged layouts.
5. **F1468 — Typed resource loader.** Add known-ID package-relative reads and
   stream handles with focused unit coverage; packaged builds must not search
   the source tree or current working directory.

F1464 is committed as `473e8325b9f98069c8396291d057a398a9683ebe`; F1465 is
accepted and awaits its slice commit. Start Batch 2 discovery when work on F1467
begins, the second-to-last candidate in this batch. Completed batch history
and push receipts belong in the Phase 4 progress log, not this tracker.
