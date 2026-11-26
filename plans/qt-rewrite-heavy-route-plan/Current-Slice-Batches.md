# Current Slice Batches

## Phase 10 — Batch 1: Visual, behavioral, and cross-platform parity (In Progress; supplemental Windows x64 evidence recorded through Candidate 26)

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

Candidate 24's current-source Windows x64 offscreen Campus Directory and C46
capture follow-up has been recorded. It adds scoped build, focused test, route
capture, and visual-review evidence; native Windows UI and pixel-parity gates
remain open. See the [Candidate 24 report](../../artifacts/phase10/p10-win-c24-campus-directory-20261126/execution-report.md).
Candidate 25's current-source Windows x64 Class Transfer workflow and capture
follow-up is recorded. The focused Next workflow CTest passed 1/1; the paired
offscreen capture produced 24 readable images across four locale/theme pairs
and three states. The first missing-glyph capture is diagnostic only; the
test-only font harness correction and final rerun are documented. No pixel
parity or native-window claim is made. See the [Candidate 25 report](../../artifacts/phase10/p10-win-c25-class-transfer-20261126/execution-report.md).
Candidate 26's current-source Windows x64 Classes Analytics follow-up is
recorded. The focused Navigator page CTest passed 1/1, both selected Analytics
cases exited 0, and the paired offscreen capture produced 16 readable PNGs
covering All and Winter across four locale/theme pairs. The selected class and
viewport match in all manifest entries; 16 source hashes match. Shell and
viewport differences are observations only. See the [Candidate 26 report](../../artifacts/phase10/p10-win-c26-classes-analytics-20261126/execution-report.md).
Candidate 19's next-batch discovery produced Candidates 21–22; their
current-source requalifications did not establish product defects.

Candidate 2 also has a focused Windows x64 Debug Edit-action follow-up:
`shellEditActionsFollowFocusDispatchAndClipboard` passed under Qt offscreen.
This checks QAction dispatch and the isolated Qt clipboard backend only; native
Windows clipboard, physical focus, and UIA remain open. See the
[execution report](../../artifacts/phase10/p10-win-c2-edit-clipboard-offscreen-20261126/execution-report.md).

