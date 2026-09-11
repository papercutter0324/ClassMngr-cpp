# Personalize the Current Project

Run this procedure only when the user's trimmed message is exactly:

    codex_workflow --personal

Use this persistent resource:

    .codex_workflow_hidden_resources/personalization.md

Use Python 3.11 or newer with the lifecycle CLI to apply a validated resource
directly:

    ~/.codex/codex_workflow/runtime/workflow.py

## Questions

First read and validate the installed default resource at
`~/.codex/codex_workflow/resources/personalization.md`. Then read the current
project resource. If the project resource is missing or invalid, tell the user
that it needs recovery and use the installed default as the proposed starting
state; do not attempt to preserve malformed text.

Ask, in order:

1. Frontend Project Profile.
2. Design Principles.
3. Additional Workflow Decisions.

Show the current decision and allow **Keep current** or **Reset to default**.
For **Reset to default**, copy that section's complete `Status:` and `Decision:`
values from the validated installed default resource. In recovery mode, **Keep
current** means keep the corresponding installed default section because no
valid project value exists. Allow the user to reset all three sections by
choosing **Reset to default** for each. Store only confirmed project-scoped
instructions. Never store secrets, logs, temporary state, or worker
configuration.

## Plan and apply

Write the complete proposed resource, including exactly the three required
headings and their `Status:` and `Decision:` fields, to a temporary file. Run:

```text
python3 ~/.codex/codex_workflow/runtime/workflow.py personalize \
  --project <project> --resource <candidate> --json
```

Run the command once with the complete candidate and `--json`. Delete the
temporary candidate afterward, including after an error. A missing or invalid
candidate changes no live file. If the current resource was missing or invalid,
a successful application is the explicit recovery and recreates it.

Let the script validate all three sections and atomically update the resource
and generated personalization region. Preserve the workflow-managed and
project-local regions and the enabled/disabled entry-point state through that
operation.
