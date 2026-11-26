# Phase 10 — Progress Log

Read the [Phase 10 plan](11-Phase-10-Visual-Behavioral-and-Cross-Platform-Parity.md)
for current status, requirements, and contracts. This file is the historical
progress record for Phase 10; do not put dated progress updates in the plan.

### 2026-11-25 — Phase 10 start precheck passed

At the user's request, an independent local Tester reviewed the fresh
Windows x64 Release precheck at
artifacts/phase9/p9-19-exact-release-builds/p10-release-precheck-20261125-115431-f8d41043.
The receipt SHA-256 is
977cd992aeff4f85fb4226275fa61358823d2f449cd1d5873f314cf7a1254192.
Its 2,377-file source inventory SHA-256 is
4f792604d5d0cfdcf7581cf1e875291a60c810b232c7d2d083243aeaddb83f25, and
the current source matched. The verified toolchain was VS2022 x64/v143
14.44.35207 with Windows SDK 10.0.26100.0. Four focused CTests passed:
ClassMngrNextBootstrapTests, ClassMngrNextPageHostTests,
ClassMngrNextClassesNavigatorPageTests, and
ClassMngrNextClassesNavigatorVisualCapture. The executable SHA-256 is
669ee067ff9efe2995cd5f81f8071e169323b1da931742cb3ccda8e5ad7f257d; the
CTest log SHA-256 is
6a1110f231c803532b466a31de8c21360cd678e2aa605098ab53f063e95e4347.

This passing focused precheck authorizes Phase 10 to start. It is not Phase 9
final packaged Release evidence and does not complete Phase 9 or satisfy its
full Windows x64 Debug CTest. Those remaining Phase 9 items are deferred to
Phase 14 after Phase 13. Phase 10 parity work and exit requirements remain
unchanged. The repository-root build directory remains empty by user request.

The active Batch 1 discovery is in the [current slice-batch tracker](Current-Slice-Batches.md).
Its visual candidates are discovery targets, not verified current-source
defects; requalify with source-bound captures and record source, toolchain,
fixture, and raw artifacts.

### 2026-11-25 — Phase 10 paused for Linux switch

The user paused this session to switch to Linux. Phase 10 remains In Progress.
Batch 1 candidate 1, focused Linux verification of migrated Phase 7 behavior,
has not run. Resume on Linux with the standard `linux-gcc-debug` preset and
the 19 selected CTest executables listed in the canonical
[session handoff](../../agent_docs/latest_session_work.md). Do not rerun the
full Phase 7 suite. The current Windows environment had no local Linux runtime;
the temporary hosted-workflow fallback was removed, and no GitHub workflow
was triggered.

The focused Windows x64 Release precheck passed and authorizes Phase 10 start
only; it does not satisfy Phase 9's final Release gate. The Phase 9 transition
commit `95c2d4abf3ab03ff69e6871a7b10b6a93c0efd87` is pushed to
`origin/Qt-Rewrite`. Phase 9 remains In Progress and incomplete with all
remaining work deferred to Phase 14 after Phase 13 under the unchanged gates.

### 2026-11-25 — Linux continuation paused after Candidate 14

The user directed work to stop after the current candidate and asked for
candidate completion states. The [Batch 1 tracker](Current-Slice-Batches.md)
now marks Candidates 1–14 as having bounded Linux evidence, with Candidate 4
disposition still open and Candidate 11 partial; Candidates 15–20 are not
started. Phase 10 remains In Progress and its cross-platform exit gate remains
open. Candidate 15 is the next entry point on resume. No commit, push, or
remote workflow occurred.

