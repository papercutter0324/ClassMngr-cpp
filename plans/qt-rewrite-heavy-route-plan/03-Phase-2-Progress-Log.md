# Phase 2 Progress Log

This log retains useful implementation, verification, decision, and gate
evidence. Routine slice chronology and closed-batch history are omitted; the
[current slice-batch tracker](Current-Slice-Batches.md) contains live status.

## Phase 2 exit audit — 2026-10-22

Phase 2 is Complete. The main agent accepted Gate 1 and Gate 2 against the
criteria in the [Phase 2 plan](03-Phase-2-Domain-Model-and-Application-Contracts.md).
F629's source-bound Windows x64 requalification remains separate.

### Gate 1 — app-less contracts and dependency boundary

Domain/Application contracts and test surfaces are app-less. Explicit Workspace
and Domain contract targets passed. The formal audit found app-less test
surfaces across §§2.2–2.6, passing explicit Workspace acceptance, and no
consumer-backed production gap. Section 2.1 categories are coverage goals, not
a requirement for standalone aggregates. The inspected active paths had no
DataService, MainWindow, PageManager, or widget dependency. A source scan of 169
files found no `MainWindow`, `QApplication`, or `QCoreApplication` references.
Static test entry points were 85 `QTEST_APPLESS_MAIN`, 9
`QTEST_GUILESS_MAIN`, and 75 custom main; none mentions those application or
window classes.

The incremental audit mapped 169 Domain/Application CTest targets to source
and built 168/169 targets. One pre-existing Schedule Import review-readiness
target failed compilation because its test uses stale raw-string initialization
after the explicit typed-key API change. Of 170 selected registrations, 167
passed and 3 failed. Those three tests expect teacher ID `2147483648` to be
invalid, contrary to current F799/F801/F802 policy accepting positive signed-64
teacher IDs; class IDs remain positive `int` for class-summary input. The
Investigator traced all four outcomes to stale test/fixture expectations. No
changes were made to those stale tests. The full suite was not run. This audit
was neither clean nor exhaustive; these findings do not invalidate the literal
app-less-testability criterion or separately passing acceptance targets.

### Gate 2 — baseline parity

The following bounded evidence passed; it is not exhaustive proof of parity:

- Teacher Import `sectioned_review.xlsx` matched validation, review choices,
  plan, apply counts, persisted state, and warnings. Fixture SHA-256:
  `5304a42526d3fa5c25a4201ad0563bd9ea8d86fc33a87e52e0b366d4ad8de7d`; normalized
  JSON SHA-256: `e39d00723736e433208011ca7ea83d4460bde53c93f8a078772613886732f426`.
  Focused tests passed 30 cases; one optional external-workbook case was
  skipped. The fixture does not cover ambiguous-match conflicts or every
  failure/provenance case.
- The four-case roster-name planner comparison matched trimming, repeat/order,
  U+001F collision, target blanks/capacity, and empty input (fixture SHA-256
  `c2f4e951447961124df2728363e736924658a69bbe7c9408535813c41ea5219a`). Its
  baseline loop was transcribed from the page implementation using baseline
  `StudentNameUtils`; page-level behavior was not compared.
- Teacher Import Native English ambiguity after a prior write passed a bounded
  custom-harness differential: pinned baseline
  `48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` versus F1316
  `11ee8d808d7ce87e44eafa92fbb3bd907760ae3a`. The exact error and complete
  `teachers`, `native_english_teachers`, `gs_team`, and `app_settings` snapshots
  were unchanged and byte-identical. This was not CTest.
- Speaking Evaluation page-level roster import passed an isolated differential
  against pre-F1206 baseline
  `c2d1fce26ff50751bc1473d43efcc0c8fa691806` at F1316. The full row matrix,
  dirty-state sequence `false→true→true`, and both prompt records matched. One
  selector ran with setup/cleanup; snapshot-read failure was not injected.

### Focused implementation verification

F1316 (`11ee8d808d7ce87e44eafa92fbb3bd907760ae3a`) adds Qt-free Sub Prep
generation planning and was independently accepted. In
`build/macos-clang-debug`, an incremental build of `ClassMngr`,
`ClassMngrNextApplicationSubPrepPackageRequestValidationTests`,
`ClassMngrSubPrepPageTests`, and `ClassMngrSubPrepPackageServiceTests` exited 0
(Ninja reported no work). The validation CTest passed 1/1; two selected page
checks passed (4 QtTests including setup/cleanup), and three selected service
checks passed (5 QtTests including setup/cleanup). The
`largePackageGeneratesOutputReferenceWhenConfigured` selector still fails the
macOS page-1 golden-text assertion. A controlled 19-page package/direct-render
comparison matched extracted text, dimensions, and 150-DPI pixels; the Tester
found the mismatch unrelated to F1316. The golden reference's Windows
provenance is unverified. No full suite or Windows build ran.