| Candidate | Status |
| --- | --- |
| 1. Focused Phase 7 verification | Linux focused subset and Windows x64 Debug selected subset passed 19/19 (265.79s); independently verified macOS `macos-clang-debug` passed the exact selection 19/19 in 22.74s on arm64. Universal `arm64;x86_64` binaries were built, but x86_64 was compile-time verified only. CTest was offscreen; warnings remain. Full Phase 7 was not rerun. Initial stale generated-resource manifest guard failure was resolved after preserving and hash-verifying the prior 190-file resource tree. [macOS report](../../artifacts/phase10/p10-macos-c1-exact19-20261126T061233Z-6981b584/report.md) |
| 2. Shell command parity | Focused Linux checks complete; Windows x64 offscreen check passed and the isolated Exit/geometry slot passed under Windows QPA with `qwindowsd.dll` loaded. macOS offscreen CTests passed 4/4 (Navigation 21/21, legacy About 3/3, Exit 5/5, Edit 6/6); the separate Next About slot passed 3/3. Isolated Cocoa Exit/geometry aborted before assertions with “no screens available” (134), an environment observation with no product defect established; native Cocoa behavior remains open. UIA/HWND identity, clipboard, focus, and other-host parity remain open. [Windows native-QPA report](../../artifacts/phase10/p10-win-c2-shell-exit-native-20261126/execution-report.md), [macOS report](../../artifacts/phase10/p10-macos-c2-shell-commands-20261126T065213Z-363f595c/report.md) |
| 3. Speaking Evaluation redo | Focused Linux checks complete; Windows x64 offscreen check passed and was accepted. Independently verified macOS `ClassMngrNextSpeakingEvaluationPageTests` passed 41/41 QtTest slots offscreen, including simulated Redo/Undo key-chord dispatch. The universal executable ran on arm64 only; x86_64 runtime, physical keyboard/Cocoa focus, and native parity remain open. Offscreen/font warnings remain; no code or test changes. [macOS report](../../artifacts/phase10/p10-macos-c3-speaking-redo-20261126T072332Z-6d4e3c15/report.md) |
| 4. Sub Prep page-break | Linux C53 pair passed at 12 pages each; Windows x64 accepted and independent macOS arm64 review accepted the user-selected grouping (Teachers 22–24 with notes on page 11; Sub Notes begins page 12). **Raster:** 12 Sub Prep + 5 roster pages matched at 150 DPI with 0 changed pixels. **SQL:** 750 names, 30 headers, 0 bullets. **Text:** PDFKit excerpts for pages 11–12 match, but the runner disables full text comparison; raw roster extraction differs (Legacy PUA punctuation, Next ASCII), Legacy audits include mapped tab controls, and no strict Next Sub Prep audit is present. Next roster strict audit passes. Broader text/copy fidelity remains open after Candidate 5; a source-aware text-layer follow-up is still needed before Phase 10 exit. Universal binaries ran arm64 only; no physical output. Windows ARM64 remains open. The older historical reference is sourceBound=false. [macOS report](../../artifacts/phase10/p10-macos-c4-c53-20261126T073111Z-9dcba15b/report.md) |
| 5. Output matrix | Linux and Windows x64 matrices complete; Windows x64 Schedule/Roster/Speaking passed 7/7, with Candidate 4 as the Sub Prep row. Independently reviewed macOS arm64: Schedule CTest 18/18, C51 Legacy/Next one landscape A4 page each, 0 changed pixels at 150 DPI within tolerance; Roster CTest 20/20, six page pairs exact in text/geometry/orientation/raster; Speaking slot 3/3, Standard/Advanced one 540×780 pt page each with matching text/pixels through a shared renderer (not independent-renderer parity). Candidate 4 C53 is carried forward, not rerun: selected grouping, 17-page raster, and SQL result accepted, but PDF text-layer/copy fidelity remains open. Archived Phase 7 Schedule image is not a cross-host gate; same-host Legacy/Next is exact. Universal binaries ran arm64 only; no physical print, printer, Office, or automation. Windows ARM64 and the full cross-host matrix remain open. [macOS report](../../artifacts/phase10/p10-macos-c5-output-matrix-20261126T075719Z-75fd7d20/report.md), [Candidate 4 report](../../artifacts/phase10/p10-macos-c4-c53-20261126T073111Z-9dcba15b/report.md) |
| 6. Native printing | Linux dialog/PDF checks complete; Windows x64 headless viewer/print-route evidence accepted. Independently verified macOS: three individual offscreen `ClassMngrNextDocumentViewerTests` slots passed with zero failures/errors/skips; the full registered CTest was inventoried but not run. The cancellation route constructs `QPrinter::NativeFormat` and `QPrintDialog`, but its injected hook returns `Rejected` before `exec()`; no explicit enumeration, panel, selection, or job occurred, though lower-level Qt printer-service queries are not ruled out. Native geometry after cancel, printer selection, system preview, jobs, and physical output remain open. Universal executable ran arm64 only; x86_64 runtime and native Cocoa remain unverified. [Windows evidence](../../artifacts/phase10/p10-win-36-c6-headless-print-20261126/report.md), [macOS report](../../artifacts/phase10/p10-macos-c6-safe-print-route-20261126T081814Z-eaf3b141/report.md) |
| 7. PowerPoint automation | Linux service/fallback checks complete; Windows x64 safe service/fallback tests passed 2/2. Independently verified macOS: two protected-data notice CTests and three offscreen fake-only direct slots passed 13 data cases with `CLASSMNGR_ENABLE_POWERPOINT_INTEGRATION_TESTS` unset. Real availability was excluded because it probes `/Applications/Microsoft PowerPoint.app` and `osascript`; neither was queried or launched. No Office, AppleScript, template, user Office state, or Automation permission was touched. Universal binaries ran arm64 only. Real installation/availability, activation, rendering, export, and fallback remain open; no Candidate 7 acceptance. [Windows evidence](../../artifacts/phase10/p10-win-c7-windows-x64-debug/report.md), [macOS report](../../artifacts/phase10/p10-macos-c7-powerpoint-safe-fallback-20261126T083606Z-c8350ef7/report.md) |
| 8. Updater | Linux focused checks complete; Windows x64 updater tests passed 2/2 with fake/loopback services. Windows primary action starts the selected package detached; a fake package would still launch. On macOS the restricted run passed coordinator but updater failed 16 assertions at loopback `listen()`; the same CTests then passed 2/2 in 0.94s on arm64 with approved escalation and unchanged binaries. This strongly points to an environment limit, but does not prove a kernel-level loopback-only privilege boundary. Universal binaries, no x86_64 runtime or rebuild. No package/open/Finder action; real macOS `open` and Finder remain unverified. [Windows evidence](../../artifacts/phase10/p10-win-22-updater-windows-x64-debug/report.md), [initial macOS run](../../artifacts/phase10/p10-macos-c8-updater-fake-actions-20261126T085306Z-71f97551/report.md), [macOS retry](../../artifacts/phase10/p10-macos-c8-updater-loopback-retry-20261126T090057Z-71f97551/report.md) |
| 9. Native file workflows | Linux fix and checks complete; Windows x64 focused workflow tests passed 10/10. The supplemental presenter cancellation slot passed under Windows QPA using an existing executable after a pre-compilation FileTracker failure. It exercises a Qt widget dialog, not the Windows product's system common-dialog backend; product `PlatformDefault` selects native dialogs only on macOS. Independently reviewed macOS policy CTest passed 1/1; two selected offscreen slots (forced Qt policy and Escape cancellation) passed with zero failures. The policy inspection does not run a native Cocoa panel or user path. Full presenter CTest, visual capture, native picker behavior, navigation, and visual parity remain open; universal binaries ran arm64 only. [Windows supplemental evidence](../../artifacts/phase10/p10-win-qt-dialog-qpa-20261126/report.md), [macOS report](../../artifacts/phase10/p10-macos-c9-file-dialog-20261126T090735Z-4a8c0ccf/report.md) |
| 10. External launches | Linux normalized-path fix and Windows x64 injected URL/folder checks passed, including a fake local-file opener. Independently reviewed macOS bounded evidence: five targets built, the shell-startup `ClassMngrNextUsefulLinksSelectionBehavior` CTest passed 1/1 (not URL dispatch), and five injected-handler/fake-callback slots passed; paths/settings were isolated. Universal executables ran arm64 only. No default browser/Finder dispatch, native picker/panel, or persistent user path was used. Actual macOS LaunchServices/browser/Finder behavior, x86_64 runtime, full suites, and visual captures remain open. Product dispatch still exposes no process/window identity for exact cleanup; the folder route creates/persists output. [Windows evidence](../../artifacts/phase10/p10-win-27-external-launch-windows-x64-debug/report.md), [macOS report](../../artifacts/phase10/p10-macos-c10-external-launches-20261126T092129Z-3b7d3446/report.md) |
| 11. Installed runtime | Windows x64 Release staging and Windows ARM64 Debug cross-build have limited independent acceptance; Linux evidence remains partial. ARM64 binaries were confirmed AA64/ARM64; no ARM64 CTests, app launch, or install. Three isolated Windows x64 UI attempts stopped before a normal-close/relaunch comparison: identity assertion mismatch (later resolved against the existing `QApplication.classMngrNextApplicationShell` convention), foreground activation denial, and a PowerShell `$PID` parameter collision. No installed geometry persistence or Korean screenshot is accepted. Independently reviewed macOS checks accepted two focused CTests, the Bootstrap slot, and a fresh universal signed Release app/package (919 paths, 498 SHA-256 files); 14.4 deployment minimum. DMG creation failed (`hdiutil: Device not configured`), so no image/checksum; staged-app offscreen smoke aborted before startup because the bundle lacks the offscreen plugin, with no product-defect inference. No x86_64 runtime, Finder/open, or `/Applications` install; runtime, close/relaunch geometry, window persistence, and native rendering remain open. [Windows x64](../../artifacts/phase10/p10-win-c11-installed-runtime-20261126/report.md), [Windows UI](../../artifacts/phase10/p10-win-c11-installed-runtime-20261126/interactive-followup-retry2-20261126/report.md), [Windows ARM64 cross-build](../../artifacts/phase10/p10-win-arm64-crossbuild-20261126/report.md), [macOS report](../../artifacts/phase10/p10-macos-c11-installed-runtime-20261126T095442Z-4f4a3ee0/report.md) |
| 12. My Classes visuals | Linux disposition and Windows x64 populated/empty/error captures accepted for supported states (3 focused capture CTests). Independent macOS review verified 5 page slots, 3 direct offscreen captures, all 53 hashes/provenance, and 28 PNGs (populated 8, empty 8, summary-error 12; four locale/theme pairs); images are 1024×768 except Legacy warning dialogs. Next navigation buttons appear in empty/error images where Legacy lacks them; Legacy modal versus Next inline error is observational only, with no comparator or parity/defect conclusion. Existing fixed capture directory's 9 files/hashes were preserved. Universal binaries ran arm64 only. Forced grade-tab overflow and native Cocoa rendering remain open. [macOS report](../../artifacts/phase10/p10-macos-c12-my-classes-20261126T102031Z-d27bfd04/report.md), [visual observations](../../artifacts/phase10/p10-macos-c12-my-classes-20261126T102031Z-d27bfd04/visual-observations.md), [Windows state evidence](../../artifacts/phase10/p10-win-29-my-classes-state-followup-20261126/report.md) |
| 13. Schedule display | Linux disposition complete; Windows x64 page/capture CTests passed 2/2 after a test-only font-metric precision correction; pixel parity is not claimed and the screenshot-order question remains open. Bounded macOS: two focused page slots and direct captures (19 default Legacy/Next pairs, 2 stress pairs) passed; independent review verified artifact hashes/provenance and 33 existing fixed-output files unchanged. No pixel/content parity claim; `sameModeAndWorkspaceData=true` is hard-coded. Four 256-character Regular/Testing stress entries report `documentFitsCellContent=false` in 60 px cells, without a fit assertion: unresolved overflow characterization, not a pass or product-defect conclusion. No Windows evidence is in the macOS bundle. Universal binaries ran arm64 only; native Cocoa/interactive behavior and x86_64 runtime remain open. Candidate 13 and Phase 10 acceptance remain open. [Windows evidence](../../artifacts/phase10/p10-win-13-schedule-harness-fix-20261126T065642/report.md), [macOS report](../../artifacts/phase10/p10-macos-c13-schedule-display-20261126T103458Z-e670bdc0/report.md), [visual observations](../../artifacts/phase10/p10-macos-c13-schedule-display-20261126T103458Z-e670bdc0/visual-observations.md) |
| 14. Schedule Import preview | Linux disposition complete; Windows x64 model/dialog/integration tests and Legacy/Next captures passed. Bounded macOS: conflict slot and Legacy/Next capture slots passed; independent review verified 16 Legacy + 40 Next scenes, hashes/provenance, and isolated inputs. English/light Next shows only 48 px of a 553 px Meeting-times cell, while Display/AccessibleText/ToolTip roles retain full values; this is measurement only, with no fit assertion, parity, defect, or acceptance conclusion. No native picker, hover, VoiceOver, or user path; universal binaries ran arm64 only. The two large XLSX fixture hashes were omitted from `source-fixtures.sha256`, though HEAD/tree and clean state are recorded. Live tooltip/accessibility, native Cocoa, and x86_64 runtime remain open. [Windows evidence](../../artifacts/phase10/p10-b1-c14-preview-access-conflict-windows-x64-20261126T065044/report.md), [macOS report](../../artifacts/phase10/p10-macos-c14-schedule-import-20261126T105423Z-07d78ccc/report.md), [measurements](../../artifacts/phase10/p10-macos-c14-schedule-import-20261126T105423Z-07d78ccc/preview-measurements.md) |
| 15. Teacher Profile | Windows x64 capture and focused birthday-clear/save/reopen slot passed. Bounded macOS: selected `teacherProfileUsesTypedOpenAndStaticPageHistory` passed and standalone capture produced four 1024×768 Legacy/Next images. Captures show March 14 and a visible Next clear glyph; Legacy English/light has no distinguishable glyph and Korean/dark is subtle. These are observations, not parity or defect conclusions. Universal binaries ran arm64 only. Native Cocoa popup placement, physical input, IME, accessibility, and Windows ARM64 remain open. [Windows evidence](../../artifacts/phase10/p10-win-15-teacher-profile-followup-20261126/report.md), [macOS report](../../artifacts/phase10/p10-macos-c15-teacher-profile-20261126T111138Z-3f3faf6a/report.md) |
| 16. My Details and Signature | Windows x64 four-locale/theme capture and three focused signature slots passed. macOS arm64: three focused signature slots passed, but two capture attempts exited 139 after producing only four en_US/light images and no manifest; the 16-image matrix remains open. Crash reports localize the caller to `QApplication::setStyleSheet` at line 641 during en_US/dark; EXC_BAD_ACCESS address `0x30`, cause unknown. Escalated LLDB yielded no backtrace; Rosetta returned “Bad CPU type” before app launch, so no x86_64 runtime. Fixed output was preserved. Native picker/popup, keyboard/IME, screen-reader, and remaining capture states remain open. [Windows evidence](../../artifacts/phase10/p10-win-16-my-details-signature-20261126/report.md), [macOS evidence](../../artifacts/phase10/p10-macos-c16-my-details-signature-20261126T112448Z-5c79c863/report.md), [crash diagnostic](../../artifacts/phase10/p10-macos-c16-crash-diagnostic-20261126T114253Z-5c79c863/report.md), [Rosetta probe](../../artifacts/phase10/p10-macos-c16-rosetta-20261126T115517Z-5c79c863/report.md) |
| 17. Staff Directory | Windows x64 bounded evidence independently accepted: unit 1/1, four registered CTests, four direct cases, 16 readable PNGs/eight manifests, and 67 hashes verified; page-relative table bottom is Legacy 670/Next 676 (+6 px). Bounded macOS: unit CTest 1/1; four offscreen Native/GS captures produced eight 1024×768 PNGs/four manifests, each recording 120 rows and matching repository order. Legacy table bottom=670, Next=676 (+6 px), header=38 px, rows=42 px. Descriptive only; no geometry/pixel threshold or parity claim. Existing fixed output was not targeted; no before/after hash inventory. Native DPI/window behavior, Windows ARM64, and x86_64 runtime remain open. [Windows review](../../artifacts/phase10/p10-win-37-staff-directory-20261126/review-summary.json), [macOS report](../../artifacts/phase10/p10-macos-c17-shared-staff-directory-20261126T115929Z-870f2356/report.md) |
| 18. Upcoming Birthdays | Windows x64 bounded evidence independently accepted: focused CTests 7/7, 16 readable PNGs/four manifests, and 20 hashes verified; Legacy/Next images match byte-for-byte per locale/theme pair. Bounded macOS: focused flow CTest 7/7; fresh en_US/dark and ko_KR/light captures produced eight 520×560 PNGs/two manifests. Independent review accepted this scoped evidence and found the four Legacy/Next pairs byte-identical for these cases only. Populated counts are 1 today/1 this week/2 next week; localized empty messages, unchecked dismiss checkbox, and Close are visible. No general pixel comparator/parity claim. Existing output/history was not targeted; no before/after hash inventory. Universal binaries ran arm64 only; native DPI/window behavior remains open. [Windows report](../../artifacts/phase10/p10-win-18-upcoming-birthdays-20261126T0852KST/report.md), [macOS report](../../artifacts/phase10/p10-macos-c18-upcoming-birthdays-20261126T121421Z-ded5b8e8/report.md) |
| 19. My Workspace Calendar | Windows x64 evidence independently accepted: 21 PNGs and 134 hashes verified. Lower Upcoming heading and an event row are visible in all eight Legacy/Next locale/theme captures. Korean Legacy shell labels remain English while Next labels are translated; observed strings are recorded without a defect conclusion. Page/Calendar CTests passed in an earlier valid run; the capture CTest initially failed on scroll settling and passed 1/1 after the harness fix. Independently reviewed macOS: page CTest 1/1; 21 images/13 scenarios with heading and event visible in all eight lower-Upcoming records. Korean/Next shell labels are localized while Korean/Legacy labels stay English; both show `예정된 일정`. Before/after hashes matched across 81 preserved build-output and historical baseline files. This is observation only, with no defect or acceptance conclusion. Universal binaries ran arm64 only; native Cocoa/window/DPI and x86_64 runtime remain open. [Windows report](../../artifacts/phase10/p10-win-c19-calendar-20261126/report.md), [macOS report](../../artifacts/phase10/p10-macos-c19-my-workspace-calendar-20261126T122757Z-15c60e12/report.md) |
| 20. Classes states/localization | Windows x64 evidence independently accepted: 24 PNGs, 12 manifest entries, four locale/theme pairs, and 37 hashes verified; both focused CTests passed. The first build hit MSBuild FileTracker E_ACCESSDENIED with user-temp paths; the artifact-local retry succeeded. Next read-error text is clipped at the 800×600 edge, while status and Retry remain visible. Bounded macOS: Classes Navigator CTest 1/1; 24 verified 800×600 PNGs/12 pairs across three states, two locales, and two themes. Independent review observed clipped Next Retry/trailing diagnostic, plus Legacy warning over My Workspace/Schedule versus Next Classes backdrop. Observation only: no pixel parity, Retry-functionality, native AppKit, defect, or acceptance conclusion. Universal binaries ran arm64 only; native window/DPI remains open. [Windows report](../../artifacts/phase10/p10-win-c20-classes-20261126/review-notes.md), [macOS report](../../artifacts/phase10/p10-macos-c20-classes-full-window-state-20261126T124035Z-b77be502/report.md), [captures](../../artifacts/phase10/p10-macos-c20-classes-full-window-state-20261126T124035Z-b77be502/captures/matched-full-window-state-captures.json) |
| 21. Initial Setup Wizard locale/theme capture coverage | Windows x64 evidence independently accepted: 60 final PNGs (28 Legacy/Next stage pairs plus four Next personal-details captures), eight font records, 325 hashes, and five focused CTests passed after harness fix/assertions. Legacy capture now loads/applies bundled FontManager fonts; final Korean glyphs are readable. Preserved pre-fix tofu captures are diagnostic only. [Review](../../artifacts/phase10/p10-win-c21-initial-setup-wizard-20261126/visual-review.md) |
| 22. Document Catalog paired visual review | Windows x64 current-source sidebar-region evidence independently accepted: 12 PNGs and 24 hashes; eight matched 254×630 Legacy/Next tree images across four locale/theme pairs plus four Next full-shell captures. Two focused CTests passed. Manifest records 7 folders, 30 documents, the same selected stable ID, and no loaded PDF body. Korean labels and selection are visible; some long Next labels elide at the fixed width. Scope is sidebar-only, with no full-window Legacy parity claim. [Review notes](../../artifacts/phase10/p10-win-c22-document-catalog-20261126/review-notes.md) |
| 24. Campus Directory and C46 Admin states | Current-source Windows x64 Debug build exited 0; focused Campus Directory CTest passed 1/1 and the capability-gated Admin navigation slot passed 3/3. Offscreen review decoded 40 paired C41 route captures and 12 C46 menu/authorized/read-only states across four locale/theme pairs. All nine source hashes match. Observations only: no pixel-parity or native-window claim. [Execution report](../../artifacts/phase10/p10-win-c24-campus-directory-20261126/execution-report.md) |
| 25. Class Transfer workflow and captures | Current-source Windows x64 Debug build exited 0; focused Next workflow CTest passed 1/1 (46.35s). Final offscreen capture produced 24 readable Legacy/Next PNGs at 1024×768 across four locale/theme pairs and three states; ten source hashes match. Bundled-font registration was corrected in the capture harness, and the initial tofu run is preserved as diagnostic only. No pixel-parity or native-window claim. [Execution report](../../artifacts/phase10/p10-win-c25-class-transfer-20261126/execution-report.md) |
| 26. Classes Analytics All/Winter states | Current-source Windows x64 Debug build exited 0; focused Navigator page CTest passed 1/1 and both selected Analytics cases exited 0. Final offscreen paired capture produced 16 decodable 1024×768 PNGs across four locale/theme pairs. All 16 source hashes match; labels are readable and no missing glyphs were observed. Shell and viewport differences are descriptive only, with no pixel-parity claim. [Execution report](../../artifacts/phase10/p10-win-c26-classes-analytics-20261126/execution-report.md) |

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

