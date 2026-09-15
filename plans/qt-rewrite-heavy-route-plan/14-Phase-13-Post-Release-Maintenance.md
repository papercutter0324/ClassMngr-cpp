# Phase 13 — Post-Release Maintenance

## Status

- Status: Not started
- Default route: Heavy
- Depends on: Phase 12
- Blocks: Long-term architecture stability
- Owner: Unassigned
- Last updated: 2026-09-16
- Current note: Prevent another accumulation of compatibility layers, global state, and unbounded memory.

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

- Do not allow temporary compatibility code to become permanent.
- Do not accept unbounded caches for convenience.
- Do not bypass memory or visual gates for small feature additions.
- Do not reintroduce separately updated resources without revisiting the product requirement.