F1317 (`28f77666`) implements the user's F1103 decision: for a canonical class
ID, a validation-context query error or mismatched returned class ID produces
a structured `ValidationContext` failure before conflict lookup or save;
malformed IDs retain their previous validation output. The independent Tester
accepted `ClassMngrNextApplicationClassDetailsSaveUseCaseTests`: incremental
build, Ninja reported no work, CTest passed 1/1, and 27 QtTest cases passed. No
page test or full suite ran.

#### Final focused re-verification at source tip `45ac83c6`

An independent Tester re-verified current source on macOS arm64 with Qt 6.12.0;
later changes were documentation-only. The incremental build succeeded for
`ClassMngr`, the three app-less gate targets, F1316 validation/page targets,
F1317, and the Sub Prep package-service target. App-less gate CTest passed
3/3; F1316 validation CTest passed 1/1. F1316 page selectors
`pageGenerationKeepsVacationDateAndCheckedWeekdayScopesSeparate` and
`pageGenerationRejectsInvalidPlanBeforeSourceReadsAndEffects` passed, as did
Sub Prep package-service selector
`generationConsumesTypedPlanForRosterQueryAndFolderOutput`. F1317 CTest passed
1/1 with 27 QtTest cases. This was incremental macOS verification, not a clean
build or full suite. The known unrelated
`largePackageGeneratesOutputReferenceWhenConfigured` macOS golden-text failure
was not rerun.

## Deferred-candidate closeout — 2026-10-22

After Phase 2 completion, close F614, F618, F626, F707, and F709 as inactive
implementation candidates. Their current evidence does not justify additional
production changes:

- F614 measured a 22,662-byte source/package payload, and the typed source is
  released before rendering. No material ownership refactor is warranted for
  the measured case. Reopen only if a larger representative fixture or
  owner-specific memory evidence shows a meaningful cost.
- F618 observed overlapping package, JSON-tree, and encoded-buffer lifetimes,
  but no export-specific peak measurement established a material cost. Do not
  change serialization or `QSaveFile` based on overlap alone. Reopen if a
  matched packaged Release export measurement shows a meaningful peak.
- F626's F621/F622 release and refresh coverage plus F1014 re-entry coverage
  show no current lifecycle gap. No owner-specific packaged Release retained-
  byte attribution or safe PageManager eviction/recreation contract supports
  a memory refactor. Reopen only if measured retained bytes establish a gate
  problem and a safe eviction contract is defined.
- F707's helper chain has no production callsite in the current source tree,
  so it has no active behavior to repair. Reopen if a live caller appears.
- F709's missing injection seam is superseded by the Calendar cache query seam.
  `CalendarEventCacheTests::injectedInvalidTimesAreFilteredByCacheAndModel`
  already injects malformed and padded time strings and verifies filtering
  through the cache and model. This closes that boundary-coverage candidate;
  it does not claim a separate end-to-end Calendar Page parity comparison.

F579 and F598 remain deferred pending a Windows process with an elevated
Administrator token for their matched heap captures. F285 and its dependent
Sidebar wide-ID transition, Batch 197 teacher-reassignment policy, the Class
Analytics allocation policy, and the Dense Speaking Evaluation row policy
remain open pending the product clarifications recorded in the Phase 2 plan.
F629 remains separate from Phase 2. This was a documentation-only closeout;
no source changes or tests were run.

## Durable parity and product decisions

- Class Details save fails closed on validation-context read failure or class-ID
  mismatch. This user-directed behavior is an explicit exception to pinned
  legacy behavior, which could save default teacher assignment, notes, and
  activities after a context-read failure.
- Preserve the user-approved Teacher Import rollback when its SQL cursor reports
  `lastError()`, and the Speaking Evaluation fail-closed Import Names guard when
  its evaluation snapshot cannot be read.
- Class Transfer identity-free packages remain an explicit parity exception;
  exact parity is not claimed for that input.
- F1314 (`5738d486`) preserves opening schema-v6 workspaces and rejects existing
  schemas 0–5 without mutation or migration. Independent focused checks covered
  11 affected targets and 90 QtTest cases; no full suite ran.
- F1315 (`91ba6c99`) removed Calendar Import's total 4,096-event save cap to
  match legacy while retaining per-event validation, duplicate filtering/order,
  failure precedence, projection cap, and repeat-series cap. Independent
  focused tests passed 35 cases, including 4,097-event validation and ordered
  persistence; no greater-than-4,097 stress test or full suite ran.
- No user-initiated backup/restore behavior was found. Do not add that workflow
  without baseline behavior; existing profile-replacement rollback and
  initial-setup cancel restoration cover the evidenced recovery cases.
