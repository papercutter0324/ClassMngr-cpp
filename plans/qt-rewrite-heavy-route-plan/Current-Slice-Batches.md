# Current Slice Batches

## Phase 10 — Batch 1: Visual, behavioral, and cross-platform parity (In Progress; next: remaining Windows-native interaction and cross-host gates)

Batch goal: requalify migrated behavior, appearance, and outputs against the
legacy product on supported platforms using current-source evidence. Record
source, toolchain, fixture, comparator version, and raw artifacts. The
candidates below are ordered discovery targets, not confirmed current-source
defects. Use the existing output comparators where applicable; keep the
Sub Prep W7.5.1 page-break requalification distinct from the broader output
matrix. Search remaining migrated product surfaces during next-batch discovery.

### Candidate status — 2026-11-26

Phase 10 remains In Progress and its cross-platform exit gate remains open.
“Linux complete” below records this host's bounded evidence; it does not close
the cross-platform Phase 10 requirement.

Supplemental Windows x64 follow-ups for Candidates 19–22 are closed. Candidate
2's isolated Exit/geometry slot passed under Windows QPA and independently
verified `qwindowsd.dll` loading; shell-HWND identity, clipboard, and UIA
coverage remain open. Candidate 9's presenter cancellation slot remains a
limited Qt-widget check; Windows product `PlatformDefault` selects Qt widgets,
while the forced Native backend is not the normal Windows route. Read-only
maps found no safe current-host interaction for Candidates 6–8 or 10: printing
enumerates the live default printer, Office COM activation does not isolate
the Office profile, Windows updater action launches a package, and URL/folder
dispatch lacks process identity for exact cleanup. Candidate 11's staged x64
Release package and ARM64 Debug cross-build remain limited. Three isolated
x64 interactive attempts did not produce installed geometry persistence or a
Korean visual capture; the latest stopped on a PowerShell `$PID` parameter
collision before UIA enumeration. All Candidates 1–22 have bounded Windows
x64 follow-up evidence. Candidate 1 also has independently verified bounded
macOS evidence. Native-interaction and cross-host gates remain open; no native
Windows ARM64 host is available.
Candidate 19's next-batch discovery produced Candidates 21–22; their
current-source requalifications did not establish product defects.

