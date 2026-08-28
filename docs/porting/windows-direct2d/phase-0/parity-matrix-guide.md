# Phase 0 parity matrix guide

[`parity-matrix.csv`](parity-matrix.csv) is the cutover ledger. It deliberately
keeps Phase 0 baseline evidence separate from implementation parity: source
inspection, a Qt test, or a captured Qt artifact does not mean the future
native implementation has parity.

## Columns and evidence

Each feature has independent values for data read/write, keyboard/input, visual
state, error behavior, print/export, and x64 performance. The retained ARM64
performance column is informational only and uses `deferred-unofficial` until
official ARM64 support is planned. `not-assessed` means x64 evidence has not
yet been captured; `not-applicable` must be justified by the feature contract
rather than used as a shortcut.

`capture_ledger` links to one or more stable IDs in
[`capture-ledger.csv`](capture-ledger.csv). Those IDs identify the Qt baseline
artifacts needed before a native comparison can be reviewed. `evidence` names
the source, test, or result document that establishes the scope.

`phase0_baseline` may be `source-inventory`, `startup-x64-captured`,
`fixtures-verified-x64`, or `pending`. It describes only the current Qt
baseline. `native_status` remains `not-started` until a native target produces
comparable x64 evidence. UI Automation, Narrator, and high-contrast support
are deferred and must not be inferred from the keyboard/input column.

## Recording native evidence

For the x64 release gate, record the artifact reference and outcome in the
relevant cell. Use these values consistently:

- `pending` — no native evidence yet;
- `pass` — reviewed against the specified Qt baseline artifact or semantic
  oracle;
- `fail` — observed mismatch, linked to an issue and reproduction;
- `waived` — approved intentional platform-native difference, with rationale;
- `not-applicable` — no contract applies.

The status cannot be promoted from `not-started` until every applicable x64
cell has a reviewed result. A feature can be `pass` only after the same copied
database fixture or named capture state was exercised where the row requires
data, text input, output, or error behavior.

## Phase 0 completion rule

Phase 0 does not require native `pass` results. It requires that each matrix
row has a source baseline, linked capture-ledger scope, and an unambiguous
comparison rule. The eventual cutover gate requires every applicable x64 field
to have reviewed evidence; source code citations alone never satisfy that gate.
Official ARM64 support requires a separate future parity decision.