- **Candidate 12 — My Classes:** Fresh source-bound Linux captures used the
  production fonts in four locale/theme pairs. Next grade tabs no longer elide
  Korean `E4`; the empty status label is hidden while empty and error messages
  remain visible; the combined header/content gap matches Legacy at 24 px. The
  selected tab begins at y=129 in all pairs. Focused page/capture checks passed
  and the independent review verified the captures. Existing typography,
  selector-density, schedule-text, and Korean room-label differences remain
  accepted; empty-state captures, forced tab overflow, and other hosts remain
  open. See the [capture report](../../artifacts/phase10/p10-b1-c12-linux-my-classes-20261125T104422Z-header-gap-fb5828ab/manifest.json)
  and [independent review](../../artifacts/phase10/p10-b1-c12-header-gap-final-independent-review-20261125T104803Z-fb5828ab/report.md).
- **Candidate 13 — Schedule display:** Fresh Linux captures and independent
  review covered the toolbar, all display modes, overflow, and Intensive text.
  Next now sizes the plain Testing assignment row from translated wrapped text
  and available width, and uses the catalog's translated Korean room prefix.
  Focused page/capture CTests passed 2/2. Extreme 256-character Regular and
  Testing labels clip in both routes; Next exposes their full text through
  display, accessibility, and tooltip data, and the compact-row behavior is
  accepted. Other hosts remain unreviewed. See the [final capture report](../../artifacts/phase10/p10-b1-c13-korean-room-prefix-20261125T121321Z-fb5828ab/report.md),
  [independent review](../../artifacts/phase10/p10-b1-c13-korean-room-prefix-independent-review-20261125T123000Z-fb5828ab/report.md),
  and [long-label review](../../artifacts/phase10/p10-b1-c13-wrap-stress-independent-review-20261125T124500Z-fb5828ab/report.md).
- **Candidate 14 — Schedule Import preview:** Fresh Linux Legacy/Next captures
  verified the compact Next preview still clips Meeting times at the initial
  horizontal position; full strings are now exposed through tooltip and
  accessibility roles. The deterministic first typed overlap now appears in
  localized status with both classes and time ranges; Apply remains disabled.
  The five locale catalogs and their Phase 4 inventory records were updated.
  App-state validation, Legacy import dialog, and Next integration CTests each
  passed 1/1, and an independent review verified the captures and inventory
  delta. Windows/macOS and live screen-reader/tooltip interaction remain open.
  See the [implementation report](../../artifacts/phase10/p10-b1-c14-preview-access-conflict-20261125T133418Z-fb5828ab/report.md)
  and [independent review](../../artifacts/phase10/p10-b1-c14-preview-access-conflict-independent-review-20261125T135300Z-fb5828ab/report.md).

Candidates 5–10 also have focused Linux evidence, with their platform-specific
follow-ups still open; Candidate 11 remains partial because a full installed
Next runtime was not staged. Candidate 4's page-break/product disposition was
unresolved at that point; its later user decision and Windows x64 evidence are
recorded below. At the time of that pause, Candidate 15 was the resume point;
Candidate 19 still triggers next-batch discovery.

### 2026-11-26 — Windows x64 follow-up through Candidate 5

Phase 10 remains In Progress. Candidates 1–5 now have Windows x64 evidence;
resume with Candidate 6 Windows x64. Candidates 6–10 and 12–14 have no
Windows x64 follow-up yet; Candidate 11 remains partial Linux evidence.

- **Candidate 1:** The exact selected 19 migrated-feature CTests passed 19/19
  in Windows x64 Debug (265.79s). Toolchain: VS18 2026, MSVC 19.52.37029,
  SDK 10.0.28000, Qt 6.12.0. A one-line `ClassMngrAboutPresentation` test link dependency was added at
  `cmake/tests/next.cmake:782`; no C2/C3 product implementation changed. See
  [CTest log](../../artifacts/phase10/p10-win-c1-c3-filetracker-20261125T153339Z-8dadae2b/ctest-c1-exact19-followup5.log)
  and [commands](../../artifacts/phase10/p10-win-c1-c3-filetracker-20261125T153339Z-8dadae2b/p10-win-c1-c3-commands-followup5.txt).
