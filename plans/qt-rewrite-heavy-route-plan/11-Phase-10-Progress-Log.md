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
