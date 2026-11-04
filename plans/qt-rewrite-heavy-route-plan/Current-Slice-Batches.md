# Current Slice Batches

## Phase 8 — Active Batch 5 (3 candidates)

1. **8.7.6 Windows redistributable package validation — static acceptance
   passed; package CI pending:** use the exact architecture-specific signed VC
   redist package name; validate its version, signer, and staged hash without
   treating the x86 installer stub as the runtime payload architecture.
2. **8.7.7 Linux CMake link-group cycle — solution mapped; implementation
   pending:** preserve the Runtime/WorkspaceStore RESCAN group while removing
   reverse interface edges through shared Sub Prep and Calendar UI targets.
3. **8.7.8 macOS disk-image cleanup — solution mapped; implementation
   pending:** handle a busy read-only verification mount without losing package
   validation, checksum generation, or artifact upload.

## Phase 9 — Next Batch (20 candidates; gated on Phase 8 package CI)

1. **9.1 Measurement contract and artifact schema:** define named checkpoints,
   metrics, fixture/build provenance, and failure artifacts.
2. **9.2 Lifecycle instrumentation:** cover page, model, editor, and database
   result counts and ownership gaps found in the existing diagnostics.
3. **9.3 Resource instrumentation:** cover image/font/resource-cache ownership
   and PDF/export lifetime gaps found in the existing diagnostics.
4. **9.4 Packaged Windows Release runner:** validate installed x64 Release
   executable, architecture, Qt/CRT configuration, and deployed contents.
5. **9.5 Empty-workspace startup:** measure cold launch through first render.
6. **9.6 Representative-workspace startup:** measure startup-ready and first
   render with a fixed representative fixture.
7. **9.7 Idle and normal navigation:** capture five-minute idle and settled
   navigation separately from operation peaks.
8. **9.8 Repeated workspace/page lifecycle:** verify memory, handles, threads,
   and return after release.
9. **9.9 Theme and language rebuild:** verify settled memory across repeated
   palette and translation changes.
10. **9.10 Sub Prep 96-class lifecycle:** cover entry, summary, detail, refresh,
    output, leave, re-entry, widget/editor bounds, and memory return.
11. **9.11 Large schedule import:** measure parse, review, apply/cancel, and
    cleanup checkpoints.
12. **9.12 Large roster editing:** bound item/editor counts and measure release.
13. **9.13 Campus map lifecycle:** open, close, reopen, and measure decoded
    dimensions/bytes and release.
14. **9.14 Document catalog startup:** prove catalog metadata loads without PDF
    bodies or rendered pages.
15. **9.15 QtPdf and PDF preview lifecycle:** open, render, close, release,
    reopen, and measure preview cleanup.
16. **9.16 Calendar import and presentation:** measure the large import and
    retain the Phase 8 lazy-view lifecycle.
17. **9.17 My Classes, Classes, and Speaking navigation:** bound retained page,
    detail, and editor counts across repeated navigation.
18. **9.18 Staff Directory and AI batch review:** measure large review routes
    and bound table/editor counts.
19. **9.19 Speaking report generation:** measure operation peak, output release,
    and return to the pre-operation range.
20. **9.20 Class transfer export/import/cancel:** measure package/preview
    ownership, cancellation cleanup, peak, and return.

Phase 9 implementation and packaged measurements start only after the Phase 8
Windows, macOS, and Linux package workflows pass for the completed package
batch. The Phase 9 exit gate follows candidate 20.
