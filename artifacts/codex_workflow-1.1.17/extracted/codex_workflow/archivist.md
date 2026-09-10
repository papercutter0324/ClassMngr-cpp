# Archivist Assignments

Use Archivist for verified documentation work in Medium or Heavy. Give each
assignment a Task ID and the Documentation Context + Audience, Documentation
Task + Goal, and Main-Agent Documentation Guidance capsule. Identify the write
surface and provide verified facts or exact evidence references. Require the
smallest durable update that preserves current decisions, state, limitations,
and a recoverable continuation point; remove stale or redundant detail instead
of accumulating session history.

Choose the number, timing, and reuse of Archivist workers according to the task.
Give concurrent workers non-overlapping document ownership. Ordinary assignments
normally use `agent_type="archivist"` and `fork_turns="none"`.

## Deployment Closure

Assign one Archivist to close each substantive deployment before the final
response, including paused or blocked work. First finish your own required
updates to `agent_docs/project_progress.md`, `agent_docs/project_diary.md`, and
`agent_docs/latest_session_work.md`. Keep those files outside Archivist's write
scope, but identify them as the canonical deployment-state sources for the
handoff. Combine other verified documentation updates with this assignment when
practical. Include the deployment ID, closure state (`complete`, `paused`, or
`blocked`), and read-only Git handoff.

Reuse an Archivist when its retained context plus a concise delta is sufficient.
Otherwise create one with `agent_type="archivist"`, a unique task name such as
`archivist_<deployment_id>`, and `fork_turns="200"`. For this inherited-context
assignment, reference the fork as documentation context instead of writing a
deployment summary. Supply only scope and guidance the fork does not establish.

Ensure other workers have finished changes relevant to the handoff before
Archivist seals it. Assign only one reporting owner for each deployment. Wait
for its documentation handoff and exact six-column `$deployment-token-report`
table, then relay both without repeating its operational checks. A later
substantive deployment uses a new ID and receives its own closure and report.

For questions and small bounded tasks on the direct fast path, work directly
without this closure or token report. If Archivist is unavailable or blocked,
report the limitation and the remaining work accurately.
