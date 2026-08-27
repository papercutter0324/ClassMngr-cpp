# Reference capture protocol

This protocol captures the Qt Windows product before a native equivalent is
implemented. Store files outside the source tree or in an approved release
artifact store; only redacted metadata and stable fixture-derived outputs are
checked in.

## Capture matrix

For every surface in [feature-inventory.md](feature-inventory.md), capture the
following fixture-backed states where they exist: no database/empty, populated,
loading/background work, validation/error, dirty/unsaved, disabled command,
and completed/cancelled output.

| Dimension | Required values |
| --- | --- |
| Theme | light, dark |
| Scaling | 100%, 150%, 200% Windows display scaling |
| Window | default size plus narrow/short and large layouts where the feature resizes |
| Language/input | English UI/input and Korean UI/input; test Hangul composition, commit, backspace, arrows, selection, clipboard, undo/redo |
| Keyboard/accessibility | keyboard-only focus order and Escape/default behavior; Narrator name/role/state/value for each interactive control |
| Output | representative PDF/print preview/export files containing English and Korean text |

## Artifact naming

Use `<feature>__<state>__<theme>__<dpi>__<language>`. For example,
`schedule__import-review__dark__150__ko.png` and
`speaking-evaluation__batch-report__light__100__en.pdf`. Pair each image/PDF
with a `.json` metadata record containing the source revision, fixture ID,
architecture, Windows edition/build, display scaling, app language, input
language, font-size setting, window size, and exact action sequence.

## Work ledger

[`capture-ledger.csv`](capture-ledger.csv) is the source-backed checklist for
the inventory. Each row supplies a stable artifact prefix and starts `pending`.
Use `in-progress`, `captured`, `verified`, or `blocked` while preserving a short
reason in its notes field. A row is `verified` only when all its required
states, input/accessibility observations, and output artifact (where required)
are represented by redacted metadata. The ledger records references and status;
screenshots, PDFs, recordings, and raw diagnostics remain external artifacts.

## Metadata sidecars and validation

After recording a screenshot, PDF, text observation, or other external
artifact, generate its metadata sidecar from the ledger ID. The filename must
begin with that ID's `artifact_prefix` followed by `__`; the script writes a
same-directory `<artifact-name>.json` sidecar and records the artifact's
SHA-256.

```powershell
.\scripts\porting\windows\new_phase0_capture_metadata.ps1 `
  -LedgerId page.calendar `
  -Architecture x64 `
  -Theme dark `
  -DisplayScalePercent 150 `
  -AppLanguage ko `
  -InputLanguage ko-KR `
  -FixtureId typical `
  -ArtifactPath D:\ClassMngrCapture\calendar__populated__dark__150__ko.png `
  -SourceRevision 48fc5c5 `
  -WindowsEdition "Windows 11 Pro" `
  -WindowsBuild 26100 `
  -Action "Open typical fixture" `
  -Action "Navigate to Calendar"
```

Replace the generated `TODO` observations with the actual keyboard, IME/UIA,
and review notes, then set `verification` to `verified` only after review.
Validate a capture store before linking artifacts in the ledger:

```powershell
.\scripts\porting\windows\validate_phase0_capture_artifacts.ps1 `
  -ArtifactRoot D:\ClassMngrCapture `
  -RequireVerified
```

## Manual workflow

1. Build the current Qt Windows product from a clean settings root and open a
   copied approved fixture.
2. Set the theme, language, display scale, and window size specified by the
   matrix. Do not use private production data.
3. Take the screenshot only after async work has settled; preserve the visible
   focus ring when keyboard navigation is under test.
4. Record Narrator output/Accessibility Insights observations in the metadata,
   including missing names or unexpected focus changes.
5. For Korean IME, record the active IME and the exact composition sequence;
   screenshots must show preedit and committed text when the control exposes
   it.
6. For print/PDF/export, retain the source fixture, output checksum, and
   operation settings; visually inspect Korean glyphs, margins, pagination,
   tables, images, and cancellation/failure cleanup.

The first native screen cannot be declared equivalent on pixels alone: it must
also match the recorded workflow, data mutation, error, input, and accessibility
contract. Native Windows control appearance may differ where it follows platform
conventions; information hierarchy and behavior may not.