- **Candidates 2–3:** Windows x64 offscreen `ClassMngrNextNavigationTests`
  and `ClassMngrNextSpeakingEvaluationPageTests` passed 1/1 each and were
  independently accepted for tested behavior. Native interactive Windows
  focus, clipboard, and window behavior remain open. See the
  [Candidate 2 log](../../artifacts/phase10/p10-win-c1-c3-filetracker-20261125T153339Z-8dadae2b/ctest-c2-navigation-followup5.log)
  and [Candidate 3 log](../../artifacts/phase10/p10-win-c1-c3-filetracker-20261125T153339Z-8dadae2b/ctest-c3-speaking-redo-followup5.log).
- **Candidate 4:** The user selected the historical grouping as the product
  contract: Teachers 22–24 stay together on page 11 and Sub Notes starts on
  page 12. The shared pagination renderer and focused test were updated;
  `ClassMngrSubPrepPrintPdfTests` passed 1/1, and independent review accepted
  Windows x64. Refreshed C53 has 12 Sub Prep and 5 roster pages; all 17
  Legacy/Next rasters match at 150 DPI, the SQL oracle reports 750 students,
  30 class headers, and 0 bullets, and extracted Legacy/Next text matches
  (11,119 chars) with 30 class notes and 24 teacher notes. The older historical
  reference is `sourceBound=false`. The strict Next Sub Prep PDF diagnostic
  found 903 U+0009 TAB mappings and zero unmapped; that diagnostic is not a
  pass. The formal strict C53 audit passes for the Next roster PDF. Windows
  ARM64 and macOS remain open. See the [independent report](../../artifacts/phase10/p10-win-13-subprep-c53-repair-20261126T023203/report.md)
  and [test log](../../artifacts/phase10/p10-win-13-subprep-c53-repair-20261126T023203/subprep-test-final.log).
- **Candidate 5:** Schedule, Roster, and Speaking targets passed 7/7 with
  page, text, geometry, and raster checks; independent review accepted this
  Windows x64 matrix. Candidate 4's refreshed C53 is the Sub Prep row. Windows
  ARM64/macOS remain open. Cross-host comparison values and an LNK4075 warning
  are documented; two capture environment assignments were not fully
  recorded. See the [matrix report](../../artifacts/phase10/p10-win-c5-output-parity-20261125T170351Z/report.md).

For the Windows run, resource input byte drift was corrected only for guarded
HEAD/inventory-identical CRLF variants; all 238 Phase 4 inventory records pass.
A process-only TEMP/TMP override avoided MSBuild FileTracker denial; the Phase
4 verification evidence is [here](../../artifacts/phase10/p10-win-c1-c3-filetracker-20261125T153339Z-8dadae2b/phase4-verify-238-followup5.json).

### 2026-11-26 — Windows x64 follow-up through Candidate 18; Candidate 19 started

Phase 10 remains In Progress. Candidates 6–10 and 12–18 now have bounded
Windows x64 follow-up evidence; Candidate 11 remains partial because a full
installed Next runtime was not staged. These Windows captures and tests do not
close native interactive, Windows ARM64, or macOS requirements unless stated.

- **Candidate 6 — Native printing:** Independent review accepted bounded
  Windows x64 headless viewer and print-route evidence: two capture CTests and
  focused injected print slots passed. No native dialog, printer, print job,
  or physical output was exercised. See the [report](../../artifacts/phase10/p10-win-36-c6-headless-print-20261126/report.md).
