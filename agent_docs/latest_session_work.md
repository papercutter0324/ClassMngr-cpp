# Latest Session Work

Qt rewrite Phase 0 is still in progress on branch `Qt-Rewrite`.

## Detailed Current State

- User instruction: use the heavy route for every slice; commit each slice and
  immediately continue unless the user asks to pause. The latest user request
  is to complete the current task, record resume context, and pause.
- The `<250 MiB` RAM value is the target for the completed rewrite, not a
  Phase 0 acceptance gate for the legacy widget graph. Phase 0 retains current
  packaged Windows x64 Release measurements as baseline/trend evidence. Later
  phases must avoid regressions and should lower the affected heavy-route
  working set. The temporary `<512 MiB` ceiling is diagnostic only.
- Phase 0 is not complete and Phase 1 must not begin yet. Remaining gaps
  include Sub Prep editing/read-only/loading and other feature error states,
  remaining generated-output references, cross-platform Release evidence, and
  the eventual v2 memory/ownership remediation.

## Session Changes

- `b5c1a8a1` — `Phase 0: retain PowerPoint renderer reference`
  - Added a real Speaking Evaluation export-dialog PowerPoint renderer
    selection capture and checkpoint to the packaged heavy route.
  - Retained `speaking-evaluation-powerpoint-renderer.png`.
  - Manifest records `powerPointRendererVisualReference=true` and
    `powerPointAutomationExecuted=false`; external Office automation was not
    run in the Windows offscreen environment.
- `e3fcde2d` — `Phase 0: retain Sub Prep validation reference`
  - Extended the real packaged Sub Prep generation dialog controller to clear
    both output options, capture the validation-error state, verify the OK
    action is disabled, restore valid controls, and complete normal generation.
  - Retained `sub-prep-output-validation-error.png`, checkpoint/trace evidence,
    updated metrics, and manifest flag `validationErrorVisualReference=true`.
- `17b3336b` — `Phase 0: restore Sub Prep PDF evidence`
  - Restored/tracked the two generated PDFs that were temporarily staged as
    deletions because the app-created output folder had a sandbox-only ACL.
  - The exact generated evidence folder was granted read access to the normal
    Git identity; no source behavior changed.

Prior completed commits remain in the log, including the PDF viewer visual
states, resource ownership trace, memory-target trend contract, and Sub Prep
visual references.

## Verification

- Packaged Windows x64 Release `ClassMngr.exe` rebuilt successfully after the
  Sub Prep validation change.
- Focused heavy route passed:
  `capturesLargeSubPrepOutputBoundaryWhenConfigured` (exit code 0).
- Full `ClassMngrStartupPerformanceTests.exe` suite passed (exit code 0).
- Validation capture was visually inspected and shows the real dialog message
  `Select Create Folder and/or Print Paper Copies to continue.` with OK
  disabled.
- The Sub Prep validation route retained output checkpoints at
  `8,747`/`9,296`/`9,337 ms`, generated two PDFs over 17 pages, released to
  zero live PDF documents, and peaked at `498,176,000` working-set bytes,
  below the temporary 512 MiB diagnostic ceiling.
- `git diff --check` passed before the slice commit.

## Pending Work and Blockers

- No manual intervention is currently required to resume.
- The worktree contains workflow-generated, untracked
  `.codex_workflow_staging_1.1.17/` and `agent_docs/` files. They are workflow
  state/support artifacts, not Qt rewrite source changes; preserve them for
  reboot/resume context.
- Do not run native Office automation unless a suitable platform/environment is
  available; the current in-process/offscreen boundary is intentional.

## Next Entry Point

After reboot, start by reading this file and checking:

```powershell
git status --short
git log -6 --oneline
```

Then inspect the Phase 0 baseline/plan tail. Resume with the next packaged
Windows x64 Release heavy-route evidence slice, most directly the remaining
Sub Prep editing/read-only/loading state or another explicitly open Phase 0
visual/output gap. Keep every slice separately committed and do not start
Phase 1 until Phase 0's evidence/contract work is genuinely complete.
