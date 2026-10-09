# ClassMngr Qt-Rewrite — Start Here

## Collective status

- Overall status: In progress
- Default route: Heavy route
- Branch scope: Qt-Rewrite
- Last updated: 2026-10-09
- Current milestone: Phase 2 remains in progress. F253 (Qt-free private-notes
  splitter), F254 (Qt-free roster-score assignment planning), F255 (single-
  report AI eligibility policy reuse), F256 (typed roster read cutover for
  Speaking Evaluation name workflows), F257 (selected-class report-context
  reads), and F258 (canonical evaluation-name policy) are accepted; F259
  (roster-transfer target metadata cutover), F260 (roster evaluation-column
  classification), F261 (RosterPrintDialog class-label reads), F262
  (Class Transfer dialog class-label reads), F263 (matched-teacher labels in
  ClassImportDialog), F264 (optional roster reads in RosterPrintDialog),
  F265 (transfer-menu target-roster reads), F266 (ClassExportDialog class-list
  reads), F267 (RosterPrintDialog normal class-list read), F268
  (MyClassesPage class-list read), F269 (setup wizard teacher-choice list
  read), F270 (MyClassesPage assigned-teacher profile read), F271 (sidebar
  delete-prompt class display-name read), F272 (RosterPrintDialog
  current-class-only testing-class read), F273 (transfer-time target-roster
  read), and F274 (RosterPrintDialog extra-info class-list read) are accepted;
  F275 (class-delete chooser class-list read), F276 (upcoming Native English
  and GS birthday-directory reads), F277 (roster transfer-menu class-list
  read), F278 (initial-setup wizard teacher-existence reads), and F279
  (sidebar selected-teacher profile read), F280 (sidebar teacher-delete
  chooser teacher-list read), F281 (sidebar teacher-list refresh read), and
  F282 (sidebar action-state list reads), F283 (My Classes roster-backed
  student counts), and F284 (recheck the selected class before delete
  confirmation) are accepted; F285 is deferred because required-name
  validation prevents its post-create profile read from being reached; F286
  (use the workspace session location for FileController create/open state) is
  accepted; F287 (remove ClassImportDialog's unreachable direct class lookup),
  F288 (Campus Dashboard selector campus-directory read), F289 (roster-template
  print class-scope enumeration through the classes-list query), and F290
  (roster-template print per-class roster read through the roster query) are
  accepted; F291 (My Classes dedicated compact class-information
  query/snapshot/port with one ApplicationServices adapter read) is accepted;
  F292 (sidebar Korean birthday-directory read) is accepted; F293 (sidebar
  class-teacher-assignment read) is accepted; F294 (latest-import-date read for
  teacher import) is accepted; F295 (roster-template reuse of classes-list
  names) and F296 (purpose-fit class-information projection for roster-template
  printing) are accepted; F297 (selected-campus detail read for Campus
  Dashboard) is accepted; F298 (`SidebarController::addClass()` post-create
  reread) remains deferred pending a decision on read-failure warning and
  navigation behavior; F299 (Class Analytics dashboard read/use case) is
  accepted; its separate missed-slice completeness audit found no additional
  direct legacy-read routes and identified F302-F305 as batching candidates.
  F300 (batch ClassImportDialog destination-label subtitle reads), F301
  (batch ClassExportDialog selected-subtitle reads), F302 (batch class-delete
  chooser subtitle-label reads following F275), F303 (batch RosterPrintDialog
  per-class subtitle reads after F261), and F304 (batch RosterPrintDialog
  extra-column roster reads after F264) and F305 (batch transfer-menu target
  metadata, capacity, and roster reads after F259/F265) are accepted; F305
  remains distinct from F273's transfer-time target read. F306-F331 are
  accepted; F332 (purpose-fit class-detail reads for Class Notes and
  Co-Teacher pages, preserving consumed class/schedule fields and per-page
  read behavior) is accepted; F333 (Class Transfer preview assigned-teacher
  profile reuse) is accepted; F334 (purpose-fit Class Details projection) is
  accepted; F335 (one-statement Schedule Editor class-info projection) is
  accepted; F336 (one-column Selected Class Grade read), F337 (reuse the F332
  class-details reader for Class Details validation context), F338 (move the
  import-dialog teacher batch read to v2), F339 (narrow Schedule Testing
  class choices), F340 (narrow Sub Prep roster schedule reads), F341 (narrow
  My Classes assigned-teacher profile batch), F342 (narrow Sub Prep roster-
  output teacher profiles), and F343 (narrow Schedule Import snapshot class-info
  projection) are accepted; F344 (remove compatibility-service availability
  gates in migrated Classes/My Classes) and F345 (app-less roster save
  normalization and validation policy) are accepted; F346 (roster row-transfer
  application workflow) and F347 (Class Details save orchestration) are
  accepted; F348 (Class Transfer typed apply validation/request) is accepted;
  F349 (Testing Classes delete transition baseline parity evidence), F350
  (Testing Classes create/update persistence baseline parity), F351 (My
  Classes assigned-teacher display baseline parity), F352 (Class Import
  teacher-choice display baseline parity), and F353 (Sub Prep roster-output
  semantic baseline parity) are accepted. F354 (Class Transfer package-build
  Application contract), F355 (Co-teacher assignment purpose-fit persistence
  boundary), F356 (remove Speaking Evaluation compatibility-service gates
  around typed roster-name and selected-subtitle reads), F357 (remove Roster
  Print compatibility gates around typed class, teacher, subtitle, and
  extra-column reads), F358 (retire the unused My Classes single-class
  information read contract while retaining the F291 batch path), F359
  (retire the unused Schedule Import compatibility helper while retaining the
  shared review-request type), F360 (Testing Classes cancel/failure page
  parity), F361 (Class Analytics page mapping parity), F362 (Sidebar class
  deletion Application boundary), F363 (Sidebar regular-teacher deletion
  Application boundary), F364 (Clear Testing Layout command transition),
  F365 (Initial Setup Wizard class create/save), F366 (Initial Setup Wizard
  validated teacher create), F367 (Class Transfer persisted apply boundary),
  F368 (Teacher Import UI apply integration using the existing Next use case),
  F369 (Schedule Editor baseline parity), F370 (Class Export picker baseline
  parity), and F371 (Roster Transfer remaining legacy availability gates) are
  accepted in this checkout.
  Batch 6 is complete; Batch 7 (F337-F338), Batch 8 (F339-F344), Batch 9
  (F345-F353), and Batch 10 (F354-F361) are complete. Batch 11 (F362-F371)
  and Batch 12 (F372-F381) are complete; Batch 13 (F382-F390) is complete with
  F382-F384 and F386-F390 accepted, including the independently verified F386
  Staff Directory closed-session navigation parity. F385 is retired as a
  duplicate of F369. F387 Calendar Preferences event-reset parity and F388
  Class Details/Notes/student Evaluation route availability parity are
  accepted; F389 My Info route navigation gate parity and F390 Sub Prep route
  gate parity are also accepted. The seven user-reported MSVC build errors in
  the Teacher Profile Edit persistence target are fixed in commit `0b128601`;
  the target and standard all-target builds passed independently with zero
  errors. F391 Teacher route closed-session gate parity is accepted. Batch 14
  F392 Campus Directory root/section navigation confirmation and destination
  parity is accepted. F393 Document Catalog route confirmation and PDF Viewer
  navigation parity is accepted on pre-slice source `41da57c5`; its focused
  target, CTest, and three direct QtTest cases passed. F394 Classes landing
  open-session confirmation parity and F395 Campus Dashboard typed save
  boundary are accepted with focused build and runtime evidence. Batches 14-15 are complete. F396 Staff Directory open-session dirty-exit parity and F397 MainWindow
Document Catalog retranslation integration are accepted with focused
evidence. F398 FileController same-path workspace-open parity is accepted:
the focused coordinator, FileController, and MainWindow targets built, CTest
passed 3/3, and all three direct QtTest cases passed. Batch 16 is complete.
F399 Close File action parity, F400 Open File action dirty-page gate, and
F401 Recent-workspace menu selection and missing-path pruning are accepted and
committed (F401: `2121ce59`). F402 MainWindow application-exit confirmation
is committed (`9cbe39c9`); F403 Dynamic Teacher Sidebar leaf navigation is
committed (`28e881cd`), completing Batch 17. Batch 18 is active.
F404 Save choice on Open/Close File actions is committed (`b445e102`).
F405 MainWindow Save As and Export action integration is committed
(`07dc5864`). F406 Manage Campuses QAction transition is committed (`28b27998`).
F407 Schedule↔Testing Classes handoff is committed (`f639fbd3`); F408 Dynamic
Teacher Sidebar selection/state during retranslation is committed (`9f92b78d`).
F409 My Workspace Sidebar root producer-to-handler integration is committed
(`ee319df2`); F410 Classes Sidebar root integration is committed (`14723973`).
F411 Sub Prep Sidebar root integration is committed (`3e5dbea4`); F412 Campus
Sidebar root/section producer integration is committed (`76661791`); F413
Initial Setup success navigation is committed (`5d8a941a`), completing Batch 18.
F414 Teacher profile save preserving the selected duplicate Sidebar occurrence
is committed (`6fb39b2b`). F415 Campus Dashboard page-tab-to-Sidebar
synchronization is committed (`f4bc5282`). F416 Document Catalog rendered
  Sidebar leaf through MainWindow/viewer is committed as `a4082f80`. Batch 19
  resumed with F417 Staff Directory rendered leaf through MainWindow
  independently accepted and committed as `d04d9bb0`. F418 Schedule Import
  through MainWindow apply and Sidebar refresh is committed as `ea755736`.
  F419 MainWindow Print/Save Current Page As action capability and enabled
  state is committed as `e2ad222b`. F420 Class/Schedule save signal to Sidebar
  action-state refresh is committed as 4c10f0d1; F421 Useful Links URL handoff
  is committed as 27c064e3. F422 Testing Classes edits refreshing both Schedule views is committed as
  08f44ac6. F423 My Schedule display-mode handoff to Classes is committed as 34966439.
  F424 Sidebar Add Class context-menu handler is committed as 3342963b.
  F425 Upcoming Birthdays QAction is committed as 37588279.
  F426 Class Transfer import QAction is committed as 249d57b1.
  F427 Schedule Save As/PDF output is committed as bbdf10e8.
  F428 canceled database profile Save As is committed as b82bddaa.
  F429 Document Catalog viewer Save As is committed as 19f6024d; see the Phase 2
  progress log for acceptance evidence. F430 Schedule Print QAction is committed as
  aa7fca37. F431 Import Teachers QAction through MainWindow is committed as 2ac08388; see
  the progress log for its matrix and acceptance evidence. F432 Export Classes
  QAction is committed as 4bfe3dc2; see the progress log for its acceptance evidence.
  F433 New Teacher QAction is committed as b60c8025; its characterization and F285
  deferral are recorded in the progress log. F434 Delete Teacher QAction cancellation is committed as 3fb52413; see the progress
  log for its acceptance evidence. F435 Empty-state Open/New Profile button handoff through Banner, PageManager, and
  MainWindow is committed as 99856511; see the progress log for acceptance evidence.
  F436 Invalid UTF-8 document resource references is committed as c19e247f; see the progress log for acceptance evidence.
  F437 Report worker event-post failure with a zero-capacity queue is
  committed as f355a1aa; see the progress log for acceptance evidence.
  F438, “Phase2 - Reject existing IDs in repeat-series creation (F438),” is
  committed as 0c2ceca6; Batch 21 is complete. F439, Delete Teacher QAction
  confirmation success, is committed as a0c50d2d (branch ahead 24). Batch 22 is
  F440, “Phase2 - Cover Export Classes JSON output (F440),” is committed as
  01d1559c (branch ahead 25). F441, Import Classes QAction apply success, is
  committed as 75a559ae (branch ahead 26). F442, Import Teachers QAction apply
  success, is committed as 8cde2826 (branch ahead 27). The requested pause after
  the F442 commit was observed; the user has resumed. F443 is committed as
  1c03b326 (branch ahead 28). F444 is committed as
  8a21da618870ba4308415aaf5927fa1393af6e97 (branch ahead 29). F445 is committed as
  d9180f1c465976dfdd707382a1e615108aed9387 (branch ahead 30). F446 is committed as
  4b34a3b8a15a062377a245607228097fb43ee46b (branch ahead 31). F447 is committed as
  d5b130bd0558146185ebf4cdeab885bde6956fec (branch ahead 32); Batch 23 is complete. F448,
  “Phase2 - Cover New File QAction open-profile success (F448),” is committed as
  f825a388db1897bc42cacf43c488850fa48a9de8 (branch ahead 33); Batch 24 is complete. F449,
  “Phase2 - Cover Exit QAction close-confirmation handoff (F449),” is committed as
  4e42a5a9cbe5c261fb78b2e39a98f81c93ef0372 (branch ahead 34); Batch 25 is complete. F450,
  “Phase2 - Cover Undo QAction focused-editor dispatch (F450),” is committed as
  e39852c8a9cef33c80d684dd0e63e18d19a6acf7 (branch ahead 35); Batch 26 is complete. Its commit
  includes exactly seven approved paths and has a clean commit diff check. F451,
  “Phase2 - Cover Redo QAction focused-editor dispatch (F451),” is committed as
  3137d522796365e81d4c3f99e341aacaf83cc392 (branch ahead 36); Batch 27 is complete. Its commit
  includes exactly six approved paths and has a clean commit diff check. F452,
  “Phase2 - Cover Paste QAction focused-editor dispatch (F452),” is committed as
  249dd38b8b3ee2223292c99ec740f2470e20e5e9 (branch ahead 37); Batch 28 is complete. Its commit
  includes exactly six approved paths and has a clean commit diff check. F453, “Phase2 - Cover Cut QAction focused-editor dispatch (F453),” is committed as
  4e9b3d6d79aff3c43b943aec8e165a6ab16ca8a9 (branch ahead 38); Batch 29 is complete. Its commit
  includes exactly six approved paths and has a clean commit diff check. F454, “Phase2 - Cover Copy QAction focused-editor dispatch (F454),” is committed as
  dd73b6d9c536809f05d38487ecca4bc873b9ea86 (branch ahead 39); Batch 30 is complete. Its commit
  includes exactly six approved paths and has a clean commit diff check. F455, “Phase2 - Cover About QAction modal handoff (F455),” is committed as
  76c67663cda5604353b4ba25e54c97160f6daf90 (branch ahead 40); Batch 31 is complete. Its commit
  includes exactly seven approved paths and has a clean commit diff check. F456, “Phase2 - Cover Check for Updates QAction manual handoff (F456),” is committed as
  d90def93f7947d0f031dc6f37a8a4a491f4d3718 (branch ahead 41); Batch 32 is complete. The commit
  contains exactly seven scoped paths and its cached diff check was clean. F457, “Phase2 - Cover Font Size QAction application handoff (F457),” is committed as
  719efeacb6e5a8533f2dd45f6fb0fdfe152c0714 on Qt-Rewrite, 42 commits ahead of origin; Batch 33 is
  complete. Its commit contains exactly seven scoped paths and the cached diff check was clean. Batch 34 is complete. F458, “Phase2 - Cover Theme QAction application handoff (F458),” is
  committed as 2ec351a805af2064a865c34b76b46f60df80acdc on Qt-Rewrite, 43 commits ahead of origin.
  F459 Document Viewer Background QAction parity is committed as cba31503f0bb5accf68de2032131db984403deff;
  Batch 35 is complete. F460, “Phase2 - Cover Document Viewer Page Spacing QAction parity (F460),” is committed
  as fba46915d76b05aab53de85760a3f857dc4ed2a2 on Qt-Rewrite, 45 commits ahead of origin; Batch 36 is complete.
  Its commit contains exactly seven scoped paths. Post-commit status was clean except for excluded
  latest_session_work.md and %SystemDrive%/. F461, “Phase2 - Cover Sidebar Overflow Tooltips QAction parity (F461),” was
  committed as e590ea773d4b6b1d415d248c9c3f5068094d5d4c on Qt-Rewrite, 46 commits ahead of origin; Batch 37 is
  complete. Its commit contains exactly seven scoped paths. Post-commit status was clean except for excluded pre-existing
  latest_session_work.md and %SystemDrive%/. F462, “Phase2 - Cover Save Mode QAction application handoff (F462),” was committed
  as ad0d4de70aa5d984dd50eb0da13c1e3506e2be70 on Qt-Rewrite, 47 commits ahead of origin; Batch 38 is complete.
  Its commit contains exactly seven scoped paths. Post-commit status was clean except for excluded pre-existing
  latest_session_work.md and %SystemDrive%/. F463, “Phase2 - Cover Automatic Update Preference QAction parity (F463),” was
  committed as e0771d5a3b87f75f6385bff23dd869e24e237242 on Qt-Rewrite, 48 commits ahead of origin; Batch 39 is
  complete. Its commit contains exactly seven scoped paths. Post-commit status was clean except for excluded pre-existing
  latest_session_work.md and %SystemDrive%/. F464, “Phase2 - Cover AI Comment Voice QAction prompt handoff (F464),” was committed as
  27d30bbd8452f0d350e968ef462af83ae340cdd3 on Qt-Rewrite, 49 commits ahead of origin; Batch 40 is complete.
  Its commit contains exactly seven scoped paths. Post-commit status was clean except for excluded pre-existing
  latest_session_work.md and %SystemDrive%/. F465, “Phase2 - Cover AI Comment Provider QAction dialog handoff (F465),” was committed as
  599916bada7a2a59ae041dc80d59cba2187cf3be on Qt-Rewrite, 50 commits ahead of origin; Batch 41 is complete.
  Its commit contains exactly seven scoped paths. Post-commit status was clean except for excluded pre-existing
  latest_session_work.md and %SystemDrive%/. F466, “Phase2 - Cover Custom Website provider QAction modal handoff (F466),” was committed as
  0abbaf9238a25407f3c8a6c88a07adb4e93aa238 on Qt-Rewrite, 51 commits ahead of origin; Batch 42 is complete.
  Its commit contains exactly seven scoped paths and the cached diff check was clean. Post-commit status was clean except for
  excluded pre-existing latest_session_work.md and %SystemDrive%/. F467, “Phase2 - Cover Sidebar Marquee QAction parity (F467),” was committed as
  0384c3768c18009689f918f456ae932c0c1d5a89 on Qt-Rewrite, 52 commits ahead of origin; Batch 43 is complete.
  Its commit contains exactly seven paths and the cached diff check was clean. Post-commit status was clean except for excluded pre-existing
  agent_docs/latest_session_work.md and %SystemDrive%/. F469, “Phase2 - Cover Document Viewer Page Spacing None and Medium QAction parity (F469),” committed as
  4044c80a on Qt-Rewrite. The commit contains the six reviewed test and Phase 2 tracking paths. Batch 45 is complete.
  Post-commit worktree entries are only the excluded pre-existing agent_docs/latest_session_work.md and %SystemDrive%/. F470, “Phase2 - Cover Document Viewer Background White QAction parity (F470),” committed as
  738f40f18baeb468be77e402d6fc0ea69aef8deb on Qt-Rewrite; Batch 46 is complete. F471, “Phase2 - Cover Small and Extra Large Font Size QAction parity (F471),” committed as 0d972b53 on Qt-Rewrite; Batch 47 is complete. F472, “Phase2 - Cover System Default Theme QAction parity (F472),” committed as 88fd2088 on Qt-Rewrite. Batch 48 is complete. F473 committed as 438f541a; Batch 49 is complete. F474 committed as f102e7ad; Batch 50 is complete. F475 committed as 643d9674 with six scoped paths; Batch 51 is complete. F476 committed as 9e58daae with six scoped paths; Batch 52 is complete. F477 committed as 9a4c9e85 with six scoped paths; Batch 53 is complete. F478 committed as d5a0eef5 with seven scoped paths; Batch 54 is complete. F479 committed as 803ff239 with six scoped paths; Batch 55 is complete. F480 committed as 869b7331 with six scoped paths; Batch 56 is complete. F481 committed as ae1da379 with six scoped paths; Batch 57 is complete. F482 committed as 82733b05 with six scoped paths; Batch 58 is complete. F483 committed as 3589209f with six scoped paths; Batch 59 is complete. F484 committed as 4116c83f with six scoped paths; Batch 60 is complete. F485 committed as f8453210 with six scoped paths; Batch 61 is complete. F486 committed as 964da4e3 with six scoped paths; Batch 62 is complete. F487 committed as efa93a42 with six scoped paths; Batch 63 is complete. Batch 64 is active with F488 batch-dialog Include-checkbox reset independently verified, accepted, and ready to commit. Focused evidence is recorded in the progress log.
  F457 acceptance and verification remain in the progress log. F456
  acceptance and focused verification remain in the progress log. F454
  acceptance and focused verification remain in the progress log. F453
  acceptance and focused verification remain in the progress log.
  F452 acceptance remains in the progress log. F451 acceptance remains there. F450 acceptance
  remains recorded there. F449’s acceptance and focused verification remain there; its independent
  Tester rerun and environment notices are preserved.
  F451 acceptance remains in the progress log. F450 acceptance remains there. F449’s acceptance
  and focused verification remain there; its independent Tester rerun and environment notices are
  preserved.
  F448 acceptance remains in the progress log, including the reviewer command limitation. F435
  covers no-database New Profile creation and picker metadata; F444 covers Open File replacement.
  F285 stays deferred. See the
  progress log. Gates 1 and 2 remain Partial,
  with broader feature migration, parity, and 96-class Release memory evidence still open.
  See the
  [Phase 2 progress log](03-Phase-2-Progress-Log.md) for current scope and
  acceptance evidence.
- Phase 1 is complete. Its 2026-09-19 closure update records passing hosted
  baseline jobs for Windows x64 and macOS universal, the Phase 1 Build Quality
  and Dialog policy workflows, and packaged Release workflows. Linux x64 and
  Windows ARM64 also passed as informational jobs; they were not required for
  closure. See the [Phase 1 plan](02-Phase-1-Build-System-and-Repository-Structure.md)
  for the run evidence. Phase 0 is complete; Phase 2 remains in progress.
- Release target: ClassMngr v2 with feature parity, no splash screen, no resource packs, and Windows startup memory below 250 MiB

### Phase status

| Phase | File | Status | Default route | Depends on |
|---|---|---|---|---|
| 0 | 01-Phase-0-Product-Contract-and-Baseline.md | Complete | Heavy | None |
| 1 | 02-Phase-1-Build-System-and-Repository-Structure.md | Complete | Heavy | 0 |
| 2 | 03-Phase-2-Domain-Model-and-Application-Contracts.md | In progress | Heavy | 1 |
| 3 | 04-Phase-3-Persistence-Rewrite.md | Not started | Heavy | 1, 2 |
| 4 | 05-Phase-4-Resource-Loader-and-Packaging.md | Not started | Heavy | 1 |
| 5 | 06-Phase-5-Startup-and-Bootstrap-Rewrite.md | Not started | Heavy | 2, 3, 4 |
| 6 | 07-Phase-6-Shared-UI-Rewrite.md | Not started | Heavy | 5 |
| 7 | 08-Phase-7-Feature-Migration.md | Not started | Heavy | 2–6 |
| 8 | 09-Phase-8-Platform-and-Output-Adapters.md | Not started | Heavy | 2, 3, 7 |
| 9 | 10-Phase-9-Windows-Memory-Hardening.md | Not started | Heavy | 4–8 |
| 10 | 11-Phase-10-Visual-Behavioral-and-Cross-Platform-Parity.md | Not started | Heavy | 7–9 |
| 11 | 12-Phase-11-Test-Restructuring-and-Release-Gates.md | Not started | Heavy | 0–10 |
| 12 | 13-Phase-12-Beta-Cutover-and-Legacy-Removal.md | Not started | Heavy | 10, 11 |
| 13 | 14-Phase-13-Post-Release-Maintenance.md | Not started | Heavy | 12 |

Statuses are intentionally conservative. A phase is not In progress until its work has started in the repository, and it is not Complete until its exit gate has passed.

## Phase notes, slice batches, and progress logs

- Keep each phase plan's `Current note` limited to the latest information that
  is relevant to the current or next slice. Move stale progress details to that
  phase's progress log; keep durable requirements and decisions in their
  appropriate plan sections.
- Keep only the most recent slice commit in the phase plan's
  `Latest Progress Update` section. When a newer progress update is written,
  move the previous update into that phase's progress log before replacing it.
- Discover upcoming slices in ordered batches of up to ten (or all remaining
  slices when fewer than ten remain) and record each batch in its phase plan's
  `Slice discovery batches` section. Work through the recorded slices in order.
  Begin discovering and recording the next batch when starting work on the
  second-last slice in the current batch. If a discovery pass finds fewer than
  ten slices, add the standalone line `No other slices were found.` after that
  batch.
- Use a sibling file named from the phase plan and ending in `-Progress-Log.md`
  when that phase has one (for example,
  `03-Phase-2-Progress-Log.md`); otherwise use its `Progress log` section.
  Preserve historical entries in date order, including verified-slice reports
  and dated exit-gate snapshots. Keep requirements, decisions, and current
  status in the phase plan.

## Build and test verification

- Use `build/windows-x64-debug` as the standard local Windows x64 Debug
  build folder for slice work. Reuse this same configured folder across
  slices and phases; do not create slice-, task-, or reviewer-specific build
  folders. Build the affected targets and their dependencies, and let the
  build system recompile changed or out-of-date inputs.
- If a fresh build is needed, empty the applicable standard build folder
  before configuring and building in it. For Windows x64 Debug, keep using
  `build/windows-x64-debug`; do not create a new folder for the fresh build.
  Apply the same rule when changing the build system, toolchain, or
  dependency configuration; when stale artifacts could explain a result;
  or when a phase gate explicitly requires clean-checkout evidence.
- A gate requiring another platform or configuration must use that CMake
  preset's standard `build/<preset-name>` folder and reuse it for that
  preset, rather than creating a slice-specific folder.
- Keep any phase-specific full builds, test suites, platform matrices, and
  packaging checks required by an exit gate. Record whether verification
  used an incremental or fresh build, which targets were built, and which
  tests ran. Describe focused results as focused; do not report them as a
  full-suite pass.

## Cross-cutting memory remediation

The audit of large-data and widget-heavy paths is tracked in the
[Qt Rewrite Memory Hotspot Remediation Plan](memory-hotspot-remediation-plan.md).
It is an execution plan across the numbered phases, not a new phase. Phase 0
owns the measurements, Phases 2–8 own the data and lifecycle fixes, Phase 9
owns the packaged Release memory gate, and Phases 11–13 own permanent
regression coverage and removal of temporary paths.

## Commit message convention

Use the standardized prefix `Phase# - ` for commits related to this rewrite, replacing `#` with the primary phase number. For example: `Phase0 - Add the initial baseline evidence`. For changes spanning multiple phases, use the phase that owns the primary deliverable.

## Session handoff notes

Update `agent_docs/latest_session_work.md` only when a handoff is expressly requested by the user.

## Phase 0 update - 2026-09-15

- What changed: started the Phase 0 evidence set at commit `75755460`; added
  static source archaeology, feature preservation, file compatibility, resource
  ownership, baseline, and risk documents under `docs/qt-rewrite/`.
- What remains: capture visual references, create representative file fixtures,
  run a fresh packaged Release baseline on every target platform, and complete
  startup/resource tracing.
- Evidence: `ctest --test-dir build/windows-x64-debug -N` currently enumerates
  66 tests, but that build tree contains stale cache options and is not treated
  as authoritative. A clean Phase 0 build directory is being established.
- Risk: historical baseline artifacts describe an older source snapshot and
  must not be used as the rewrite's acceptance baseline.

## Product contract update - 2026-09-16

- The document catalog may load startup metadata, but PDFs displayed through
  QtPdf are loaded only when the user requests them.
- The active viewer session owns the loaded document and must close/release it
  when the document is replaced, the viewer is closed, or the page is left or
  released. Generated and print-output PDFs remain operation-scoped.
- Phase 0 records the startup-negative, on-demand-open, and release-memory
  evidence still required; later phases now carry the same lifecycle contract.

## Phase 1 update - 2026-09-18

- Phase 0 is complete at commit `f8bb5954`: the combined Windows x64 and macOS
  universal 24-route exit gate passed, and the user confirmed the visual review.
- Phase 1 slices 1.1-1.3 establish the parallel Qt Core-only `ClassMngrNext`
  bootstrap, source-free layer/feature boundaries, and per-target dependencies
  measured from Ninja data. The legacy runtime retains its full Qt module set.
- A fresh Ninja/MSVC Debug configuration built both executables in 351 steps;
  `ClassMngrNextLaunch` passed 1/1. The `ClassMngrDomain` compile command has
  only QtCore/QtGui include paths, with no Widgets, Sql, or Network paths.
- `ninja -t commands ClassMngr.exe` confirms the full legacy Qt link set,
  including `Qt6::QuickControls2`. `ClassMngrSharedPolicyTests` built after
  importing VS DevCmd, and its targeted CTest passed 1/1; `ClassMngrNext.exe`
  launched with exit code 0.
- Slice 1.4 completed: explicit source manifests and configure-time ownership
  checks cover production, executable, QML, and test sources. A clean Windows
  Ninja/MSVC Debug configure reported 653 handwritten files; the build passed
  for `ClassMngr`, `ClassMngrNext`, and five affected test targets. Six
  targeted CTests passed.
- Slice 1.5 adds the compile database, v2-scoped format/tidy CI, configure-time
  gates, module/resource/package reports and checks, packaged Release workflow
  integration, and a startup/memory-labeled CTest entry point. Local resource
  checks passed for six generated RCCs and seven runtime IDs; the staged
  package report passed. Cross-platform CI and local clang tools were not run.
- Slice 1.6 adds PR-triggered Debug validation for Windows x64, Windows ARM64,
  macOS universal, and Linux. ARM64 cross-builds on x64 without execution;
  existing platform packaging workflows remain the Packaged Release paths.
  Independent static review passed. On a clean snapshot at `6f2f5fb0`, local
  Windows x64 Debug configure/build and CTest passed 66/66, including the v2
  launch and startup performance tests. The local VS 2026/MSVC 19.51 + Qt 6.12
  Windows x64 Release packaged-installer path also succeeded, but is
  supplemental because the exact VS2022 configure failed: no VS2022 instance
  is installed.
- Hosted workflow results have now been queried on source commit `57f5dff6`:
  the official Windows x64 Debug job passed 66/66, and the macOS Debug job
  failed after the hosted runner lost communication with GitHub. The user saw
  `ClassMngrUpdaterTests` running, but no log or test report confirms it as the
  cause. The Windows and macOS Packaged Release runs passed. The local Windows
  x64 Debug pass of 66/66 on `4dbe3ca7` using VS 2026/MSVC 19.51 remains
  independent passing evidence. Phase 1 Build Quality still needs a hosted
  run. Linux and Windows ARM64 are unofficial, deferred builds; their tests
  and native launch are not Phase 1 blockers. Phase 1 remains open for the
  official Windows/macOS checks and quality workflow. No later phase is
  complete.

## Important context

### Local Windows x64 Debug build timing (2026-09-28)

- Configuring and generating the `windows-x64-debug` preset in the existing
  `build/windows-x64-debug` tree took 4m25s (192.8s configuring and 72.7s
  generating).
- Building `ClassMngr` and `ClassMngrNext` with
  `cmake --build --preset windows-x64-debug --target ClassMngr ClassMngrNext`
  succeeded in 12m17.5s. This is a local reference from a partially built
  tree, not a clean-build guarantee. The toolchain was Visual Studio 18 / MSBuild
  18.10.1 with Qt 6.12.0.
- The default preset also builds test executables. That full build did not
  complete: `ClassMngrNextPlatformApplicationServicesWorkspacePortTests`
  failed to compile because it used the forward-declared `SettingsService`.
  CTest was not run.
- During that full build, `cmake/production_sources.cmake` changed and caused
  another configure/generate pass lasting 10m30s (449.0s configuring and
  181.2s generating). Treat that extra pass as a source-change interruption,
  not normal build overhead.

This plan is based on the current Qt-Rewrite source tree. Existing plan documents in the branch are intentionally ignored and do not define scope or architecture.

The current repository contains a large Qt desktop application with:

- A monolithic startup path in src/main.cpp.
- A MainWindow that composes services, pages, actions, menus, and controllers.
- A lazy but resource-pack-coupled PageManager.
- ApplicationServices and feature services that still fall back to a broad DataService compatibility facade.
- A database layer with repositories and a large compatibility surface.
- ResourcePackManager, ResourcePackLease, ResourcePackUpdateService, ResourcePaths, external RCC packs, and resource-pack deployment settings.
- A splash screen and splash asset on the startup path.
- Large table-oriented UI areas that create many Qt objects.
- Embedded or bundled documents, fonts, templates, maps, translations, styles, icons, and report assets.
- Qt Widgets plus a Qt Quick calendar component.

The worktree already contains user-owned changes. Do not overwrite or revert them while implementing this plan. In particular, preserve unrelated CMake changes and existing deletions.

## Product constraints

The rewrite must:

1. Preserve all current user-facing features.
2. Discard the splash screen rather than replacing it with another full-screen startup screen.
3. Add a new typed resource loader for startup and on-demand resources.
4. Preserve the current appearance, layout, typography, themes, language support, navigation, shortcuts, dialogs, and output formats.
5. Reduce normal Windows memory use below 250 MiB.
6. Remove resource packs and their separate automatic update path.
7. Keep application updates, with resources delivered as part of the application release.
8. Preserve .tps files, legacy .db import behavior, class-transfer behavior, exports, backups, and generated outputs.

## What the heavy route means

The default route for every phase is the heavy route:

- Build a parallel ClassMngr v2 application instead of making only incremental edits to the old composition root.
- Create explicit domain, application, persistence, resource, platform, UI, and feature boundaries.
- Migrate complete vertical slices from storage through UI and output.
- Keep the old application as a parity oracle until cutover.
- Delete compatibility code after migration instead of leaving permanent fallback paths.
- Treat memory, visual parity, file compatibility, and cross-platform behavior as release gates.
- Keep Qt as the presentation framework while removing unnecessary coupling from the core.

### Sub-agent check-in cadence

Each sub-agent may be checked in with only once every 10 minutes. Batch questions
and status requests so this cadence is maintained.

The heavy route is not permission to remove features, change user workflows, or redesign the application. The developer-only Memory Usage Monitor and its in-app diagnostics are an explicit scope exception. Otherwise, this is a commitment to replace the underlying ownership and lifecycle model thoroughly enough to meet the memory target.

### Slice-by-slice reminder

Every slice of every phase must use the heavy route. Treat a slice as a bounded
vertical unit of work, not a one-layer patch: define its target v2 boundary,
move and verify its end-to-end behavior, keep the legacy path only as a parity
oracle or an explicitly temporary bridge, and remove that bridge when the
slice is accepted. Do not switch an individual slice to a lightweight or
incremental route without recording an explicit product or architecture
decision in this plan.

Reuse the standard build folder across slices. Do not create a unique subfolder
under `build/` for an individual slice. If a fresh build is needed, empty the
applicable standard build folder before configuring and building in it.

## Target architecture

    ClassMngrNext
    ├── AppShell
    │   ├── NavigationModel
    │   ├── PageHost
    │   ├── CommandBus
    │   ├── DialogService
    │   └── NotificationService
    ├── ApplicationRuntime
    │   ├── Use cases
    │   ├── Application state
    │   ├── Preferences
    │   └── Application update coordinator
    ├── Persistence
    │   ├── WorkspaceStore
    │   ├── Schema migrations
    │   └── Repositories
    ├── Domain
    │   ├── Models
    │   ├── Value objects
    │   ├── Rules
    │   └── Validation
    ├── ResourceSystem
    │   ├── ResourceCatalog
    │   ├── ResourceLoader
    │   ├── ResourceCache
    │   └── ResourceDiagnostics
    ├── Qt presentation adapters
    └── Platform adapters

The dependency direction is:

    UI → Application → Domain
    UI → Application → Persistence
    UI → ResourceSystem
    Platform → Application interfaces

Domain code must not depend on Qt Widgets. UI code must not issue SQL. Feature pages must not know where installed resources are stored. Application code must not depend on widget ownership or visibility.

Prompt inspection, dismissal, default-action activation, and screenshot capture
used by legacy startup/performance workflows belong to a Qt presentation or
test adapter. They must not be added to the permanent domain or application
prompt contract. Any compatibility driver for the legacy executable must be
explicitly temporary and removed when the corresponding v2 startup and shared
UI slices are accepted.

## Shared status rules

Each phase file contains its own Status section. Update it whenever work starts, a milestone is reached, a blocker appears, or the exit gate passes.

Use these statuses:

- Not started: no implementation work has begun.
- In progress: implementation or verification is actively underway.
- Blocked: work cannot continue because a concrete external dependency or unresolved decision prevents it.
- Ready for review: implementation is complete and evidence is being checked.
- Complete: all deliverables and exit criteria have passed.
- Deferred: explicitly moved out of the release scope by a documented decision.

Every status update should include:

- What changed.
- What remains.
- Evidence or test command.
- Any new risk or blocker.
- Date of the update.

## Resource policy

Resources are installed with the application in a deterministic read-only resource tree. There are no independently mounted RCC packs, no runtime resource-pack directory, no resource manifest download, and no resource-pack update request.

The new loader classifies resources as:

- Core: required to create the normal shell.
- Startup: required for the initial page.
- Feature: loaded when a feature is entered.
- Operation: loaded only for a print, export, import, or report operation.

Large documents, maps, report artwork, templates, optional fonts, and PDF content must not be resident merely because the application opened.

### Document viewer lifecycle

The document catalog is startup metadata, not document content. Startup may
load catalog schema, localized names, and validated asset references, but a
PDF displayed through QtPdf is loaded only after the user requests it. The
active viewer session owns that loaded document; closing the viewer, replacing
the document, leaving the viewer, or releasing the page must close the QtPdf
document and release its resource. Reopening may load it again. Generated and
print-output PDFs remain separate operation-scoped resources.

## Memory policy

The primary hard gate is Windows working set for a packaged Release build. Record private bytes, commit, and peak working set as secondary values.

Measure:

- Empty workspace startup.
- Representative workspace startup.
- First page render.
- Five minutes idle.
- Navigation through all normal pages.
- Large schedule, roster, campus, document, and speaking-evaluation workflows.
- Memory after leaving each large feature.
- Repeated open/close and language/theme changes.

No feature may pass solely by disabling functionality or reducing visual fidelity. The solution must improve ownership, loading, caching, and widget allocation.

## Feature migration order

The recommended dependency order is:

1. Setup and workspace/file flows.
2. Teachers and staff.
3. Classes.
4. Schedule and imports.
5. Calendar.
6. Rosters.
7. Speaking evaluations.
8. Campus and documents.
9. Substitute preparation and output.

Each feature is complete only when its domain behavior, persistence, application use cases, UI, resources, output, localization, and tests have moved.

## Non-negotiable review questions

Before accepting any phase, ask:

- Does this preserve every required feature?
- Does this preserve the current appearance?
- Does this introduce a new global owner or unbounded cache?
- Does this load a large resource earlier than necessary?
- Does this create one Qt object per data cell?
- Does this leave a compatibility fallback that should be removed later?
- Is the Windows Release memory impact measured?
- Can the behavior be tested without constructing the entire main window?
- Can the feature be released and recreated without losing persistent state?