| Candidate | Status |
| --- | --- |
| 1. Focused Phase 7 verification | Linux focused subset and Windows x64 Debug selected subset passed 19/19 (265.79s); independently verified macOS `macos-clang-debug` passed the exact selection 19/19 in 22.74s on arm64. Universal `arm64;x86_64` binaries were built, but x86_64 was compile-time verified only. CTest was offscreen; warnings remain. Full Phase 7 was not rerun. Initial stale generated-resource manifest guard failure was resolved after preserving and hash-verifying the prior 190-file resource tree. [macOS report](../../artifacts/phase10/p10-macos-c1-exact19-20261126T061233Z-6981b584/report.md) |
| 2. Shell command parity | Focused Linux checks complete; Windows x64 offscreen check passed and the isolated Exit/geometry slot passed under Windows QPA with `qwindowsd.dll` loaded. macOS offscreen CTests passed 4/4 (Navigation 21/21, legacy About 3/3, Exit 5/5, Edit 6/6); the separate Next About slot passed 3/3. Isolated Cocoa Exit/geometry aborted before assertions with “no screens available” (134), an environment observation with no product defect established; native Cocoa behavior remains open. UIA/HWND identity, clipboard, focus, and other-host parity remain open. [Windows native-QPA report](../../artifacts/phase10/p10-win-c2-shell-exit-native-20261126/execution-report.md), [macOS report](../../artifacts/phase10/p10-macos-c2-shell-commands-20261126T065213Z-363f595c/report.md) |
| 3. Speaking Evaluation redo | Focused Linux checks complete; Windows x64 offscreen check passed and was accepted. Independently verified macOS `ClassMngrNextSpeakingEvaluationPageTests` passed 41/41 QtTest slots offscreen, including simulated Redo/Undo key-chord dispatch. The universal executable ran on arm64 only; x86_64 runtime, physical keyboard/Cocoa focus, and native parity remain open. Offscreen/font warnings remain; no code or test changes. [macOS report](../../artifacts/phase10/p10-macos-c3-speaking-redo-20261126T072332Z-6d4e3c15/report.md) |
| 4. Sub Prep page-break | Linux C53 pair passed at 12 pages each; Windows x64 accepted and independent macOS arm64 review accepted the user-selected grouping (Teachers 22–24 with notes on page 11; Sub Notes begins page 12). **Raster:** 12 Sub Prep + 5 roster pages matched at 150 DPI with 0 changed pixels. **SQL:** 750 names, 30 headers, 0 bullets. **Text:** PDFKit excerpts for pages 11–12 match, but the runner disables full text comparison; raw roster extraction differs (Legacy PUA punctuation, Next ASCII), Legacy audits include mapped tab controls, and no strict Next Sub Prep audit is present. Next roster strict audit passes. Broader text/copy fidelity remains open for Candidate 5. Universal binaries ran arm64 only; no physical output. Windows ARM64 remains open. The older historical reference is sourceBound=false. [macOS report](../../artifacts/phase10/p10-macos-c4-c53-20261126T073111Z-9dcba15b/report.md) |
| 5. Output matrix | Linux and Windows x64 matrices complete; Windows x64 Schedule/Roster/Speaking passed 7/7, with Candidate 4 as the Sub Prep row. Independently reviewed macOS arm64: Schedule CTest 18/18, C51 Legacy/Next one landscape A4 page each, 0 changed pixels at 150 DPI within tolerance; Roster CTest 20/20, six page pairs exact in text/geometry/orientation/raster; Speaking slot 3/3, Standard/Advanced one 540×780 pt page each with matching text/pixels through a shared renderer (not independent-renderer parity). Candidate 4 C53 is carried forward, not rerun: selected grouping, 17-page raster, and SQL result accepted, but PDF text-layer/copy fidelity remains open. Archived Phase 7 Schedule image is not a cross-host gate; same-host Legacy/Next is exact. Universal binaries ran arm64 only; no physical print, printer, Office, or automation. Windows ARM64 and the full cross-host matrix remain open. [macOS report](../../artifacts/phase10/p10-macos-c5-output-matrix-20261126T075719Z-75fd7d20/report.md), [Candidate 4 report](../../artifacts/phase10/p10-macos-c4-c53-20261126T073111Z-9dcba15b/report.md) |
| 6. Native printing | Linux dialog/PDF checks complete; Windows x64 headless viewer/print-route evidence accepted. The cancellation slot bypasses `QPrintDialog`; Windows product flow enumerates the live printer list/default first and no fake-printer seam exists. No dialog or printer was touched; actual dialogs, printers, jobs, and physical output remain open. [Windows evidence](../../artifacts/phase10/p10-win-36-c6-headless-print-20261126/report.md) |
| 7. PowerPoint automation | Linux service/fallback checks complete; Windows x64 safe service/fallback checks passed 2/2. Registration is only an availability heuristic; live COM activation/rendering remain open because Office profile state is not isolated. [Windows evidence](../../artifacts/phase10/p10-win-c7-windows-x64-debug/report.md) |
| 8. Updater | Linux focused checks complete; Windows x64 updater tests passed 2/2 with fake/loopback services. Windows primary action starts the selected package detached; a fake package would still launch. Native installer/folder actions remain open. [Windows evidence](../../artifacts/phase10/p10-win-22-updater-windows-x64-debug/report.md) |
| 9. Native file workflows | Linux fix and checks complete; Windows x64 focused workflow tests passed 10/10. The supplemental presenter cancellation slot passed under Windows QPA using an existing executable after a pre-compilation FileTracker failure. It exercises a Qt widget dialog, not the Windows product's system common-dialog backend; product `PlatformDefault` selects native dialogs only on macOS. UIA, navigation, and visual parity remain open. [Supplemental evidence](../../artifacts/phase10/p10-win-qt-dialog-qpa-20261126/report.md) |
| 10. External launches | Linux normalized-path fix and Windows x64 injected URL/folder checks passed, including a fake local-file opener. Product dispatch exposes no process/window identity for exact cleanup; the folder route creates/persists output. Registered OS handlers and real browser/Explorer launches remain open. [Windows evidence](../../artifacts/phase10/p10-win-27-external-launch-windows-x64-debug/report.md) |
| 11. Installed runtime | Windows x64 Release staging and Windows ARM64 Debug cross-build have limited independent acceptance; Linux evidence remains partial. ARM64 binaries were confirmed AA64/ARM64; no ARM64 CTests, app launch, or install. Three isolated Windows x64 UI attempts stopped before a normal-close/relaunch comparison: identity assertion mismatch (later resolved against the existing `QApplication.classMngrNextApplicationShell` convention), foreground activation denial, and a PowerShell `$PID` parameter collision. No installed geometry persistence or Korean screenshot is accepted; installed macOS runtime behavior remains unverified. [x64 report](../../artifacts/phase10/p10-win-c11-installed-runtime-20261126/report.md), [interactive evidence](../../artifacts/phase10/p10-win-c11-installed-runtime-20261126/interactive-followup-retry2-20261126/report.md), [ARM64 report](../../artifacts/phase10/p10-win-arm64-crossbuild-20261126/report.md) |
| 12. My Classes visuals | Linux disposition and Windows x64 populated/empty/error captures accepted for supported states (3 focused capture CTests); forced grade-tab overflow remains open because the current grade contract collapses unknown labels to “Other.” [Windows state evidence](../../artifacts/phase10/p10-win-29-my-classes-state-followup-20261126/report.md) |
| 13. Schedule display | Linux disposition complete; Windows x64 page/capture CTests passed 2/2 after a test-only font-metric precision correction; pixel parity is not claimed and one screenshot-order question remains inconclusive. ARM64/macOS and interactive behavior remain open. [Windows evidence](../../artifacts/phase10/p10-win-13-schedule-harness-fix-20261126T065642/report.md) |
| 14. Schedule Import preview | Linux disposition complete; Windows x64 model/dialog/integration tests and Legacy/Next captures passed; live tooltip and screen-reader behavior, ARM64/macOS remain open. [Windows evidence](../../artifacts/phase10/p10-b1-c14-preview-access-conflict-windows-x64-20261126T065044/report.md) |
| 15. Teacher Profile | Windows x64 capture and focused birthday-clear/save/reopen slot passed; native popup/pointer, keyboard/IME, accessibility, ARM64/macOS remain open. [Windows evidence](../../artifacts/phase10/p10-win-15-teacher-profile-followup-20261126/report.md) |
| 16. My Details and Signature | Windows x64 four-locale/theme capture and three focused signature slots passed; native picker, keyboard/IME, screen-reader, ARM64/macOS remain open. [Windows evidence](../../artifacts/phase10/p10-win-16-my-details-signature-20261126/report.md) |
| 17. Staff Directory | Windows x64 bounded evidence independently accepted: unit 1/1, four registered CTests, four direct cases, 16 readable PNGs/eight manifests, and 67 hashes verified; page-relative table bottom is Legacy 670/Next 676 (+6 px). Native DPI/window behavior, ARM64/macOS remain open. [Review summary](../../artifacts/phase10/p10-win-37-staff-directory-20261126/review-summary.json) |
| 18. Upcoming Birthdays | Windows x64 bounded evidence independently accepted: focused CTests 7/7, 16 readable PNGs/four manifests, and 20 hashes verified; Legacy/Next images match byte-for-byte per locale/theme pair. Native display, ARM64/macOS remain open. [Windows report](../../artifacts/phase10/p10-win-18-upcoming-birthdays-20261126T0852KST/report.md) |
| 19. My Workspace Calendar | Windows x64 evidence independently accepted: 21 PNGs and 134 hashes verified. Lower Upcoming heading and an event row are visible in all eight Legacy/Next locale/theme captures. Korean Legacy shell labels remain English while Next labels are translated; observed strings are recorded without a defect conclusion. Page/Calendar CTests passed in an earlier valid run; the capture CTest initially failed on scroll settling and passed 1/1 after the harness fix. [Report](../../artifacts/phase10/p10-win-c19-calendar-20261126/report.md) |
| 20. Classes states/localization | Windows x64 evidence independently accepted: 24 PNGs, 12 manifest entries, four locale/theme pairs, and 37 hashes verified; both focused CTests passed. The first build hit MSBuild FileTracker E_ACCESSDENIED with user-temp paths; the artifact-local retry succeeded. Next read-error text is clipped at the 800×600 edge, while status and Retry remain visible. [Report](../../artifacts/phase10/p10-win-c20-classes-20261126/review-notes.md) |
| 21. Initial Setup Wizard locale/theme capture coverage | Windows x64 evidence independently accepted: 60 final PNGs (28 Legacy/Next stage pairs plus four Next personal-details captures), eight font records, 325 hashes, and five focused CTests passed after harness fix/assertions. Legacy capture now loads/applies bundled FontManager fonts; final Korean glyphs are readable. Preserved pre-fix tofu captures are diagnostic only. [Review](../../artifacts/phase10/p10-win-c21-initial-setup-wizard-20261126/visual-review.md) |
| 22. Document Catalog paired visual review | Windows x64 current-source sidebar-region evidence independently accepted: 12 PNGs and 24 hashes; eight matched 254×630 Legacy/Next tree images across four locale/theme pairs plus four Next full-shell captures. Two focused CTests passed. Manifest records 7 folders, 30 documents, the same selected stable ID, and no loaded PDF body. Korean labels and selection are visible; some long Next labels elide at the fixed width. Scope is sidebar-only, with no full-window Legacy parity claim. [Review notes](../../artifacts/phase10/p10-win-c22-document-catalog-20261126/review-notes.md) |

