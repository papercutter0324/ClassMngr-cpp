# Workflow Update

Supported command forms:

    codex_workflow --update

Use Python 3.11 or newer. Apply the validated update directly with the lifecycle
CLI.

## Source

Use the script to query GitHub Releases, select the highest non-draft SemVer
release containing both the universal ZIP and `SHA256SUMS`, verify the checksum,
and extract it safely. Include prereleases and never clone the repository. Let
the installed launcher delegate planning and application to the incoming CLI,
which validates its package schema.

## Update

Run:

```text
python3 ~/.codex/codex_workflow/runtime/workflow.py update --project <project>
```

When the installed package still stores `VERSION` at its root, run the incoming
package's `runtime/workflow.py` instead of the installed launcher. The incoming
runtime recognizes that historical layout and migrates it transactionally.

Let the script replace installed routes, worker TOMLs, and workflow-owned skills
with the incoming release's fixed definitions. Expect it to preserve unrelated
Codex settings and skills, project documents, personalization, project-local
instructions, source backups, and the project's enabled/disabled state. For a
project still using an older workflow version, expect the script to validate its
managed region against that version's source backup. Expect it to remove
obsolete workflow-owned files and the retired workflow-owned `agent_docs/`
`.gitignore` rule, create a verified timestamped backup, and apply user/project
state through one compensating transaction. Preserve an `agent_docs/` ignore
rule that the user owns outside the workflow-managed block.

If a legacy project entry point contains merged local edits, expect the update
to stop. Review and extract only the project-local instructions into a temporary
file, then rerun with:

```text
--legacy-local-instructions <reviewed-file>
```

Treat this as a one-time migration into the dedicated local region. Never infer
the content automatically. Add `--allow-downgrade` for a downgrade.

Report the installed version, backup location, and any failure.
Do not describe a partial or rolled-back update as successful.