## Next batch — Phase 10 Batch 2: Current-source Windows x64 visual follow-ups (Candidate 26 recorded; next: read-only discovery)

Discovery at source HEAD `fe48d3484be80fbdc69ce9e299a1154192ed0fc6` found:

- **Document Viewer (previously proposed Candidate 23): already covered.** The
  accepted Windows x64 Candidate 6 artifact contains 16 Next and 16 Legacy
  current-source-equivalent captures across all four locale/theme pairs and
  Ready/error/closed/reopened states. All 34 C++/header source inputs still
  match; the only source-inventory difference is an unrelated CMake test-file
  change. No duplicate capture run is needed. See the [Candidate 6 report](../../artifacts/phase10/p10-win-36-c6-headless-print-20261126/report.md).
- **Candidate 24 — Campus Directory and C46 Admin states.** The current-source
  Windows x64 Debug run is recorded. The focused directory CTest and
  capability-gated Admin-navigation slot passed. Fresh offscreen evidence
  contains 40 C41 Legacy/Next route PNGs plus 12 C46 menu, authorized, and
  read-only PNGs across all four locale/theme pairs. The manifests, 52 decoded
  images, representative localized states, and nine source hashes were
  reviewed. See the [execution report](../../artifacts/phase10/p10-win-c24-campus-directory-20261126/execution-report.md).
  Legacy and Next retain different page structures, so this remains
  observational and makes no pixel-parity claim.

