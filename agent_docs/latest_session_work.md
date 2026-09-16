# Latest Session Work

Qt rewrite Phase 0 is still in progress on branch `Qt-Rewrite`.

## Detailed Current State

- User instruction: use the Heavy route for every slice; commit each slice and
  immediately continue unless the user asks to pause. The current request is
  to commit the completed slice and continue.
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

Then inspect the Phase 0 baseline/plan tail. Resume with the next packaged
  Windows x64 Release Heavy-route evidence slice, now continuing with another
  explicitly open Phase 0 visual/output gap. Keep every slice separately
  committed and do not start Phase 1 until Phase 0's evidence/contract work is
  genuinely complete. If a commit is made after 13:00 Asia/Seoul, stop after
  that commit; if work is still active at 13:15, commit a handoff update and
  stop.
