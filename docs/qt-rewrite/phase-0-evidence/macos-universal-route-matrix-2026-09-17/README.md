# macOS universal 24-route run

This record covers the complete packaged Release Phase 0 matrix run on
2026-09-17. The host was macOS 27.0 on Apple Silicon. The fresh Release app
and Debug test harness were built for `arm64` and `x86_64`, with a macOS 14.4
deployment target, then audited before the final route run.

## Result

- All 24 route IDs completed: 1 fixture, 8 visual, 8 memory, 1 workflow, and
  6 output routes.
- The final run recorded 27 commands; each exited 0 without timing out. The
  macOS universal validator passed with zero failures and all 151 of 151
  required artifacts present.
- The run contains 70 JSON files, 84 PNGs, 27 PDFs, and one ZIP. The resource
  trace and lifecycle checks passed.
- The package audit covered 116 embedded Mach-O files. The app and test
  harness each contain `arm64` and `x86_64` slices with a 14.4 minimum OS
  version. The packaged app signature verified.
- The validator reported one memory trend warning: 404 of 1,433 samples were
  at or above the legacy 250 MiB comparison. The maximum compared working
  set was 516,521,984 bytes (492.594 MiB); no sample reached the temporary
  512 MiB diagnostic ceiling. The memory thresholds are not Phase 0 failure
  gates.

The platform-specific validation passed. Its combined exit-gate field remains
incomplete with Windows x64 pending because the validator was supplied only
the macOS run; the original Windows raw evidence root is not available on this
host.

The toolchain was Xcode 27.0, macOS SDK 27.0, Qt 6.12.0 universal, CMake
4.3.3, Ninja 1.13.2, and Python 3.14.4. The source commit was
`c5dd6e6b96f630d776165b9dbf61917ad743f6dd`.

## Retained evidence

The full successful run root is
`/private/tmp/classmngr-qt0-macos-universal-20260917T-run2Z`. It contains 643
regular files totaling 462,599,685 bytes and 156 symlinks. The SHA-256
inventory covers every regular file; the companion symlink inventory records
each link target. The compact bundle contains byte-identical copies of the
run manifest, validation summary, package provenance, and the source build's
packaging audit summary.

The clean build and package audit were produced in
`/private/tmp/classmngr-qt0-macos-universal-20260917T1403Z`. The final run
reused those audited artifacts and verified the package signature again. The
first route attempt in that build root could not start its two Calendar
Import test servers because the default shell sandbox blocks loopback socket
binding. The final run reran the complete matrix with local loopback access;
both Calendar Import routes passed. The source run manifest and its validation
summary are included to preserve that build and retry provenance.

To revalidate the successful run while its full root remains available, run
the validator without an output-file option:

```sh
python3 scripts/phase0/validate_phase0_evidence.py \
  --evidence-root /private/tmp/classmngr-qt0-macos-universal-20260917T-run2Z \
  --expected-platform macos-universal
```

The committed bundle is an audit record, not a complete validator evidence
root. Visual and generated-output semantic review remains a separate human
review step.