- **Candidates 7–10 — PowerPoint, updater, file workflows, external launches:**
  Focused Windows x64 service/fallback tests passed 2/2 for Candidate 7, updater
  tests passed 2/2 for Candidate 8, and file-workflow tests passed 10/10 for
  Candidate 9. Candidate 10 verified injected URL/folder routes and a fake
  local-file opener; registered OS handlers, Office COM/PowerPoint rendering,
  native installer/folder UI, native pickers, and real browser/Explorer launches
  remain open. Evidence: [C7](../../artifacts/phase10/p10-win-c7-windows-x64-debug/report.md),
  [C8](../../artifacts/phase10/p10-win-22-updater-windows-x64-debug/report.md),
  [C9](../../artifacts/phase10/p10-win-25-file-workflow-followup/report.md),
  [C10](../../artifacts/phase10/p10-win-27-external-launch-windows-x64-debug/report.md)
  and its [local-file follow-up](../../artifacts/phase10/p10-win-27-external-launch-windows-x64-debug/follow-up.md).
- **Candidate 12 — My Classes:** Windows x64 populated, empty, and summary-read
  error capture CTests passed 3/3; the accepted populated captures stayed
  byte-identical to the prior Windows run. Forced grade-tab overflow remains
  open because unsupported grade labels collapse to “Other.” See the
  [state follow-up](../../artifacts/phase10/p10-win-29-my-classes-state-followup-20261126/report.md)
  and [populated capture report](../../artifacts/phase10/p10-win-29-my-classes-visual-20261126/report.md).
- **Candidate 13 — Schedule display:** A test-only precision correction to
  `plainTextEvidence` preserved all fit and containment assertions. The focused
  page and capture CTests passed 2/2; 38 Legacy/Next records were captured.
  Testing rows grow in Next to fit wrapped room text, while long Intensive text
  remains available to accessibility/tooltip roles. Pixel parity is not
  claimed. Independent review found the manifest's same-data field is
  hard-coded and a screenshot-order concern inconclusive; current source order
  did not establish a product defect. See the [report](../../artifacts/phase10/p10-win-13-schedule-harness-fix-20261126T065642/report.md).
- **Candidate 14 — Schedule Import preview:** Focused model/dialog/integration
  checks and both Legacy/Next capture CTests passed. The artifact set has 72
  PNGs across four locale/theme pairs; live tooltip and screen-reader behavior
  remain open. See the [report](../../artifacts/phase10/p10-b1-c14-preview-access-conflict-windows-x64-20261126T065044/report.md).
- **Candidate 15 — Teacher Profile:** The capture CTest and focused birthday
  clear/save/reopen slot passed; four page images were readable. Native popup,
  pointer, keyboard/IME, and accessibility behavior remain open. See the
  [report](../../artifacts/phase10/p10-win-15-teacher-profile-followup-20261126/report.md).
- **Candidate 16 — My Details and Signature:** The four-locale/theme capture
  CTest and three focused signature slots passed; all 16 images were reviewed
  and readable. Native picker, keyboard/IME, and screen-reader behavior remain
  open. See the [report](../../artifacts/phase10/p10-win-16-my-details-signature-20261126/report.md).
- **Candidate 17 — Staff Directory:** Independent review accepted the Windows
  x64 offscreen evidence: the unit CTest passed 1/1, four registered capture
  CTests passed, and four direct cases exited 0. All 16 PNGs were readable and
  all 67 hashes verified. Eight page-relative manifests measure Legacy table
  bottom Y=670 and Next Y=676 (+6 px), matching the historical direction; no
  pixel-parity threshold was applied. Native DPI/window behavior, ARM64, and
  macOS remain open. See the [review summary](../../artifacts/phase10/p10-win-37-staff-directory-20261126/review-summary.json)
  and [hash inventory](../../artifacts/phase10/p10-win-37-staff-directory-20261126/SHA256SUMS.json).
- **Candidate 18 — Upcoming Birthdays:** Independent review accepted the final
  focused CTest run (7/7), 16 readable populated/empty captures, four manifests,
  and 20 matching hashes. Legacy and Next PNGs are byte-identical per pair;
  Inter/Pretendard Variable resolution and Hangul support are recorded. Native
  display behavior, ARM64, and macOS remain open. See the [report](../../artifacts/phase10/p10-win-18-upcoming-birthdays-20261126T0852KST/report.md).