### Ordered candidates

1. **Focused Linux verification of migrated Phase 7 behavior** — complete the
   Phase 7.5 transfer only; do not rerun the full Phase 7 suite.
2. **Application shell command parity** — compare legacy Exit, Undo/Redo,
   Cut/Copy/Paste, and About with the Next shell; verify shortcuts and enabled
   behavior and confirm the product contract. Compare
   src/app/menu_builder.cpp with src/next/ui/application_shell.cpp.
3. **Speaking Evaluation redo parity** — verify the redo control and shortcut
   against behavior and tests in src/next/ui/speaking_evaluation_page.cpp and
   tests/next/speaking_evaluation_page_tests.cpp.
4. **Sub Prep W7.5.1 page-break output requalification** — the user-selected product
   grouping is Teachers 22–24 on page 11, with Sub Notes on page 12. Windows
   x64 evidence is accepted; compare remaining hosts. The historical reference
   is not source-bound.
5. **Current-source output comparison matrix** — compare Schedule, Roster, Speaking
   reports, and Sub Prep C53 across platforms. Linux and Windows x64 evidence
   is complete; Windows ARM64/macOS remain open. Keep distinct from 4.
6. **Native printer and print-preview parity** — a safe feasibility map found
   the existing cancellation slot does not open `QPrintDialog`; the product
   Windows route uses the default printer before opening it. No dialog or
   printer was touched; compare complete native workflows across platforms.
