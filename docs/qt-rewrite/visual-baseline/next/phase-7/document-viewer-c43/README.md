# Document viewer — Candidate #43

Task ID: `phase7_c43_document_viewer_implementation_01`.

This evidence package captures the Next document viewer in English and Korean,
with light and dark themes. Each combination has ready, missing-selection
error, closed, and reopened states at 1024×720. The [capture manifest](document_viewer_capture.json)
records state, process measurements, active stream/session state, and PDF
wrapper counts.

| State | English · light | English · dark | Korean · light | Korean · dark |
|---|---|---|---|---|
| Ready | [Next](document_viewer_en_US_light_ready.png) · [Legacy](legacy_document_viewer_en_US_light_ready.png) | [Next](document_viewer_en_US_dark_ready.png) · [Legacy](legacy_document_viewer_en_US_dark_ready.png) | [Next](document_viewer_ko_KR_light_ready.png) · [Legacy](legacy_document_viewer_ko_KR_light_ready.png) | [Next](document_viewer_ko_KR_dark_ready.png) · [Legacy](legacy_document_viewer_ko_KR_dark_ready.png) |
| Error | [Next](document_viewer_en_US_light_error.png) · [Legacy](legacy_document_viewer_en_US_light_error.png) | [Next](document_viewer_en_US_dark_error.png) · [Legacy](legacy_document_viewer_en_US_dark_error.png) | [Next](document_viewer_ko_KR_light_error.png) · [Legacy](legacy_document_viewer_ko_KR_light_error.png) | [Next](document_viewer_ko_KR_dark_error.png) · [Legacy](legacy_document_viewer_ko_KR_dark_error.png) |
| Closed | [Next](document_viewer_en_US_light_closed.png) · [Legacy](legacy_document_viewer_en_US_light_closed.png) | [Next](document_viewer_en_US_dark_closed.png) · [Legacy](legacy_document_viewer_en_US_dark_closed.png) | [Next](document_viewer_ko_KR_light_closed.png) · [Legacy](legacy_document_viewer_ko_KR_light_closed.png) | [Next](document_viewer_ko_KR_dark_closed.png) · [Legacy](legacy_document_viewer_ko_KR_dark_closed.png) |
| Reopened | [Next](document_viewer_en_US_light_reopened.png) · [Legacy](legacy_document_viewer_en_US_light_reopened.png) | [Next](document_viewer_en_US_dark_reopened.png) · [Legacy](legacy_document_viewer_en_US_dark_reopened.png) | [Next](document_viewer_ko_KR_light_reopened.png) · [Legacy](legacy_document_viewer_ko_KR_light_reopened.png) | [Next](document_viewer_ko_KR_dark_reopened.png) · [Legacy](legacy_document_viewer_ko_KR_dark_reopened.png) |

## Selection and content lifecycle

The #42 typed `DocumentId` selection callback is bound at the application
orchestration boundary. Its C43 route coordinator activates the registered
viewer page, obtains that typed page instance, reads the current catalog
projection, and passes the selected ID to `DocumentViewerPage::openDocument`.
The page resolves the ID through `DocumentCatalogUseCase`, requests a
tokenized `DocumentContentSession`, and opens only the selected primary PDF
reference as a file-backed stream. Startup and the catalog sidebar remain
metadata-only.

Replacement, close, folder selection, suspension, and page release close the
`QPdfDocument` to `Status::Null` before releasing the borrowed stream and
content session. Replacement opens the next session only after the previous
release. A retained page may keep its empty `QPdfDocument` and `QPdfView`
wrappers; page release destroys them. Selecting the same document after close
opens a fresh stream and session. The focused lifecycle test observes close,
stream-release, and session-release ordering and checks viewer-owned resource
counts.

The page preserves one-based clamped navigation, zoom in/out and percentage
entry, Fit Width, Fit Page, print capability from catalog metadata, and the
optional PDF/Office save-copy action. Fit tests check computed width/page
factors and the Custom mode used by the Legacy fit calculation; reopening
resets to page 1 at 100%. Copying uses bounded 1 MiB reads into a `QSaveFile`,
including PDF exports. Tests cover PDF and PPTX copies, dialog cancellation,
preservation of an existing destination when the selected export source fails,
and the optional open-after-save action. A narrow print-handler seam verifies
that the action delegates only for printable entries.

If QtPdf does not reach `Status::Null` after close, the page destroys the view
and document while the borrowed stream remains alive, then releases the stream
and session and recreates empty wrappers. A deterministic close-wait seam
drives this path for ordinary close and suspension tests without waiting five
seconds. Those tests check consumer-before-stream ordering, a Released session
with no retained reference, zero active viewer-owned streams, and a fresh
stream/session on reopen. The page keeps the same fallback ordering in its
destructor.

## Process and resource observations

`PlatformProcessMemorySnapshotProvider` sampled whole-process memory and
thread counts. The table shows the English/light sequence. The manifest has
the same stages for each locale/theme combination; its body-diagnostic count
is per combination, while the top-level total is cumulative for the capture
process.

