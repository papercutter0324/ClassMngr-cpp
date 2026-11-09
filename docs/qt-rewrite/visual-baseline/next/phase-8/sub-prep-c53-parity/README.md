# Sub Prep C53 output parity by platform

The Refactoring baseline workflow captures the matched-SQL C53 package on
Linux x64, Windows x64, and macOS universal. It generates the Legacy package
from a retained SQLite snapshot, derives an independent SQL text oracle, then
runs the Next capture against the same database and Legacy PDFs.

Each host retains the prepared workspace, Legacy and Next PDFs, extracted
page text, per-page raster images, font/resource provenance, and used-code
audits under `artifacts/sub-prep-output/<report>` in the
`refactoring-baseline-<report>` workflow artifact. The capture
requires all 12 Sub Prep pages and all 5 By Day roster pages to match their
Legacy counterparts in page count, dimensions, and 150-DPI QtPdf raster pixels.
The SQL oracle verifies 750 unique weekday roster names and 30 expected class
headers without reading either PDF. Both embedded font resources must have
matching hashes in the Legacy and Next paths.

Raster comparisons are performed within each host. The results describe
same-host Legacy/Next parity; they do not compare pixels between operating
systems. Host OS, architecture, Qt version, output hashes, and page metrics are
recorded in the comparison manifest.

## Reproduce locally

Build `ClassMngr`, `ClassMngrStartupPerformanceTests`, and
`ClassMngrNextSubPrepSqlParityCapture` in a supported standard Debug build
folder, then run:

```sh
QT_QPA_PLATFORM=offscreen \
QT_LINUX_PREFIX="$PWD/Qt/6.12.0/gcc_64" \
python scripts/phase8/capture_sub_prep_c53_parity.py \
  --build-dir build/linux-gcc-debug \
  --report linux-gcc-debug \
  --output-dir /tmp/classmngr-c53-linux
```

Set `QT_LINUX_PREFIX`, `QT_MSVC_X64_PREFIX`, or `QT_MACOS_PREFIX` to the
matching Qt 6.12.0 kit root on Linux, Windows, or macOS. The script requires
that kit's platform plugins and configures the Qt plugin and runtime library
paths. On Windows, use PowerShell syntax for the environment variable and
command. The output directory must be new or empty; successful captures are
written to `artifacts/sub-prep-output/<report>` by default.

## Linux x64 Debug evidence

The candidate-2 Linux capture passed all 17 Sub Prep/C53 pages with zero
changed pixels. The independent SQL oracle verified 750 unique weekday roster
names and 30 class headers, and the strict used-code audit passed. This uses
the valid Qt 6.12 release composition of Qt Core 6.12 with QtPdf 6.14. Windows
x64, macOS universal, and hosted
exact-toolchain captures remain pending.
