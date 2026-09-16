# Phase 0 packaged evidence automation

This folder contains a Windows PowerShell runner and a Python standard-library
evidence validator for the existing Qt startup/visual/lifecycle test harness.
The Phase 0 exit gate requires packaged Release evidence on Windows x64 and
macOS universal. Windows ARM64 and Linux are unofficial ports deferred to later
work; their missing evidence is not a Phase 0 failure. The runner produces
Windows x64 evidence only. No source, C++ test, retained baseline, or existing
`build/`/`dist/` output is changed by the default plan.

## Plan and run

Planning is the default when no evidence output root is supplied. The no-write
all-route plan prints exact build and route commands for all 24 existing
opt-in packaged routes without creating files or starting processes:

```powershell
powershell.exe -NoProfile -File scripts/phase0/run_phase0_evidence.ps1 `
  -Plan -Routes all
```

To execute the full Windows x64 run, explicitly choose a parent output
directory. A timestamped child directory is created for the run, and the
Release build, test harness build, package, screenshots, generated documents,
traces, metrics, manifests, logs, and machine summary stay under that child:

```powershell
$evidenceParent = Join-Path $env:TEMP "ClassMngr-Phase0-Evidence"
powershell.exe -NoProfile -File scripts/phase0/run_phase0_evidence.ps1 `
  -EvidenceRoot $evidenceParent
```

If the local PowerShell execution policy is `Restricted`, use a process-scoped
policy override when launching the script; the runner does not change the
machine or user execution policy.

By default the runner configures and builds into the new run directory, then
installs the packaged Release application there. It uses the repository's
configured Qt MSVC x64 prefix (`C:\Qt\6.12.0\msvc2022_64` by default). To use
already-built binaries without building, provide the packaged Release app and
test executable paths explicitly. The runner does not scan `build/` or `dist/`:

```powershell
$evidenceParent = Join-Path $env:TEMP "ClassMngr-Phase0-Evidence"
powershell.exe -NoProfile -File scripts/phase0/run_phase0_evidence.ps1 `
  -EvidenceRoot $evidenceParent `
  -SkipBuild `
  -ApplicationPath C:/path/to/package/ClassMngr.exe `
  -TestExecutable C:/path/to/tests/ClassMngrStartupPerformanceTests.exe
```

The `-Routes` argument accepts `all`, one or more categories (`visual`,
`workflow`, `output`, `memory`, `fixture`), or route IDs. For example,
`-Routes visual,workflow` selects those evidence slices. `-SkipRun` records an
intentional skip and does not produce a passing evidence gate. `-SkipValidation`
is intended for diagnostics only; run the validator before accepting such a
run. A timestamp collision receives a numeric suffix. An explicitly reused
run name is refused if it already exists; `-AllowExistingRoot` permits only an
empty existing directory, never replacement of retained evidence. With no
`-EvidenceRoot`, the runner remains plan-only even when `-Plan` was omitted.

For an existing completed run, validation is read-only unless an output file is
explicitly supplied:

```powershell
$runRoot = Join-Path (Join-Path $env:TEMP "ClassMngr-Phase0-Evidence") "phase0-<timestamp>"
python scripts/phase0/validate_phase0_evidence.py `
  --evidence-root $runRoot
```

This read-only invocation prints the machine-readable summary to stdout. When
an output file is requested, the validator refuses to replace an existing file
unless `--force-json-out` is supplied. Its deterministic standard-library
checks can be run without a packaged application or evidence output root:

```powershell
python scripts/phase0/validate_phase0_evidence.py --self-test
```

## Evidence and acceptance behavior

The route set invokes the existing startup visual capture command and the
opt-in QtTest slots declared in `tests/startup_performance_tests.cpp`. It
retains a route manifest with the exact executable, argument array, working
directory, relevant child-process environment, completion status, and log
paths. The run manifest identifies the Windows x64 acceptance platform,
deferred unofficial platforms, requested route set, build/install paths,
commands, and memory interpretation.

Validation checks route/file presence, non-empty fixtures, JSON parsing, PNG
headers/dimensions, PDF/archive signatures, normal process completion, startup
and lifecycle checkpoint order, transient-operation release flags, and
generated output presence. A per-run validation can pass for a subset of routes;
that does not complete the Phase 0 exit gate. The gate requires all 24 route
IDs on both Windows x64 and macOS universal. Pass the Windows run root and the
macOS universal evidence root together to check the complete gate:

```powershell
python scripts/phase0/validate_phase0_evidence.py `
  --evidence-root $windowsRunRoot `
  --macos-evidence-root $macosEvidenceRoot `
  --require-exit-gate
```

