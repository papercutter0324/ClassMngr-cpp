# Current Slice Batches

## Active batch — Phase 10 supplemental Windows x64 continuation (Candidates 29–33)

Historical Phase 10 findings and discovery records belong in the [progress log](11-Phase-10-Progress-Log.md).

| Slice | Current status | Evidence / next work |
| --- | --- | --- |
| Candidate 29 — Testing Classes | Complete. | [execution report](../../artifacts/phase10/p10-win-c29-testing-classes-20261126/execution-report.md), [visual review](../../artifacts/phase10/p10-win-c29-testing-classes-20261126/visual-review.md) |
| Candidate 30 — Schedule | Complete. | [discovery](../../artifacts/phase10/p10-win-c30-schedule-discovery-20261126/discovery-report.md), [execution](../../artifacts/phase10/p10-win-c30-schedule-20261126/execution-report.md) |
| Candidate 31 — Schedule Import | Complete by current-source evidence reuse; focused tests and paired capture CTests passed. | [discovery and evidence](../../artifacts/phase10/p10-win-c31-schedule-import-discovery-20261126/discovery-report.md) |
| Candidate 32 — Schedule output and printing | Complete. Five focused CTests passed; fresh capture produced 12 dialog images and a PDF preview. | [discovery](../../artifacts/phase10/p10-win-c32-schedule-output-discovery-20261126/discovery-report.md), [execution report](../../artifacts/phase10/p10-win-c32-schedule-output-20261126/execution-report.md), [visual review](../../artifacts/phase10/p10-win-c32-schedule-output-20261126/visual-review.md) |
| Candidate 33 — Calendar events, preferences, and month grid | In progress. Initial configure/build and captures used the nonstandard `build/windows-x64-debug-p10-c33` folder, so standard-folder requalification remains open. The standard build attempt is blocked at `ZERO_CHECK` by MSBuild FileTracker `E_ACCESSDENIED`; the later elevated/serial attempt failed with WinError 5 while replacing the ignored generated `Resources` directory, before the Next page target linked. CTest #261 previously passed independently at HEAD `60af92d1`, but no CTests ran in the latest attempts. The shared 59-file capture tree is unchanged. Capture acceptance remains pending because the fixture does not wait for `isMonthLoaded(2026,10)` or hidden `visibleMonthEventsLoading` before capture. | [discovery](../../artifacts/phase10/p10-win-c33-calendar-discovery-20261126/discovery-report.md), [execution artifacts](../../artifacts/phase10/p10-win-c33-calendar-20261126/) |

## Next batch — Phase 10 supplemental Windows x64 (18 slices)

| Slice | Status and scope / next action |
| --- | --- |
| Candidate 34 — Calendar import | Queued — Run focused import-use-case and campus-code query tests, then capture Next Calendar import states. Reuse C33 visuals only if the manifest proves matching current inputs. ([Phase 7 scope](08-Phase-7-Progress-Log.md#L950)) |
| Candidate 35 — Roster editor/student model | Queued — Run `ClassMngrNextRosterEditorTests` and capture English/Korean light/dark states. ([Phase 7 scope](08-Phase-7-Progress-Log.md#L992)) |
| Candidate 36 — Roster score import/student transfer | Queued — Run editor import/transfer and WorkspaceStore action tests; capture the import/transfer flow. ([Phase 7 scope](08-Phase-7-Progress-Log.md#L1010)) |
| Candidate 37 — Roster templates/printing | Queued — Confirm target definitions, run Next roster output/template-print tests, and cover Testing Classes and Extra Info. ([Phase 7 scope](08-Phase-7-Progress-Log.md#L1034)) |
| Candidate 38 — Speaking Evaluation core | Queued — Run Next page tests and capture English/Korean light/dark states. ([Phase 7 scope](08-Phase-7-Progress-Log.md#L1067)) |
| Candidate 39 — AI-assisted comments/batch workflow | Queued — Verify eligibility, prompt, selection, review, and apply cases. ([Phase 7 scope](08-Phase-7-Progress-Log.md#L1107)) |
| Candidate 40 — Reports/PDF/PowerPoint/export jobs | Queued — Cover report widgets, batch reports, PDF output without a widget, and UI/export states; use injected or fake Office automation only. ([Phase 7 scope](08-Phase-7-Progress-Log.md#L1133)) |
| Candidate 43 — Document viewer lifecycle/actions | Queued — Run Next viewer tests and Legacy/Next captures for PDF open/loading/error, close/release, and reopen behavior. ([Phase 7 scope](08-Phase-7-Progress-Log.md#L1354)) |
| Candidate 44 — Sub Prep class information/output | Queued — Build current class-model/page and print targets, then capture the page and output. ([Phase 7 scope](08-Phase-7-Progress-Log.md#L1412)) |
| Candidate 45 — App-wide Preferences | Queued — Verify model/snapshot adapters and the production Preferences route in Next bootstrap. ([Phase 7 scope](08-Phase-7-Progress-Log.md#L1456)) |
| Candidate 46 — Admin/permission gates | Queued — Run operation-level Admin navigation and Campus Dashboard save/identity permission tests. ([Phase 7 scope](08-Phase-7-Progress-Log.md#L1496)) |
| Candidate 47 — User-initiated updates | Queued — Verify coordinator, shared presentation, and manual Help routes; produce the 36-state capture matrix. ([Phase 7 scope](08-Phase-7-Progress-Log.md#L1526)) |
| Candidate 48 — Useful Links external navigation | Queued — Run focused selection and locale-label tests, then produce four captures. ([Phase 7 scope](08-Phase-7-Progress-Log.md#L1565)) |
| Candidate 49 — Workspace Calendar | Queued — Check current inputs, run My Workspace page/calendar tests, and refresh captures if needed. ([Phase 7 scope](08-Phase-7-Progress-Log.md#L1601)) |
| Candidate 50 — Classes parity/transfer boundary | Queued — Run navigator and transfer workflow tests, capture full-window states, and refresh captures when current inputs require it. ([Phase 7 scope](08-Phase-7-Progress-Log.md#L1630)) |
| Candidate 51 — Schedule import/output follow-up | Queued — Run the current normal-schedule integration target, then cover Schedule page/output and five-block comparisons. ([Phase 7 scope](08-Phase-7-Progress-Log.md#L1669)) |
| Candidate 52 — Document Catalog live-locale behavior | Queued — Verify the bootstrap live-locale switch and run catalog/viewer tests and captures. ([Phase 7 scope](08-Phase-7-Progress-Log.md#L1701)) |
| Candidate 53 — Sub Prep PDF text mapping/output parity | Queued — Run PDF finalizer/package tests and capture current C53 SQL parity output. ([Phase 7 scope](08-Phase-7-Progress-Log.md#L1726)) |
