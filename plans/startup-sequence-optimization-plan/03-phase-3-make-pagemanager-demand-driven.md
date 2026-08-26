<!-- ClassMngr Startup Optimization Plan — Phase 3 -->

> **Codex scope:** Read `00-START-HERE.md` plus this file. Implement and validate **Phase 3 only**. Do not start later phases.

# Phase 3 — Make PageManager Demand-Driven
## Objective
Register all application pages without constructing pages that are not required for the first visible screen.

This must be shared behavior on all operating systems.

## Tasks
### 3.1 Audit eager PageManager construction
Review all pages currently created by `PageManager::initialize()` or equivalent startup paths.

At minimum, evaluate these for lazy construction:

- My Classes;

- standalone Schedule;

- Sub Prep;

- Testing Classes;

- Teacher Info;

- Native English Teachers;

- GS Team;

- Classes;

- Campus Dashboard;

- PDF Viewer.

Calendar should remain lazy.

### 3.2 Register factories without instantiating pages
Desired pattern:
```cpp
registerFactory(PageType::SubPrep, ...);

registerFactory(PageType::TeacherInfo, ...);

...
```
Do not call `ensurePage()` during startup unless that page is necessary for the selected initial view.

### 3.3 Minimize the startup page graph
If startup lands on ****My Workspace → Schedule****, aim for approximately:
```text
MainWindow

├─ navigation/sidebar shell

└─ PageManager

   └─ MyWorkspacePage

      └─ visible Schedule content
```
Any hidden My Workspace subpage should also be considered for lazy child creation where practical.

### 3.4 Ensure database state works for lazy pages
A page created after startup must correctly receive:

- database-open state;

- active database/service references;

- current theme/font inherited from the application;

- current navigation context;

- any required feature state.

Do not require eager page construction simply to propagate state.

### 3.5 Verify navigation behavior
All navigation destinations should remain visible and functional before their widgets are constructed.

## Acceptance Criteria
At startup:
```text
registered pages = all required application pages

instantiated pages = only those required for initial view
```
No hidden top-level feature should be created merely because it exists in navigation.

---
