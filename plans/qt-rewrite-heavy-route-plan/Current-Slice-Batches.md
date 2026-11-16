# Current Slice Batches

## Phase 8 — Adapter-boundary gate follow-ups (active)

The user asked to attempt closing the Phase 8 and 8a gates. The committed
macOS candidate at `d8f31c7aaf29bdc07f12f7f6f229bc8ea596889e` passed local
Debug, output-capture, and universal Release-package checks on 2026-11-14.
The local worktree has an in-progress ZIP writer-port implementation, so that
committed-head result remains historical evidence for the new files. The
hosted clean current-source baseline has since passed on docs-only commit
`6aa3715a`: Windows 618/618 and Linux 621/621, with required output and C53
checks. Windows Release package verification and ARM64 cross-build also passed.
Hosted results are linked in the Phase 8 and 8a progress logs; only final
cross-host/source review, remaining adapter-boundary review, and historical
macOS/Linux target-only dispositions remain open.

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
   UI has no `Q_OS_*` or `QFontDatabase` calls. Focused macOS tests pass, and
   hosted Windows/Linux Schedule Legacy/Next captures passed with identical
   same-host rasters. A separate UI capture review matched both Korean
   screenshots byte-for-byte with the retained baseline;
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
   hosted current-source Windows/Linux baselines also passed (see the cross-host
   record below).
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

Hosted clean current-source baseline workflow
[43908745977](https://github.com/papercutter0324/ClassMngr-cpp/actions/runs/43908745977)
ran at pushed docs-only commit `6aa3715a`, with production source inputs
matching local candidate `286468ea`. Windows passed 618/618 and Linux 621/621;
Schedule, all four Roster layouts, Speaking Standard/Advanced, and C53 passed
on both hosts. All 41 historical Windows and 20 historical Linux target names
passed, though their old target-only cases remain unknown. The local Windows
startup-case failure did not reproduce hosted; its cause is unknown and no fix
is claimed. Windows Release package verification and ARM64 cross-build passed
in workflow [43908562397](https://github.com/papercutter0324/ClassMngr-cpp/actions/runs/43908562397).
Current-source Linux Release workflow
[43939557328](https://github.com/papercutter0324/ClassMngr-cpp/actions/runs/43939557328)
also passed and was independently accepted. Its result and checksum are in the
Phase 8 progress log. Windows/Linux at `6aa3715a` and macOS at `25190b33` share
source/package inputs because intervening changes were documentation-only;
the Release matrix is covered. Phase 8/8a remain open for final cross-host/source
and adapter-boundary review and historical macOS/Linux target-only dispositions.
See the [Phase 8 hosted record](09-Phase-8-Progress-Log.md#2026-11-17--hosted-current-source-baseline-and-windows-release-verification),
[Linux Release record](09-Phase-8-Progress-Log.md#2026-11-17--current-source-linux-release-verification-and-package-matrix),
and [Phase 8a hosted record](09.5-Phase-8a-Progress-Log.md#2026-11-17--hosted-windows-and-linux-current-source-gate-results).

## 2026-11-14 gate attempt — candidate f6b258f

This historical gate attempt is separate from the current hosted-baseline
status above. Its final clean candidate was
`f6b258f1bbb010ec0b7847f81827fd0409832888` (tree
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
