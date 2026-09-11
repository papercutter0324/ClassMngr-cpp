# Medium Route

Use after Medium is selected under `AGENTS.md`.

## Main Ownership

You are the main agent. Own planning, root-cause reasoning, implementation,
production repair, verification, integration, acceptance, final claims, and user
communication. Medium does not delegate production or verification.

Optimize for fewer main-agent decision turns and lower main-agent context
consumption while preserving quality and completion. Aggregate support-worker
token use is not the optimization target.

Use only these support roles:

| Role | Ownership |
| --- | --- |
| Companion | One required persistent read-only secretary for project context, diary/module intake, large synthesis, and retained operational context. |
| Investigator | A disposable read-only evidence worker for a bounded project or Internet context gap the main does not already understand. It supplements context; the main decides root cause and solution. |
| Archivist | The required substantive-deployment closure worker defined by `~/.codex/codex_workflow/archivist.md`. It owns concise assigned documentation outside the main-owned deployment-state documents, read-only Git reporting, and the closing Deployment Token Report. |

## Shared Deployment-State Entry

Before route-specific planning or execution, complete the shared first-entry
Companion and `agent_docs/` contract in `AGENTS.md`. Do not repeat that intake
or create another Companion when it was completed earlier under either route.

## Context Routing After Intake

Immediately after the shared intake and before broader source discovery or
planning, use `agent_docs/` and Companion's initial brief to create a compact
working-context map:

- **Direct**: code, contracts, interfaces, and evidence the main must inspect to
  own diagnosis, implementation, verification, integration, risk, or acceptance.
- **Companion**: supporting modules, tools, configuration, logs, dependencies,
  and other non-decisive project surfaces whose function or state should be
  returned as one bounded summary.
- **Investigator**: one bounded unfamiliar or ambiguous project or Internet
  evidence gap not resolved by the main's intake or Companion's retained
  context.

Keep this map in working state, not durable documentation, and revise it only
when material evidence changes relevance. Do not directly explore a Companion
or Investigator surface unless it becomes necessary for main-owned production
or a material decision; update the map explicitly when that happens. Combine
related Companion questions, and create an Investigator only when independent
investigation materially reduces an unresolved context gap.

## Deployment Boundary

At the start of each substantive Medium deployment, choose a unique lowercase
underscore-safe deployment ID and include this hidden comment once in the first
commentary message:

```text
<!-- codex-workflow-deployment-start: <deployment_id> -->
```

Keep it in the main session as Archivist's reporting boundary.

## Support Packages and Investigation

Start each initial support package with **Task ID**, a logical identifier unique
within the deployment, followed by the capsule for that role:

| Role | Capsule parts |
| --- | --- |
| Companion | **Project Context Scope**; **Context Task + Goal**; **Main-Agent Context Guidance** |
| Investigator | **Investigation Context**; **Evidence Question + Goal**; **Main-Agent Investigation Guidance** |
| Archivist | **Documentation Context + Audience**; **Documentation Task + Goal**; **Main-Agent Documentation Guidance** |

Treat these parts as the complete structure. Include only material context,
references, boundaries, intended outcomes, main-owned decisions, constraints,
and cautions. Require Task ID in every report. Follow-ups repeat it and send
only changed capsule parts.

The main may investigate and decide root cause because it holds the decisive
project context. Use Companion for peripheral or bulky project context. Create
Investigator only for a bounded evidence lane the main does not already
understand and whose independent project inspection or Internet research would
materially improve the decision. Investigator returns evidence and implications;
it never owns the causal, architecture, implementation, or acceptance decision.

## Rollout-Efficient Support

- Batch independent main-owned reads, searches, metadata checks, and tool
  operations into bounded calls.
- When several support workers inform one decision, dispatch them together,
  wait for the relevant set, and synthesize once. Start another batch only when
  existing evidence materially changes the questions.
- Do not poll support workers, request status-only updates, inspect activity
  files, or repeatedly request available evidence. Use lifecycle events,
  appropriately long waits, and `list_agents` only for genuine terminal-state
  uncertainty.
- Keep dependent or overlapping main-owned changes sequential and verification
  proportionate to risk. Never weaken validation or claim an unrun check passed.

## Fixed Boundaries

- Medium has no workflow-imposed aggregate active-subagent limit.
- Use exactly one persistent Companion. Limit other Medium workers to
  Investigator and Archivist.
- Give support workers bounded questions and sufficient context; retain every
  material interpretation and final claim in the main.
- Archivist owns assigned documentation outside `project_progress.md`,
  `project_diary.md`, and `latest_session_work.md`, plus closure reporting. Keep
  those three documents, implementation, production repair, root-cause
  decisions, and verification with the main.
- Preserve unrelated work and keep Git mutations within explicit authority.

## Fast Path and Closure

Use the worker-free direct fast path only when the complete request is a question
or small bounded leaf task. Do not initialize deployment state merely because
Medium remains selected.

Before the final response that completes, pauses, or blocks a substantive
deployment, update `agent_docs/project_progress.md`,
`agent_docs/project_diary.md`, and `agent_docs/latest_session_work.md` yourself,
keeping them concise and canonical. Then follow
`~/.codex/codex_workflow/archivist.md` exactly once. Combine any other verified
documentation updates with its required closure assignment when practical.
Relay its handoff and exact six-column `$deployment-token-report` table. Use a
new deployment ID for each later deployment.
