<!-- codex-workflow-id: viettran-edgeAI/codex_workflow -->
<!-- codex-workflow-managed-start -->
# AGENTS.md

## Design Principles

- Keep modules cohesive, interfaces explicit, coupling minimal, and behavior
  testable, replaceable, and reusable.
- Define proportionate acceptance and verification before implementation. Never
  weaken coverage, assertions, or failure visibility to save time or tokens.
- Avoid unnecessary process or safeguards; preserve unrelated user work and use
  verified facts in durable documentation.

## Working State

- `deployment state`: planning or executing a broad, possibly multi-session
  deployment plan.
- `leaf state`: otherwise, including general questions and small bounded
  operations.

## Project Documentation

Use the durable project documents under `agent_docs/`:

- `project_overview.md`: goals, architecture, workflow, and major decisions.
- `project_core_tech.md`: concise special technology or architecture notes.
- `project_structure.md`: layout, modules, components, and ownership.
- `project_progress.md`: goal, overall progress, current position, next milestone.
- `project_diary.md`: distilled decisions, discarded approaches, mistakes, and
  reusable lessons.
- `latest_session_work.md`: detailed handoff evidence and continuation point.
- Module-specific documents, when present.

In deployment state, you own `project_progress.md`, `project_diary.md`, and
`latest_session_work.md`. Before closure, directly record the current goal and
continuation state, concise lasting lessons, and the verified deployment
handoff in their canonical documents. Archivist owns other assigned project and
public documentation from verified facts, including overview, structure, core
technologies, and module documents, and performs the closing documentation and
reporting handoff. Require concise edits that remove stale or redundant detail,
assign module documents explicitly, and perform a direct user-requested
document edit yourself outside deployment.

Keep raw logs, temporary reasoning, and short-lived checkpoints out of durable
documents; give each fact one canonical home. Never delete a main project
document without warning and a second explicit confirmation.

## Route Selection

Select one of these routes: **Light** works directly in leaf state without subagents;
**Medium** keeps planning, diagnosis, implementation, and verification with the
main agent and uses bounded support from `~/.codex/codex_workflow/medium_route.md`;
**Heavy** delegates bounded production, verification, documentation,
project-context, and Internet research under `~/.codex/codex_workflow/heavy_route.md`.

Follow the user's route selection. Use Light when none is selected; do not infer
Medium or Heavy. Keep the route until the user changes it or the session ends.
Enter deployment state for Medium or Heavy only when the work is substantive.

## Rollout Efficiency

Batch independent reads, searches, metadata checks, and other known-input
operations. Keep dependencies and overlapping mutations sequential. In Medium or
Heavy, dispatch independent workers, wait for the
relevant set, and synthesize their reports once.

Read personalization and project-local instructions from the protected regions
at the end of this file. Apply them over workflow defaults subject to higher
instruction priority.

## Required Documentation Read

On the first `deployment state` entry under either Medium or Heavy, immediately
create one persistent Companion with `agent_type="companion"`,
`task_name="companion"`, and `fork_turns="none"`, or reuse the existing target.
Do this before planning, modifying files, or dispatching any other worker. Reuse
that Companion after route changes; do not create a second one.

Give its first assignment the current route, goal, relevant constraints, and a
bounded diary/module intake or other substantial context consolidation. It
retains supporting detail and returns only a task-relevant director brief.

If you have not already completed the session-level intake, directly read the
complete current `agent_docs/` framework exactly once: overview, core
technology, structure, progress, diary, latest session work, and every
module-specific Markdown document. This one direct read is shared across Medium
and Heavy. Never repeat it later in the session. Use retained context or assign
Companion a bounded diary/module intake, large synthesis, delta, or conflict
check when freshness or detailed supporting context matters. Missing or
unreadable required documents leave deployment entry incomplete; report the
intake blocker.

Do not overuse Companion. Each rollout reloads its persistent context. Combine
related questions, reuse earlier findings, and avoid status-only requests, tiny
lookups already answerable from main context, or repeated broad summaries. Use
it when one consolidated result replaces multiple main reads or tool turns,
suppresses bulky evidence, or will be reused later.

## Platform Paths

Interpret `/` as a platform-neutral separator and translate paths for the
current operating system and shell.
<!-- codex-workflow-managed-end -->

<!-- codex-workflow-project-personalization-start -->
<!-- codex-workflow-project-personalization-end -->

<!-- codex-workflow-project-local-instructions-start -->
# Agent orchestration

The parent agent is the orchestrator and final authority.

Preferred default parent:

- Terra High for normal planning, decomposition, orchestration, review and integration.
- Escalate to Sol when materially stronger global judgement is required.

Use the cheapest worker that can reliably complete the task:

- Spark: optional micro-worker for tiny, localised, deterministic implementation tasks.
- Luna: default bounded implementation worker.
- Terra: difficult bounded implementation, investigation or debugging.
- Sol: consequential architecture, security-sensitive work, difficult cross-system reasoning or repeated worker failure.

## Model availability

Model availability depends on the current account and Codex runtime.

Never assume every configured worker model is available.

Spark is optional. If Spark is unavailable, unsupported, rate-limited, or fails to launch because the current account does not have access:

- do not retry Spark;
- immediately route the same work package to Luna;
- do not treat missing Spark access as a task failure.

The workflow must remain fully usable without Spark.

## Direct work vs delegation

Do not invoke a worker merely because one exists.

The parent may handle directly:

- questions and analysis;
- trivial changes where delegation would cost more than execution;
- architecture or product decisions that must be resolved before implementation.

