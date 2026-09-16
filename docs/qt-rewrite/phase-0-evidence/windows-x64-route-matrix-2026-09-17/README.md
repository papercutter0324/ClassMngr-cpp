# Windows x64 24-route run

This directory preserves the machine summary and integrity inventory for the
fresh Windows x64 Phase 0 route run completed on 2026-09-17. The full run root
was created at:

`C:\Users\wfelt\AppData\Local\Temp\ClassMngr-QT0-Windows-x64-final-20260916T201256Z\qt0-windows-x64-final-20260916T201256Z-msvc`

The original root contains 331 files totaling 147,732,123 bytes. It remains in
the local temporary directory and is not copied into Git. The TSV inventory
lists each original relative path, byte size, and SHA-256. This compact bundle
does not contain the route artifacts and cannot itself be passed to the
validator as an evidence root. To revalidate the run, use the complete original
root while it remains available:

```powershell
python scripts/phase0/validate_phase0_evidence.py `
  --evidence-root 'C:\Users\wfelt\AppData\Local\Temp\ClassMngr-QT0-Windows-x64-final-20260916T201256Z\qt0-windows-x64-final-20260916T201256Z-msvc'
```

## Result

- All 24 route IDs completed: 1 fixture, 8 visual, 8 memory, 1 workflow, and
  6 generated-output routes.
- All 30 commands exited 0 without timing out: five build/package commands,
  24 route commands, and one final validation command.
- The per-run validator passed with zero failures and all 151 of 151 required
  files present. It reported one warning: legacy memory samples exceeded the
  250 MiB trend comparison. The maximum working set was 496,005,120 bytes;
  no sample exceeded the diagnostic 512 MiB ceiling.
- The resource trace has 189 entries and records processFinished=true,
  exitCode=0, and timedOut=false.
- The overall Phase 0 exit gate remains incomplete because the macOS universal
  24-route run is pending. Windows ARM64 and Linux remain deferred ports.

The retained run manifest and validation summary are byte-identical copies of
the original files. The full generated-output references for catalog PDFs,
roster layouts, and Speaking Evaluation reports are retained separately under
`docs/qt-rewrite/visual-baseline/release/windows-output-reference/`.
