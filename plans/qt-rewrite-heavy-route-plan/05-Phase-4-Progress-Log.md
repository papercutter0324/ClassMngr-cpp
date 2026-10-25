# Phase 4 Progress Log

## Batch 1 — F1464–F1468 — pushed 2026-10-25

Batch 1 was pushed from branch `Qt-Rewrite` to `origin` after F1468 passed its
independent loader review. The push advanced the remote from `58de51bf` to
`eb473fab`:

```text
git push origin Qt-Rewrite
58de51bf..eb473fab  Qt-Rewrite -> Qt-Rewrite
```

Commits in the pushed batch:

- F1464 — `473e8325b9f98069c8396291d057a398a9683ebe`
- F1465 — `3423e1de05f129d407004192565a79f06e572579`
- F1466 — `b10e60e854f21e67ed3b3653b8c8b608c7588345`
- F1467 — `153b85fb7b6314baf145ddd3f9584926111049d2`
- F1468 — `eb473fab89410e6dc5f5527d872b56f150f7f6fd`

Per-slice implementation and verification evidence is recorded in the
[Phase 4 plan](05-Phase-4-Resource-Loader-and-Packaging.md). Windows and Linux
resource staging remain for F1483; Batch 1 packaging was host-staged on macOS.

## F1474 — Loader-owned document content handles — accepted — 2026-10-25

`DocumentContentResourcePort` is an independent Next loader boundary. It opens
only the selected PDF body through `ResourceLoader` under `ViewerSession`
scope and returns a move-only stream with explicit close and no pack fallback.
The optional export reference is not opened at primary selection. The port is
not wired to legacy UI: `NavigationController` continues to use
`LegacyDocumentContentResourcePort` because the Phase 4 v2 Resources component
from F1467 is excluded from the default legacy install. Preserve that legacy
viewer path until consumer migration; QtPdf integration belongs to F1475.

The initial attempt to connect the new port to legacy navigation failed three
parity cases because the default install has no v2 resources. Keeping the port
independent resolved that boundary mismatch. Independent macOS Debug review
built the port, ResourceLoader, catalog, document-catalog, navigation-parity,
and PageManager targets incrementally; the port, loader, catalog, parity, and
PageManager targets were up to date, while the document-catalog target rebuilt
and linked. Focused CTest passed 4/4; four affected PageManager PDF lifecycle
slots passed individually, and the final port-test delta passed 1/1. Coverage
includes unsafe-reference rejection before load, selected PDF ID and
ViewerSession scope, one body request, explicit close, no pack fallback,
non-PDF/wrong-category/wrong-kind references, wrong catalog package path,
missing body, and parser rejection of `expected_type` inconsistent with the
`.pdf` extension. The adapter expected-type check was source-reviewed because
`ResourceCatalog` prevents loading a mismatched type/path record.

The full PageManager suite stalled in `registeredPagesAreCreatedOnFirstUse`;
the affected PDF cases passed individually. Windows/Linux were not run, and
`git diff --check` passed. F1474 is committed as `34926f4b`.
