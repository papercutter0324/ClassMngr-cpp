# Phase 8 Schedule output parity matrix

The `ClassMngrNextNormalScheduleImportIntegrationTests` evidence capture
generates Legacy and Next PDFs from the same imported C51 fixture on the
current host. It verifies the expected profile and four course labels in both
documents, one landscape A4 page, and renders each first page at 150 DPI.
`output-evidence.json` records the host, Qt version, PDF hashes, text, page
geometry, and raster measurements. The PDF and PNG files are kept with the
workflow artifact; the temporary workspace is not retained by the capture.

## Rendering tolerance

The Legacy and Next images from one platform may differ by at most 0.1% of
pixels and a mean absolute RGB channel difference of 0.5 on the 0–255 scale.
The checks run on the same QtPdf renderer and font stack, so the tolerance
allows small antialiasing variation while the text and page-geometry checks
continue to catch content and layout changes.

The archived Phase 7 macOS image is also compared and its measurements are
recorded when its dimensions match, but it is not a cross-platform pixel gate.
The requested font was unavailable on the original capture host, and each
operating system can use a different fallback face and QtPdf rasterizer. The
per-platform Legacy/Next comparison is the parity check for Linux, Windows
x64, and macOS.

## Local reproduction

After configuring the standard Linux Debug preset and building the integration
target:

```sh
cmake --build build/linux-gcc-debug \
  --target ClassMngrNextNormalScheduleImportIntegrationTests --parallel 2

CAPTURE_DIR="$(mktemp -d)"
CLASSMNGR_C51_SCHEDULE_OUTPUT_DIR="$CAPTURE_DIR" \
QT_QPA_PLATFORM=offscreen QT_SCALE_FACTOR=1 \
  ctest --test-dir build/linux-gcc-debug \
    -R '^ClassMngrNextNormalScheduleImportIntegrationTests$' \
    --output-on-failure
```

The Refactoring baseline workflow enables this capture in its full CTest run
on Linux x64, Windows x64, and macOS universal Debug runners, then uploads each
platform's PDFs, PNGs, and JSON evidence in its baseline artifact.

## Linux x64 Debug evidence

The candidate-2 local capture passed on Fedora Linux 44 x86_64 with Qt 6.12.0;
the valid release composition uses Qt Core 6.12 with QtPdf 6.14. Legacy and
Next produced one landscape A4 page, both passed all five content checks, and
their 1754×1240 renders had zero changed pixels and zero mean channel
difference. Focused Schedule CTest passed 1/1; the independent focused
Schedule/Roster/Speaking CTest set passed 3/3. Against the archived macOS image,
this Linux render had 140,929
changed pixels and mean channel difference 9.4227; that measurement confirms
why the archived image is retained as reference data instead of a cross-host
pixel threshold.

Windows x64, macOS universal, and hosted exact-toolchain captures remain
pending.
