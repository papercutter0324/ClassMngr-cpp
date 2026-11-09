# Speaking Evaluation report output by adapter path

The refactoring baseline runs matched synthetic `StudentReport` inputs through
the Legacy `SpeakingEvalBatchReportService::exportReports()` entry point and
the Next `Output::runSpeakingEvaluationReports()` entry point. It captures both
Standard and Advanced reports on Linux x64, Windows x64, and macOS universal.

This is matched-data adapter-path output evidence. Both paths reach the same
`exportResolvedReports()` implementation, `SpeakingEvalInternalPdfRenderer`,
and `SpeakingEvalReportPainter`; this does not compare independent renderers or
claim independent Legacy/Next layouts. PDF byte equality is not a gate, and
pixels are compared only between the two paths on the same host.

For each template, the test requires one PDF page per path, matching extracted
text, the expected report text, 540 × 780 point geometry, and exact pixel
equality after Qt PDF rendering at 150 DPI. The evidence includes both PDFs,
both raster PNGs, and `manifest.json` with host and Qt metadata, page metrics,
text, and the scope limitation. CI uploads each host's directory inside its
`refactoring-baseline-<report>` artifact at
`artifacts/speaking-evaluation-output-parity/<report>`.

## Reproduce locally

Configure and build the `ClassMngrSpeakingEvalBatchReportServiceTests` target
in a supported Debug build. Run the focused test with an empty or new output
directory:

```sh
mkdir -p /tmp/speaking-evaluation-output-parity
CLASSMNGR_SPEAKING_EVALUATION_OUTPUT_PARITY_DIR=/tmp/speaking-evaluation-output-parity \
QT_QPA_PLATFORM=offscreen \
build/linux-gcc-debug/ClassMngrSpeakingEvalBatchReportServiceTests \
  legacyAndNextOutputPathsMatchForStandardAndAdvanced
```

The test fails if the requested output directory already contains files. The
capture uses synthetic fixture values and does not contain student records.

## Linux x64 Debug evidence

The candidate-2 Linux capture passed for Standard and Advanced: extracted text,
540 × 780-point geometry, and 150-DPI raster pixels were equal between the
Legacy and Next paths. This uses the valid Qt 6.12 release composition of Qt
Core 6.12 with QtPdf 6.14. Windows x64, macOS universal, and hosted
exact-toolchain captures remain pending; the evidence remains same-host
adapter-path parity because both paths share the renderer.
