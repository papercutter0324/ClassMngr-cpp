---
name: deployment-token-report
description: Compile per-agent rollout counts and cached-input, input, and output token totals from Codex local session JSONL after a substantive workflow deployment. Use only for the workflow's required post-deployment usage handoff, not for direct-fast-path work or live cost estimation.
---

# Deployment Token Report

<!-- codex-workflow-skill: deployment-token-report -->

Use this skill only as Archivist for an assigned deployment closure, after all
assigned documentation updates, compact checks, and Git inspection are complete.
Treat that closure state as sealed;
the repository and closure evidence remain unchanged after reporting starts.

Confirm that the main agent placed this exact hidden comment in its first
commentary message for the deployment, using the supplied unique lowercase
underscore-safe ID:

```text
<!-- codex-workflow-deployment-start: <deployment_id> -->
```

Run the bundled `scripts/report_tokens.py` with `--deployment-id` and
`--format markdown`. Let it use `CODEX_THREAD_ID` to identify this Archivist
rollout, resolve its parent main-agent thread, find the exact marker in
assistant message text there, and read only metadata and token-count fields
beneath `~/.codex/sessions/`. Accept the marker when surrounded by Markdown or
explanatory prose. Exclude guardian sessions.

Return only the script's six-column Markdown table verbatim to the main agent,
with no pricing, estimates, inferred usage, or additional statistics. Treat a
script failure or incomplete-evidence warning as the report result and return
the limitation verbatim.

The required table template is exactly:

```text
| Agent | Quantity | Rollouts | Cached input | Input | Output |
| --- | ---: | ---: | ---: | ---: | ---: |
| <agent role> | <count> | <count> | <tokens> | <tokens> | <tokens> |
```

Keep the columns exactly as shown and preserve every data row emitted by the
script, including its final `main agent` row.

Interpret `Input` as total input tokens, including the cached-input subset, and
`Rollouts` as model generations with a `last_token_usage` record. Stop totals
when the script starts, excluding Archivist's post-tool final response
and the main agent's later final response.
