<!-- ClassMngr Startup Optimization Plan — Phase 9 -->

> **Codex scope:** Read `00-START-HERE.md` plus this file. Implement and validate **Phase 9 only**. Do not start later phases.

# Phase 9 — Refine Resource-Pack Initialization

> **Status:** Complete — validated 2026-08-27.

## Objective
Ensure feature resource packs are discovered, validated, and mounted only as needed.

## Tasks
### 9.1 Audit resource initialization
Separate:
```text
lightweight pack registry/metadata discovery
```
from:
```text
expensive file hashing

pack validation

pack mounting

feature parsing
```
where safe.

### 9.2 Keep feature packs demand-driven
Examples:
```text
campus resources → first Campus use

templates → first template/report use

document assets → first document use

PDF resources → first PDF use
```
### 9.3 Avoid unnecessary full startup hashing
If large packs are hashed every startup, evaluate safe alternatives such as validation:

- after installation/update;

- when file metadata indicates change;

- on first acquisition after change.

Do not weaken integrity protections without justification.

### 9.4 Keep pack behavior cross-platform
Resource-pack lifecycle should be shared unless platform storage behavior genuinely requires a difference.

## Acceptance Criteria
Normal startup does not read, hash, parse, or mount large feature packs unrelated to the initial page.

---