7. **PowerPoint automation behavior** — verify automation availability and
   fallback behavior on supported hosts.
8. **Updater runtime behavior** — verify installer/folder actions and
   platform-specific branches.
9. **Native file workflows** — the existing presenter cancellation slot passed
   under Windows QPA using the pre-existing executable after a pre-compilation
   FileTracker build failure. Native picker behavior remains open.
10. **External launch behavior** — compare external URL and folder launches
    across platforms.
11. **Installed-runtime matrix** — Windows x64 Release staging has a limited
    pass; Linux evidence remains partial. The Windows ARM64 Debug cross-build
    is accepted as cross-build-only; ARM64 runtime tests and installed-app
    geometry across relaunches and visual font/translation rendering remain
    unverified. Native GUI, installer, Program Files, and macOS remain outside
    the accepted result.
12. **My Classes visual disposition** — review documented layout and
    typography differences with fresh source-bound captures.
13. **Schedule display visual disposition** — review display toolbar,
    typography, wrapping/elision, and overflow indicators.
14. **Schedule Import preview disposition** — review normal preview clipping
    and conflict-status detail behavior.
15. **Teacher Profile visual disposition** — review residual geometry and the
    birthday-clear affordance.
16. **My Details and Signature capture review** — complete missing locale/theme
    captures and review the accepted vertical reveal.
17. **Staff Directory geometry review** — compare table height and geometry
    across current modes and themes.
18. **Upcoming Birthdays missing captures** — capture English/dark and
    Korean/light for populated and empty states.
19. **My Workspace Calendar disposition** — capture lower Upcoming content
    and disposition the reported Korean shell-label difference.
20. **Classes state and localization disposition** — capture full-window
    empty, delete, read-error, modal-inline, and localization states.

Candidate 19's required next-batch discovery ran on 2026-11-26 and identified
Candidates 21–22 above. The Initial Setup Wizard and Document Catalog are
discovery targets, not verified defects. The Document Viewer and Campus
Directory already have historical four-pair Legacy/Next capture packages;
their cross-host evidence is still distinct from current-source requalification.
Historical visual findings under
docs/qt-rewrite/visual-baseline/next/phase-7/ and behavior/output evidence in
the Phase 7, 7.5, 8, and 8a progress logs are references for discovery only;
requalify against current source before calling a difference a defect.
