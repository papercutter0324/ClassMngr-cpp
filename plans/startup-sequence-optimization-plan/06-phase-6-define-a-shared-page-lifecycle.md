<!-- ClassMngr Startup Optimization Plan — Phase 6 -->

> **Codex scope:** Read `00-START-HERE.md` plus this file. Implement and validate **Phase 6 only**. Do not start later phases.

# Phase 6 — Define a Shared Page Lifecycle
## Objective
Make page behavior predictable across all platforms and prevent constructors or show events from becoming hidden initialization paths.

## Tasks
### 6.1 Establish lifecycle responsibilities
Use existing architecture where possible, but enforce these semantics.

#### Construction
Allowed:

- create lightweight UI structure;

- connect signals;

- initialize trivial local state;

- create lightweight models.

Avoid:

- expensive database queries;

- resource-pack acquisition;

- full dataset loading;

- expensive rendering.

#### First-use preparation
Allowed:

- acquire feature resources;

- create expensive child widgets;

- initialize feature-specific infrastructure.

#### Activation
Allowed:

- load visible data if stale;

- update content required for the current navigation context.

#### Refresh
Use only when data or relevant preferences changed.

#### Deactivation
Where beneficial:

- pause timers;

- release temporary resources;

- discard short-lived caches.

Do not destroy/recreate pages simply because the user navigates away.

### 6.2 Audit heavy `showEvent()` work
Review all page `showEvent()` implementations.

A `showEvent()` should not cause a complete data reload when activation has already loaded current data.

In particular, remove duplicate schedule refresh behavior.

### 6.3 Introduce stale/dirty state where appropriate
Examples:
```cpp
bool m_needsRefresh = true;
```
or a data-generation/version mechanism.

When hidden data changes:
```text
mark page stale
```
When the page becomes active:
```text
refresh only if stale
```
### 6.4 Keep behavior shared
Do not create Windows-only lazy behavior.

All pages should follow the same lifecycle regardless of platform.

## Acceptance Criteria
First use:
```text
construct once

→ prepare once

→ load/render once
```
Returning to an unchanged page:
```text
activate

→ no unnecessary query

→ no unnecessary full render
```
---