Delegate when implementation can be packaged into a clear, independently judgeable unit.

## Spark routing

Prefer Spark when all of the following are true:

- the required outcome is unambiguous;
- the relevant files or component are already known;
- the change is localised;
- architecture is already decided;
- failure is cheap to detect and correct;
- deterministic validation can be specified.

Good Spark tasks include:

- small targeted edits;
- straightforward single-function changes;
- simple tests;
- mechanical refactors;
- renames and repetitive edits;
- documentation changes;
- small UI or styling changes with explicit textual requirements.

Do not use Spark when:

- the bug cause is unknown;
- substantial repository exploration is required;
- hidden or cross-system invariants must be inferred;
- authentication, authorisation, security or data integrity is involved;
- concurrency or complex state behaviour is involved;
- architecture or product judgement is unresolved;
- image or screenshot inspection is required;
- broad repository context is required.

Spark is an execution worker, not a final reviewer.

Always give Spark explicit validation commands or checks.

## Luna routing

Use Luna Max for normal bounded implementation when the task:

- requires more coding judgement than a mechanical edit;
- may span several related files;
- has clear architecture and contracts;
- remains independently testable;
- does not require major product or architecture decisions.

Luna is the default implementation worker when Spark is unavailable or not appropriate.

## Terra routing

Use Terra High for bounded work requiring materially more independent reasoning, such as:

- difficult debugging;
- investigation where the cause is not obvious;
- subtle state or lifecycle behaviour;
- cross-cutting but still bounded implementation;
- migrations;
- integration problems;
- reviewing a suspicious worker result.

The normal Terra Medium parent may solve such work directly or delegate to a Terra High worker when parallelism or context isolation is useful.

## Sol escalation

Use Sol when the problem requires stronger global judgement rather than merely more implementation effort.

Escalate to Sol for:

- major architecture decisions;
- authentication or security-sensitive design;
- consequential data or infrastructure changes;
- difficult cross-system reasoning;
- unresolved product ambiguity;
- repeated failure after a well-specified Terra attempt;
- final review of particularly high-risk changes.

Do not use Sol merely to compensate for poor task decomposition.

## Workflow

For non-trivial implementation:

1. Understand the request and relevant code.
2. Resolve architectural and product ambiguity.
3. Make a concise implementation plan.
4. Split implementation into small, self-contained packages.
5. Select the cheapest appropriate worker.
6. Give the worker only the context required for that package.
7. Require deterministic validation where possible.
8. Review the returned diff and validation evidence.
9. Send narrowly scoped correction work if required.
10. Integrate and perform final acceptance.

## Worker selection

Use this escalation order when appropriate:

Spark -> Luna -> Terra -> Sol

This is an escalation ladder, not a requirement to try every model.

If a task obviously requires Luna, Terra or Sol, route directly to that model.

Do not repeatedly retry the same model when a clearly specified task has already demonstrated that the model is insufficient.

## Delegation package

Every worker assignment must contain:

- ROUTE
- GOAL
- CONTEXT
- SCOPE
- DO NOT TOUCH
- CONTRACT
- DONE WHEN
- VALIDATION
- RETURN

### ROUTE

Specify the selected worker:

- spark-worker
- luna-worker
- terra-worker
- sol-escalation

### GOAL

One precise outcome.

### CONTEXT

Only information necessary to perform this task.

Do not dump the parent's full context into workers.

### SCOPE

Files, components or behaviour owned by the worker.

### DO NOT TOUCH

Relevant boundaries that must remain unchanged.

### CONTRACT

Interfaces, invariants, inputs, outputs and existing behaviour that must be preserved.

### DONE WHEN

Concrete and testable acceptance criteria.

### VALIDATION

Exact tests, builds, linting, type checks or smoke tests to run.

Validation must always be explicit for Spark.

### RETURN

Return only:

1. files changed;
2. concise implementation summary;
3. validation performed and exact result;
4. assumptions;
5. remaining risks, questions or blockers.

## Worker rules

Workers must:

- make the smallest defensible change;
- follow existing repository patterns;
- stay inside assigned scope;
- preserve contracts;
- avoid unrelated refactors;
- avoid scope expansion;
- stop and report ambiguity instead of guessing;
- perform requested validation;
- never self-approve the overall project.

A task is too broad if the worker must:

- rediscover the overall architecture;
- decide major design questions;
- infer unclear product requirements;
- modify several unrelated areas;
- guess what the parent intends.

## Failure and escalation

If Spark fails a clearly specified task:

- do not repeatedly retry Spark;
- route the same bounded task to Luna.

If Luna fails:

1. check whether the assignment was unclear or too broad;
2. improve or split the package if necessary;
3. if the package was already good, escalate to Terra.

If Terra fails:

- determine whether the problem is architectural, ambiguous or consequential;
- escalate those problems to Sol.

Prefer better decomposition before stronger models.

## Concurrency

Parallelise only genuinely independent work.

Never allow multiple writing workers to modify overlapping areas simultaneously.

## Review

Worker validation comes before parent review whenever possible.

Workers may inspect and test their own implementation, but their assessment is not final acceptance.

The parent owns:

- integration;
- review;
- correction decisions;
- final acceptance.

For consequential issues escalated to Sol, resolve the issue before final acceptance.

## Context discipline

Keep the parent focused on:

- user requirements;
- architecture;
- decisions;
- plan;
- worker packages;
- concise worker summaries;
- diffs;
- validation evidence.

Workers should return evidence rather than long transcripts or raw logs.

Keep worker context minimal and sufficient.
<!-- codex-workflow-project-local-instructions-end -->
