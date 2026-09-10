# Enable the Project Workflow

Run the installed lifecycle CLI from the project directory:

```text
python3 ~/.codex/codex_workflow/runtime/workflow.py enable --project <project> --json
```

Expect the command to move the recognized hidden entry point atomically to
`AGENTS.md`, update project state, and preserve its exact contents. Treat an
already enabled project as a safe no-op and a conflicted or unrecognized entry
point as a hard error.
