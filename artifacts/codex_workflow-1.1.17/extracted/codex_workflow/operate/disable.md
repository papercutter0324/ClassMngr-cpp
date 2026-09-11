# Disable the Project Workflow

Run the installed lifecycle CLI from the project directory:

```text
python3 ~/.codex/codex_workflow/runtime/workflow.py disable --project <project> --json
```

Expect the command to move the recognized `AGENTS.md` atomically to the hidden
entry point, update project state, and preserve its exact contents. Treat an
already disabled project as a safe no-op and a conflicted or unrecognized entry
point as a hard error.