Candidate 19's next-batch discovery is complete, and its Windows x64 visual
follow-up is now in progress. The current harness
(`tests/next/my_workspace_visual_capture.cpp`) covers the initial embedded
Calendar view in all four locale/theme pairs but does not scroll to the lower
Upcoming section. Requalification will capture upper and lower Legacy/Next
views and record actual shell labels, including the reported Korean difference.
The focused CTests are `ClassMngrNextMyWorkspaceVisualCapture`,
`ClassMngrNextMyWorkspacePageTests`, and `ClassMngrNextCalendarPageTests`; no
Candidate 19 result is available yet.
Candidate 20's source mapping has also begun as read-only exploration; no
Windows Candidate 20 implementation or verification has started.

The beyond-20 scan found two further discovery targets, not confirmed defects:
the historical Initial Setup Wizard package has 14 Legacy/Next stage pairs but
omits English/dark and Korean/light ([README](../../docs/qt-rewrite/visual-baseline/next/phase-7/initial-setup-wizard/README.md));
the historical Document Catalog package contains Next states without a Legacy
pair ([README](../../docs/qt-rewrite/visual-baseline/next/phase-7/document-catalog-c42/README.md)).
Document Viewer and Campus Directory already have historical four-pair
Legacy/Next packages. Requalify all historical findings against current source.

### 2026-11-26 — Windows x64 follow-up through Candidate 21; Candidate 22 started

Phase 10 remains In Progress. Candidates 19–21 now have bounded, independently
accepted Windows x64 evidence. Candidate 22 is the next entry point. These
offscreen runs do not close Windows ARM64, macOS, or native-interaction gates.

- **Candidate 19 — My Workspace Calendar:** The focused page and Calendar page
  CTests passed in an earlier valid run; the capture CTest then failed because
  the Legacy scrollbar range changed during layout. After bounded settling and
  an 18 px heading alignment, the focused capture CTest passed 1/1. Independent
  review accepted all 21 PNGs and 134 hashes. The lower Upcoming heading and an
  event row are visible in all eight Legacy/Next locale/theme captures. Korean
  Legacy shell labels remain English while Next uses Korean labels; actual
  strings are recorded without a defect conclusion. See the
  [report](../../artifacts/phase10/p10-win-c19-calendar-20261126/report.md).
- **Candidate 20 — Classes states/localization:** Independent review accepted
  all 24 PNGs, 12 manifest entries across four locale/theme pairs, 37 hashes,
  and both focused CTests. The initial build failed in MSBuild FileTracker
  with E_ACCESSDENIED when TEMP/TMP pointed to the user temp directory; the
  artifact-local retry built both targets and passed both tests. The Next
  read-error diagnostic continues below the 800×600 capture edge while the
  localized status and Retry control remain visible. See the
  [review notes](../../artifacts/phase10/p10-win-c20-classes-20261126/review-notes.md).
- **Candidate 21 — Initial Setup Wizard locale/theme coverage:** Independent
  review accepted 60 final PNGs (28 Legacy/Next stage pairs and four extra Next
  personal-details captures), eight font records, 325 hashes, and five focused
  capture CTests. Review found the first Legacy captures had missing glyphs
  because the test bypassed production FontManager initialization. The
  capture-only harness now links, loads, and applies bundled Inter/Pretendard
  fonts per locale, asserts a resolved family and U+AC00 support, and restores
  the QApplication font. The final Korean text renders; pre-fix tofu images
  remain preserved as diagnostics, not accepted captures. See the
  [visual review](../../artifacts/phase10/p10-win-c21-initial-setup-wizard-20261126/visual-review.md).
- **Candidate 22 — Document Catalog paired visual review:** Current-source
  Windows follow-up is underway. The Legacy `Sidebar` accepts the same typed
  `DocumentCatalogProjection` used by Next; a sidebar-region pair is feasible.
  The historical package contains Next shell captures without a Legacy pair.
  Full-window Legacy parity is outside this mapped scope, and no result or
  product-defect conclusion is available yet. See the
  [historical package](../../docs/qt-rewrite/visual-baseline/next/phase-7/document-catalog-c42/README.md).