Use `--require-exit-gate` in automation that must reject incomplete platform or
route coverage: missing coverage returns exit code `3`; invalid evidence returns
`1`. Without it, the exit status describes the supplied run(s), not completion
of the overall gate. The strict `<250 MiB` normal-resident target is for the
future end-of-rewrite state; legacy measurements are trend-only. The temporary
512 MiB ceiling is diagnostic. Neither is a Phase 0 failure gate.

The full default route set covers empty and representative startup, all four
empty and populated language/theme variants already supported by the harness,
Classes/Sub Prep/PDF visual states, navigation/PDF lifecycle, Schedule and
Classes lifecycle, Schedule/Calendar imports, Class Transfer, Speaking
Evaluation, Staff Directory, Sub Prep output generation, and resource tracing.
External Office/PowerPoint automation is separate from the Windows offscreen
harness. Its renderer-selection reference is retained, but the current Windows
logon could not create Office output; see the
[baseline record](../../docs/qt-rewrite/phase-0-baseline.md).

## Supplemental Windows output references

Focused QtTest slots can retain generated PDFs and page previews for visual
review. This is separate from the 24-route matrix. Set
`CLASSMNGR_WINDOWS_OUTPUT_REFERENCE_DIR` to a fresh absolute directory; each
slot creates a named child directory and refuses to overwrite existing files.
The helper copies the PDF and renders its pages to opaque RGB PNGs composited
over white. A manifest records page counts, dimensions, and file sizes. The
catalog references use packaged content; roster and report examples use
synthetic test data.

Build the three test targets in a configured Windows x64 test build, then run
the capture slots directly (adjust executable paths for the chosen build
configuration):

```powershell
$testBuild = 'C:\path\to\configured\ninja-build'
$captureRoot = Join-Path $env:TEMP 'ClassMngr-output-references-new-run'
$env:CLASSMNGR_WINDOWS_OUTPUT_REFERENCE_DIR = $captureRoot

function Invoke-CaptureSlot([string]$Executable, [string]$Slot) {
  & (Join-Path $testBuild $Executable) $Slot
  if ($LASTEXITCODE -ne 0) {
    throw "$Executable $Slot failed with exit code $LASTEXITCODE."
  }
}

Invoke-CaptureSlot 'ClassMngrDocumentCatalogTests.exe' `
  'capturesVacationSubPrepCatalogPdfLifecycleWhenConfigured'
Invoke-CaptureSlot 'ClassMngrRosterTemplatePrintServiceTests.exe' `
  'dailyPdfUsesA4PortraitAndContinuesOverflowPages'
Invoke-CaptureSlot 'ClassMngrRosterTemplatePrintServiceTests.exe' `
  'perClassWithExtraInfoPdfHonorsPortraitAndLandscape'
Invoke-CaptureSlot 'ClassMngrSpeakingEvalBatchReportServiceTests.exe' `
  'internalPdfMatchesWidgetRendering'
```

Confirm each process exits `0`; the Speaking Evaluation slot runs both
standard and advanced internal-renderer cases. Use a new output root for each
capture run because existing child directories are intentionally protected.
The accepted references are under
`docs/qt-rewrite/visual-baseline/release/windows-output-reference/`.

PowerPoint COM output is a separate opt-in integration test. In this Windows
logon environment, it failed before PDF generation with `0x80070520`; no Office
PDF references were produced. Two Speaking Evaluation UI tests in the full
offscreen CTest target also failed on Windows clipboard error `0x800401d0`;
the internal PDF capture slot and focused roster slots passed.