- **Candidate 25 — Class Transfer.** Read-only discovery found no current-source
  Windows x64 capture package, and the final bounded requalification is now
  recorded. The Next workflow CTest passed 1/1; the offscreen capture produced
  24 readable Legacy/Next images for export selection, import review, and
  schedule-conflict feedback across four locale/theme pairs. A test-only
  correction registers the packaged Inter and Pretendard fonts; the first
  missing-glyph run is preserved as diagnostic only. The broad Legacy
  aggregate has a historical failure of unknown origin and remains outside
  this slice. No pixel-parity or native-window claim is made. See the
  [execution report](../../artifacts/phase10/p10-win-c25-class-transfer-20261126/execution-report.md),
  [discovery report](../../artifacts/phase10/p10-win-c25-class-transfer-discovery-20261126/discovery-report.md),
  and [font correction report](../../artifacts/phase10/p10-win-c25-class-transfer-20261126/font-harness-correction-report.md).
- **Candidate 26 — Classes Analytics.** The current-source Windows x64 Debug
  build, focused Navigator page CTest, both selected Analytics cases, and
  paired capture are recorded. The 16-image matrix covers Legacy/Next All and
  Winter across four locale/theme pairs; the PNGs decode at 1024x768 and the
  labels are readable. The first FileTracker and default-QPA attempts are
  retained as diagnostics. No pixel-parity or defect conclusion is made. See
  the [execution report](../../artifacts/phase10/p10-win-c26-classes-analytics-20261126/execution-report.md),
  [visual review](../../artifacts/phase10/p10-win-c26-classes-analytics-20261126/visual-review.md),
  and [discovery report](../../artifacts/phase10/p10-win-c26-classes-analytics-discovery-20261126/discovery-report.md).

Candidate 11's Windows x64 installed geometry/font follow-up remains open and
separate. Candidate 26's current-source Windows x64 requalification is
recorded in the [execution report](../../artifacts/phase10/p10-win-c26-classes-analytics-20261126/execution-report.md).
The next slice is read-only discovery of another safe current-source Windows
x64 follow-up. Native interaction, Windows ARM64, macOS, and cross-host Phase
10 gates remain open.
Read-only discovery evidence:
[report](../../artifacts/phase10/p10-win-next-batch-discovery-20261126/discovery-report.md).
