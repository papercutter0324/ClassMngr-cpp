# Current Slice Batches

## Phase 8 — Adapter-boundary gate follow-ups (active)

The user asked to attempt closing the Phase 8 and 8a gates. The committed
macOS candidate at `d8f31c7aaf29bdc07f12f7f6f229bc8ea596889e` passed local
Debug, output-capture, and universal Release-package checks on 2026-11-14.
The local worktree now has an in-progress ZIP writer-port implementation, so
the committed-head result remains historical evidence and does not verify the
new files. Direct Actions dispatch is unavailable, but the connected GitHub
account can create a dedicated branch and draft PR; use the PR triggers for
current-source Linux and Windows workflow evidence after the source batch is
reviewed. No current-source hosted run has started yet.

Ordered source-boundary candidates:

1. Speaking-report ZIP archive write port — implemented and independently
   accepted. The focused macOS build passed; the report suite passed 34 cases,
   Next batch tests 9 cases, and the dependency-boundary test 1 case. An
   independent review reran the three CTest registrations (3/3) and accepted
   the port, ordering, failure, cleanup, and composition paths. The change is
   uncommitted and remains in the working tree.
2. Document Viewer Save Copy — implemented and independently accepted. The
   page now uses `DefaultOutputDirectoriesPort` for Documents/Home; independent
   CTest passed 1/1 (18 QtTest cases), and source review found no direct Qt path
   query in the feature page.
3. Roster print preview — implemented and accepted by independent review. The
   Qt adapter owns the temporary PDF document and directory, load checks,
   rendering, and release. The feature dialog contains no QtPdf document/view
   API. Focused CTest passed 1/1; QtTest reported 19/0/0. The independent
   fresh rebuild stopped at an unrelated unowned generated-Resources folder;
   it was left untouched. A forced render-failure injection remains uncovered.
4. Schedule output font defaults — source boundary implemented and accepted.
   The font port supplies the host family and English fallback size; Schedule
   UI has no `Q_OS_*` or `QFontDatabase` calls. Focused macOS tests pass.
   Windows/Linux branches remain unverified. A separate UI capture review
   matched both Korean screenshots byte-for-byte with the retained baseline;
   the differing `pageHeightBeforeCrop` value is capture geometry, not output.
5. Speaking-report PowerPoint UI — implemented; independent review is
   accepted. An injected platform capability now maps macOS to
   renderer+notice, Windows to renderer only, and Linux to neither. Office
   installation remains an operation-time check. Independent tests passed 41
   Speaking, 14 Navigation, and 90 Bootstrap cases.

## Phase 8 — Next boundary batch (active)

Discovery started with candidate 4 in the previous batch and found three
additional candidate groups:

1. Route macOS-specific typography sizing in Teacher Profile and the font-size
   presenter through host presentation policy — implemented and independently
   accepted. Focused native macOS verification passed 7/7 CTest registrations;
   native Windows/Linux branches remain for the hosted run.
2. Move direct feature font registration for typed signatures and Speaking
   reports behind a scoped registration port and existing resource ownership —
   implementation active. Preserve Legacy composition and page/report-session
   lifetimes; Inter/Pretendard remain under startup ownership.
3. Calendar XLSX ZIP reading is a confirmed formal ZIP/package boundary gap:
   the shared workbook reader owns central-directory parsing and zlib inflate.
   A streaming `ArchiveReadPort`/ZIP adapter implementation is active; retain
   the existing seek/replay requirements and bounded worksheet streaming.
4. PDF generation is a confirmed formal gap under the Phase 8 deliverables.
   Schedule, Roster, Sub Prep, and Speaking shared renderers still construct
   `QPdfWriter`; an injectable writer session should hide `QPdfWriter` while
   retaining the existing `QPainter` drawing and output settings. The bounded
   composition design is in progress.

Begin the next candidate discovery now. Search for remaining v2 boundaries
beyond typography, font registration, and Calendar ZIP reading; add only
verified formal blockers.

## Phase 8 — Next boundary batch discovery (in progress)

Discovery continues while the font, Calendar ZIP-reader, and PDF-generation
candidates are addressed; check the remaining Phase 8 integration interfaces
before closing the source-boundary batch.

The Calendar workbook network factory is currently treated as a test seam for
a Qt cross-platform service, not a platform-API leak. The existing
`ProcessMemorySnapshotProvider` satisfies the platform memory-sampling
interface requirement; do not duplicate it.

The Document Viewer still exposes some QtPdf types through its session port,
and the page uses `QPdfView` for presentation controls. Independent review
found no confirmed gate blocker: the session owns, loads, closes, and releases
the active document, while the plan permits retaining Qt PDF. Reopen only if a
toolkit-neutral viewer interface becomes an explicit requirement.

## Phase 8a — Cross-host verification open

The 2026-11-14 macOS run at `d8f31c7aaf29bdc07f12f7f6f229bc8ea596889e`
passed 5,607 Debug build steps, 620/620 CTest targets, Schedule/Roster/
Speaking/C53 output evidence, and the universal Release package verifier. An
independent C53 review confirmed all 34 decoded-pixel hashes, 17 paired
rasters, and PDF/database file hashes. Later ZIP changes make `d8f31c7a`
historical evidence for the current dirty worktree; affected checks must be
rerun after the adapter batch.

No Linux or Windows workflow was dispatched because the available GitHub
connector cannot dispatch, the browser session is unavailable, `gh` is not
installed, and local Git has no credential. Current-source Linux/Windows
Debug and Release workflow results remain required before the cross-platform
gate can close.

## 2026-11-14 gate attempt — candidate f6b258f

This later attempt supersedes the dispatch diagnosis above. The final clean
candidate is `f6b258f1bbb010ec0b7847f81827fd0409832888` (tree
`65239452d50f917b5817e8e34682e2da731224d5`) at
`/private/tmp/classmngr-phase8-8a-candidate-20261114`; its fixes are present
byte-for-byte in the shared worktree. The prior unfiltered macOS run was
613/620. Four failures (#243, #244, #259, #330) now pass focused reruns, but
the final seven-test rerun is 4/7: #326, #339, and #620 still SEGFAULT. C53
passes 17 pages with zero changed pixels. The final universal Release DMG
passes package verification and integrity checks. Exact focused CTest and C53
evidence is recorded in the [Phase 8a log](09.5-Phase-8a-Progress-Log.md).

Linux and Windows were not run. GitHub auto-review rejected `github_create_blob`
because public source upload was unauthorized; no branch, PR, workflow, or
remote ref changed. Do not retry. Explicit authorization is required to upload
candidate `f6b258f` to public `papercutter0324/ClassMngr-cpp` and open a draft
PR against `Qt-Rewrite`. Phase 8 and 8a remain in progress.
