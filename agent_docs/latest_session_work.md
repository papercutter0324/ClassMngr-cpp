# Latest Session Work

Qt rewrite Phase 0 is still in progress on branch `Qt-Rewrite`.

## Detailed Current State

- User instruction: use the Heavy route for every slice; commit each slice and
  immediately continue unless the user asks to pause. The current final slice
  is being committed after the 13:00 Asia/Seoul cutoff; stop immediately after
  that commit and do not begin another slice.
- The `<250 MiB` RAM value is the target for the completed rewrite, not a
  Phase 0 acceptance gate for the legacy widget graph. Phase 0 retains current
  packaged Windows x64 Release measurements as baseline/trend evidence. Later
  phases must avoid regressions and should lower the affected heavy-route
  working set. The temporary `<512 MiB` ceiling is diagnostic only.
- Phase 0 is not complete and Phase 1 must not begin yet. Remaining gaps
  include Sub Prep loading and other feature error states, remaining
  generated-output references, cross-platform Release evidence, and the
  eventual v2 memory/ownership remediation. Populated Classes visual states are
  now retained for both language/theme axes.

## Session Changes

- `b5c1a8a1` — `Phase 0: retain PowerPoint renderer reference`
  - Added a real Speaking Evaluation export-dialog PowerPoint renderer
    selection capture and checkpoint to the packaged Heavy route.
- `e3fcde2d` — `Phase 0: retain Sub Prep validation reference`
  - Retained the real Sub Prep generation-dialog validation-error state with
    its disabled OK action.
- `17b3336b` — `Phase 0: restore Sub Prep PDF evidence`
  - Restored/tracked the two generated PDFs that were temporarily staged as
    deletions because the app-created output folder had a sandbox-only ACL.
  - The exact generated evidence folder was granted read access to the normal
    Git identity; no source behavior changed.
- `9ba4e462` — `Phase 0: retain Sub Prep editing state`
  - Retained the packaged large-route Sub Prep mixed editing/read-only state
    and asserted four editable text editors plus six read-only line edits.
- `630f61b` — `Phase 0: retain Classes visual states`
  - Added opt-in in-process packaged Classes visual captures for populated
    entry, selected class 96, and selected class 1 after re-entry. The route
    retains all four English/Korean light/dark variants and reports the capture
    checkpoints in its metrics.
- `71e44c8` — `Phase 0: retain Schedule Import loading state`
  - Retained the real packaged large-workbook Schedule Import loading state for
    both cancel and apply boundary routes. The evidence records the
    `Loading workbook...` status, indeterminate progress, disabled source/load
    controls, and a non-empty screenshot. The focused routes and the full
    startup-performance suite passed; measured peaks were `418,488,320` bytes
    working set for cancel and `361,316,352` bytes for apply.
- `f0fb4b3` — `Phase 0: retain Calendar Import loading state`
  - Retained the real packaged Calendar Import Preferences loading state. The
  Calendar tab is scrolled to the Import section only for the opt-in capture,
  then restored; the evidence records `Importing events...`, the disabled
  import button, and a non-empty screenshot. The focused route and full
  startup-performance suite passed with a `416,694,272`-byte working-set and
  `457,310,208`-byte private-usage peak.
- Final current-session slice — `Phase 0: retain Schedule Import conflict
  warning` (commit created after the cutoff): the packaged large Schedule
  Import cancel route now retains the real conflict-warning modal, including
  visible warning text, disabled Import, and a validated screenshot. The
  cancel fixture intentionally overlaps projected meetings; the separate
  apply fixture remains conflict-free and commits normally. The current
  source/test/evidence/docs changes are included in the final post-cutoff
  commit for this session.

## Verification

- The Classes visual-state slice rebuilt the Release application and Debug
  startup test target successfully inside the Visual Studio developer
  environment.
- Its focused Heavy route passed across all four variants with peaks from
  `409,907,200` to `412,454,912` working-set bytes; a retained Classes frame
  was visually inspected.
- The full `ClassMngrStartupPerformanceTests.exe` suite passed after the
  Classes visual changes with the opt-in capture variables cleared.
- The full `ClassMngrStartupPerformanceTests.exe` suite passed with the opt-in
  capture variables cleared.
- The Schedule Import loading slice rebuilt the packaged Release application
  and Debug startup test target successfully inside the Visual Studio
  developer environment. Both focused boundary routes and the full
  `ClassMngrStartupPerformanceTests.exe` suite passed.
- The Calendar Import loading slice rebuilt the packaged Release application
  and Debug startup test target successfully inside the Visual Studio
  developer environment. Its focused packaged Heavy route, visual inspection,
  and the full `ClassMngrStartupPerformanceTests.exe` suite passed with all
  opt-in capture variables cleared for the full run.
- The Schedule Import conflict-warning slice rebuilt the packaged Release
  application and Debug startup test target successfully. Both focused
  boundary routes and the full `ClassMngrStartupPerformanceTests.exe` suite
  passed. The cancel warning capture was visually inspected and the profiler
  recorded peak working-set/private-usage values of
  `418,058,240`/`461,225,984` bytes; the conflict-free apply route recorded
  `360,960,000`/`402,731,008` bytes.
- `git diff --check` passed before the slice commit.

## Pending Work and Blockers

- No manual intervention is currently required to resume.
- `.codex_workflow_staging_1.1.17/` is ignored workflow state. The durable
  `agent_docs/` context is tracked; preserve it for reboot/resume context.
- Do not run native Office automation unless a suitable platform/environment
  is available; the current in-process/offscreen boundary is intentional.

## Next Entry Point

After reboot, start by reading this file and checking:

```powershell
git status --short
git log -6 --oneline
```

Then inspect the Phase 0 baseline/plan tail. The final post-cutoff commit from
this session contains the completed Schedule Import conflict-warning slice. On
the next session, resume with the next packaged Windows x64 Release Heavy-route
evidence slice, keep every slice separately committed, and do not start Phase 1
until Phase 0's evidence/contract work is genuinely complete.