| Stage | Working set | Private usage | Threads | Active primary streams | `QPdfDocument` wrappers | Body opens in combination |
|---|---:|---:|---:|---:|---:|---:|
| Baseline, before shell | 39,043,072 B | 12,550,816 B | 3 | 0 | 0 | 0 |
| Ready after open | 77,496,320 B | 30,983,080 B | 9 | 1 | 1 | 1 |
| Missing-ID error | 80,723,968 B | 29,606,824 B | 9 | 0 | 1 | 1 |
| Closed | 80,723,968 B | 29,606,824 B | 9 | 0 | 1 | 1 |
| Ready after reopen | 80,920,576 B | 34,194,344 B | 9 | 1 | 1 | 2 |
| After page release | 80,936,960 B | 29,541,288 B | 8 | 0 | 0 | 2 |

The two body opens are the original selection and the reselect. A missing ID
does not open a body. The capture records zero active viewer-owned streams
after error, close, and page release, with the PDF status `Null` after error
and close. After page destruction, no viewer page or PDF wrapper remains.
Whole-process memory includes Qt and allocator state and remains elevated
after page release. The macOS provider reports a zero process handle count in
this environment; thread count is recorded. These are observations without a
numeric threshold. `ResourceLoader` has no global active-stream counter, so
active stream counts here are the viewer-owned handle state.

## Capture method and limits

`ClassMngrNextDocumentViewerVisualCapture` uses Qt 6.12.0, the offscreen
platform plugin, `QT_SCALE_FACTOR=1`, Fusion, and the packaged light/dark
stylesheets. Korean uses the packaged translator. It waits briefly after the
document reaches Ready so the asynchronous QtPdf page render appears in the
capture. The error state selects an unknown stable `DocumentId`; the focused
UI test separately covers a corrupt PDF body and confirms no stale stream or
session remains.

The paired comparison target is
`ClassMngrNextLegacyDocumentViewerVisualCapture`. It captures Legacy
`PdfViewerPage` Ready, Error, Closed, and Reopened states under the same Qt
6.12.0 offscreen/Fusion process settings, `QT_SCALE_FACTOR=1`, 1024×720 widget
viewport, packaged fonts, translator, and light/dark stylesheets. It writes
`legacy_document_viewer_<locale>_<theme>_<state>.png` plus
`legacy_document_viewer_capture.json` beside the Next artifacts.

The Legacy harness instantiates the production `PdfViewerPage` directly. It
does not include the full Legacy `MainWindow`; Next captures include the full
Next shell and viewer. Legacy print/export are application actions and it has
no in-page Close button, while Next places Print, Export, and Close in the
viewer action row. The captures show these structural differences and state
handling; they do not claim pixel parity. Both sets were regenerated after
the Korean Close-label repair. Each manifest records 16 captures: four states
for each of four locale/theme combinations. The Korean Next ready and reopened
captures show the translated Close label `닫기`.

Qt's QML import scan remains enabled. `ClassMngrNext` is declared in
`cmake/next_app`, giving qmlimportscanner that narrow source root, while the
calendar QRC still explicitly lists `EventCalendar.qml` and
`MonthGridDelegate.qml`. The standard macOS Debug scan retains QtQuick,
QtQuick.Controls, and QtQuick.Layouts imports and contains no Release-stage or
KDE resource paths. The executable and autogen outputs remain at their prior
build-root locations.

The capture harnesses do not invoke a physical printer or interactive save
dialog. The focused UI test covers the open-after-save branch through a fake
URL opener; screenshots show capability state only.

## Reproduce

Configure the standard `macos-clang-debug` tree with Qt 6.12 selected through
`QT_MACOS_PREFIX`. From the repository root, build the focused targets, run
their CTests, then write the paired captures into this directory:

```sh
cmake --build build/macos-clang-debug --target \
  ClassMngrNext \
  ClassMngrNextDocumentViewerTests \
  ClassMngrNextDocumentViewerVisualCapture \
  ClassMngrNextLegacyDocumentViewerVisualCapture --parallel 6

ctest --test-dir build/macos-clang-debug --output-on-failure --parallel 1 \
  -R '^(ClassMngrNextDocumentViewerTests|ClassMngrNextDocumentViewerVisualCapture|ClassMngrNextLegacyDocumentViewerVisualCapture)$'

env QT_QPA_PLATFORM=offscreen QT_SCALE_FACTOR=1 \
  build/macos-clang-debug/ClassMngrNextDocumentViewerVisualCapture \
  docs/qt-rewrite/visual-baseline/next/phase-7/document-viewer-c43

env QT_QPA_PLATFORM=offscreen QT_SCALE_FACTOR=1 \
  build/macos-clang-debug/ClassMngrNextLegacyDocumentViewerVisualCapture \
  docs/qt-rewrite/visual-baseline/next/phase-7/document-viewer-c43
```