### 2026-11-26 — Candidate 22 accepted; supplemental Windows follow-ups closed

Candidate 22's current-source Windows x64 sidebar-region review was independently
accepted. Two focused CTests passed. The artifact set contains 12 PNGs and 24
verified hashes: four Next full-shell captures plus eight matched Legacy/Next
sidebar-tree images, 254×630, across en_US/ko_KR and light/dark. The manifest
records 7 folders and 30 documents, the same selected stable ID
`document_guides_lesson_planning`, and no loaded PDF body; the capture target
records zero `QPdfDocument` instances and zero document-body diagnostics.
Korean labels and the selected row are visible in both trees. At the fixed
254-pixel Next tree width, some long sibling labels elide, while the selected
label remains readable. This is a sidebar-region comparison and makes no
full-window Legacy parity claim. See the
[review notes](../../artifacts/phase10/p10-win-c22-document-catalog-20261126/review-notes.md).

With Candidate 22 accepted, the supplemental Windows x64 follow-ups for
Candidates 19–22 are closed. Phase 10 remains In Progress: Candidate 11 is
partial Linux evidence, and Windows ARM64, macOS, native-interaction, and
cross-host requirements remain open.

### 2026-11-26 — Candidate 11 Windows x64 installed-runtime follow-up started

Candidate 11's existing Linux evidence remains partial; its Windows x64
installed-runtime follow-up has started, and no runtime result is available
yet. The bounded plan uses a separate Release build tree, an artifact-local
install prefix, explicit components, and isolated settings. It excludes
Program Files installs and the installer target. Candidates 19–22 remain
independently accepted. Phase 10 stays In Progress with Windows ARM64, macOS,
native-interaction, and cross-host requirements open.

### 2026-11-26 — Candidate 11 Windows x64 limited pass; ARM64 cross-build started

Independent review accepted the bounded Windows x64 Release staging result.
The package manifest covered 406 paths; 338 package hashes and 192 direct
resource hashes were verified, and the catalog matched its fixture.
`ClassMngrNextResourceRootResolverTests` and
`ClassMngrNextPlatformSettingsManagerWindowGeometryPortTests` passed, and the
layout probe resolved the staged `resources` directory.
An isolated 15-second offscreen startup recorded a flushed
`application.exec.enter` event before the harness stopped that exact PID. The
368-entry artifact inventory is at
[report](../../artifacts/phase10/p10-win-c11-installed-runtime-20261126/report.md).

This is not full installed-runtime acceptance: the geometry CTest covers only
the settings adapter/key round trip; installed-app geometry across relaunches
and visual font/translation rendering were not tested. No native GUI, installer,
Program Files install, ARM64 runtime, or macOS was exercised. Candidate 11
remains partial overall because Linux evidence is partial. Its separate Windows
ARM64 Debug cross-build follow-up is now in progress; no build result is
available yet. Phase 10 remains In Progress with cross-host and native gates
open.

### 2026-11-26 — Candidate 11 ARM64 cross-build accepted; supplemental checks started

Independent review accepted Candidate 11's Windows ARM64 Debug cross-build as
cross-build-only. The standard `build/windows-arm64-debug` tree configured and
built `ClassMngrNext`, `ClassMngrNextResourceRootResolverTests`, and
`ClassMngrNextPlatformSettingsManagerWindowGeometryPortTests`. The x64 `dumpbin`
verifier confirmed all three executable headers as AA64/ARM64, and all 10
artifact checksums matched. No ARM64 CTests, application launch, or install was
performed. The qmake metadata probe was rejected by Windows before execution;
this was a host/tool execution rejection, not a source or test failure. See the
[cross-build report](../../artifacts/phase10/p10-win-arm64-crossbuild-20261126/report.md).

