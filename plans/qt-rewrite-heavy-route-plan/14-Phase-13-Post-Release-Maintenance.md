# Phase 13 — Post-Release Maintenance

Read [00-Start-Here.md](00-Start-Here.md) first for the overall plan, workflow, and phase sequence.

## Mandatory route for new and resumed work

- Use the Heavy route for every task, slice, follow-up, and reopened gate in this phase. `Default route: Heavy` is a binding instruction.
- At each start or resume, explicitly select Heavy and reread this phase plan and [00-Start-Here.md](00-Start-Here.md). Follow the active `AGENTS.md` Heavy-route instructions and `~/.codex/codex_workflow/heavy_route.md`, including their delegation, verification, and deployment-state requirements.
- A new session, handoff, or context reset does not change the route. Do not continue under Light or Medium. If a required Heavy-route step blocks progress, report the blocker before implementation instead of silently switching routes.

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

## Status

- Status: Not started
- Default route: Heavy
- Depends on: Phase 12
- Blocks: Long-term architecture stability
- Owner: Unassigned
- Last updated: 2026-09-16
- Current note: Prevent another accumulation of compatibility layers, global state, and unbounded memory.

### Slice discovery batches

Discover upcoming slices in ordered batches of up to ten (or all remaining
slices if fewer than ten remain). Record each batch as an ordered list under
`Recorded batches` below and work through those slices in order. Begin discovering
and recording the next batch when starting work on the second-last slice in the
current batch. If a discovery pass finds fewer than ten slices, add the exact
standalone line `No other slices were found.` beneath that batch.

Keep the Status `Current note` limited to the latest information relevant to the
current or next slice. Keep only the most recent slice commit in the
`Latest Progress Update` section. When writing a newer update, move the previous
one to this phase's progress log before replacing it.

#### Recorded batches

Add each ordered discovery result here as it is found.

## Progress log

Record this phase's progress here. Add a dated entry when work starts, a
milestone is reached, a blocker appears, or the exit gate passes. Append entries
in date order and include what changed, what remains, evidence or a verification
command, and any new risk or blocker.

Entry format:

### YYYY-MM-DD — <milestone or update>

- Changed:
- Remaining:
- Evidence:
- Risks or blockers:

## Objective

Make the rewritten architecture durable after release.

## Maintenance rules

### Architecture

- New features must be vertical slices.
- UI code cannot issue SQL.
- Domain code cannot depend on Qt Widgets.
- Platform code must remain behind adapters.
- Feature modules cannot reach into one another's private implementations.
- Global mutable state requires explicit architectural approval.

### Resources

- Every large resource requires a loading policy.
- Every cache requires a byte or item budget.
- Every resource scope requires a release test.
- Every startup resource must justify its startup classification.
- Document catalogs may load metadata at startup, but PDF content must remain
  in an active viewer/operation session and its QtPdf document must be closed
  and released when that session ends.
- No runtime resource download may be added without an explicit product decision.
- Application updates must include all required resources.

### UI and memory

- Every page declares creation, activation, suspension, and release behavior.
- Every new large table uses model/view unless an exception is documented.
- Large images must have a bounded decode policy.
- Export workflows must release temporary objects.
- Document viewer open/close/reopen tests must detect retained QtPdf documents
  or unbounded rendered-page caches.
- Windows packaged Release memory tests run continuously.
- The [Qt Rewrite Memory Hotspot Remediation
  Plan](memory-hotspot-remediation-plan.md) remains the reference for
  ownership, projection, model/view, operation-scope, and release-review
  requirements; new feature work must not reintroduce its retired patterns.

### Compatibility

- Legacy .db import remains tested.
- .tps compatibility remains tested.
- Import/export fixtures remain versioned.
- Schema changes require migration and backup tests.
- Output fixtures are updated only with an approved behavior change.

### Visual quality

- Visual regression tests cover English and Korean.
- Visual regression tests cover light and dark themes.
- Typography changes require screenshot review.
- Print and PDF changes require output review.

## Monitoring

Track after every significant release:

- Startup memory.
- Idle memory.
- Peak memory.
- Repeated-navigation growth.
- Resource cache size.
- Active document sessions and retained PDF/rendered-page bytes.
- Startup duration.
- Page creation time.
- Large import duration.
- Report-generation duration.
- Crash and recovery behavior.

Keep diagnostics capable of identifying which page, resource, model, or output operation owns a regression.

## Deliverables

- Architecture contribution rules.
- Resource and cache review checklist.
- Continuous Windows memory report.
- Continuous visual regression report.
- Compatibility fixture maintenance process.
- Release diagnostic dashboard or equivalent artifact.

## Exit gate

The post-release process can detect architectural, memory, resource, compatibility, and visual regressions before they reach a stable release.

## Heavy-route requirements

- For every Phase 13 maintenance slice, use the heavy route: preserve the v2
  boundaries, verify the complete affected behavior and gates, and remove any
  temporary compatibility path instead of extending it indefinitely.
- Do not allow temporary compatibility code to become permanent.
- Do not accept unbounded caches for convenience.
- Do not bypass memory or visual gates for small feature additions.
- Do not reintroduce separately updated resources without revisiting the product requirement.