Candidate 9's supplemental Windows-QPA presenter cancellation test is now in
progress, with no result. Candidate 6's safe print-dialog feasibility map is
also in progress, with no result. The user confirmed that no native ARM64 or
macOS host is currently available. Candidate 11 remains partial overall, and
Phase 10's runtime, native-interaction, macOS, and cross-host gates remain open.

### 2026-11-26 — Candidate 9 Windows-QPA cancellation and Candidate 6 print feasibility

The selected existing Candidate 9 presenter cancellation slot passed under
`QT_QPA_PLATFORM=windows`: PID 6632 exited 0 in 307 ms, and the test asserted
cancellation with no selected path and no preference write; all 46 artifact
checksum entries were verified. Its focused build
attempt failed in ZERO_CHECK with MSB4018/FileTracker access denied before
compilation, so the pre-existing x64 Debug executable was used. This verifies
in-process Escape cancellation for the Qt widget dialog only; it does not cover
UI Automation, navigation, visual parity, or the Windows common-dialog backend.
See the [supplemental report](../../artifacts/phase10/p10-win-qt-dialog-qpa-20261126/report.md).

Read-only Candidate 6 mapping found that the existing cancellation slot returns
`QDialog::Rejected` directly and never opens `QPrintDialog`. The product
Windows “Use Windows Print Dialog” route enumerates/uses the system default
printer before showing `QPrintDialog`. No dialog or printer was touched. Actual
print dialogs, printers, jobs, and physical output remain unverified.

Candidate 11 remains partial overall: its Windows x64 installed-runtime result
is limited and its ARM64 result is cross-build-only. No native ARM64/macOS host
is currently available; runtime, native-interaction, macOS, and cross-host
Phase 10 gates remain open. Continue with remaining Windows-native interaction
and cross-host requirements.

### 2026-11-26 — Windows native follow-up handoff

Candidate 2's isolated `shellExitActionUsesCloseApprovalAndGeometryPersistence`
slot built and passed under `QT_QPA_PLATFORM=windows`. Independent review
confirmed `qwindowsd.dll`, the exact PID and slot, 12 matching compilation-unit
hashes, and the artifact inventory. The window sample came from a different
process, so it is not shell-HWND or UIA evidence; the clipboard slot was not
run. This is limited platform-backend evidence. See the
[report](../../artifacts/phase10/p10-win-c2-shell-exit-native-20261126/execution-report.md).

Candidate 11's staged Windows x64 Release package and ARM64 cross-build retain
their limited acceptance. Three artifact-isolated Windows UI attempts did not
complete the app-level geometry/font/translation checks. The first rejected
the provider-prefixed root AutomationId; source and the existing P9 helper
confirm the exact identity is `QApplication.classMngrNextApplicationShell`.
The corrected attempt confirmed that root and moved the window, but foreground
activation failed, so it produced no screenshot, UIA close, or saved geometry.
The final retry stopped before UIA enumeration when PowerShell rejected a
`pid` parameter due to its case-insensitive collision with automatic `$PID`.
It terminated only the exact launch PID. No geometry persistence or Korean
visual result is accepted. Evidence is under
`artifacts/phase10/p10-win-c11-installed-runtime-20261126/interactive-followup-20261126/`,
`interactive-followup-retry1-20261126/`, and
`interactive-followup-retry2-20261126/`.

Read-only maps found no safe current-host real OS action for Candidates 6–8
and 10: the Windows print route enumerates the user's printer, Office COM
activation does not isolate the Office profile, the Windows updater starts
the selected package detached, and external URL/folder routes lack exact
process identity for cleanup. Candidate 9's Windows product dialog policy
uses Qt widgets; its forced Native test backend is not the normal Windows
product route. No printer, Office app, installer, browser, or Explorer action
was performed. The user has no native ARM64 or macOS host available. Phase 10
remains In Progress; this Windows follow-up is handed off paused, with the
native-interaction and cross-host gates open.
