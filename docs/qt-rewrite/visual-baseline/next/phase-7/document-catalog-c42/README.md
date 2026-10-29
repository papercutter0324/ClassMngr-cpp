# Document catalog — Candidate #42

Task ID: `phase7_c42_document_catalog_implementation_01`.

This package contains the Next catalog sidebar in English and Korean, in light
and dark themes. The [capture manifest](document_catalog_capture.json) records
all four states, the 1024×720 viewport, 7 folder rows, 30 document rows, the
selected stable ID, resource diagnostics, and a metadata-only lifecycle sample.

| State | Capture |
|---|---|
| English · light | [PNG](document_catalog_en_US_light.png) |
| English · dark | [PNG](document_catalog_en_US_dark.png) |
| Korean · light | [PNG](document_catalog_ko_KR_light.png) |
| Korean · dark | [PNG](document_catalog_ko_KR_dark.png) |

## Capture method and limits

The screenshots come from `ClassMngrNextDocumentCatalogVisualCapture`, using
Qt 6.12.0, the offscreen platform plugin, `QT_SCALE_FACTOR=1`, Fusion, and the
packaged light/dark stylesheets. Korean uses the generated packaged translator.
The selected row is `document_guides_lesson_planning`; selection emits a
stable `DocumentId` in the focused UI test. The capture harness does not open
the selected document, so the empty content area is expected. These captures
show the Next catalog and shell chrome only. They are not a Legacy comparison
and do not claim pixel parity.

The projection comes from packaged `documents.json` through
`ResourceDocumentCatalogPort`. The manifest records zero document-body
resource diagnostics and zero `QPdfDocument` instances. The capture executable
does not link `Qt6::Pdf` or `Qt6::PdfWidgets`, and catalog construction and
sidebar rendering do not request PDF bodies. Candidate #43 owns opening,
loading, closing, and reopening a selected PDF.

## Lifecycle observation

`PlatformProcessMemorySnapshotProvider` recorded whole-process macOS working
set and private usage around an English catalog render. Values are
observations, not catalog-attributed allocations or a memory threshold.

| Stage | Working set | Private usage | Metadata rows | PDF body diagnostics |
|---|---:|---:|---:|---:|
| Before metadata projection | 37,666,816 B | 12,468,896 B | 0 | 0 |
| After catalog render | 64,815,104 B | 24,200,032 B | 7 folders + 30 documents | 0 |
| After shell and catalog release | 64,864,256 B | 24,200,032 B | 0 retained by harness | 0 |

The whole-process footprint remains high after shell destruction; Qt and the
process allocator are included in these measurements. The lifecycle sample
records zero loaded PDF bodies and zero `QPdfDocument` instances without
setting a numeric acceptance threshold.

## Ownership and parity references

Legacy remains the behavioral reference for catalog metadata parsing,
localization, nested hierarchy, ordering, and print/export flags in
`src/features/documents/document_catalog.cpp` and
`src/ui/shared/widgets/sidebar/sidebar_tree.cpp`. Its document-selection route
continues through `src/app/controllers/navigation_controller.cpp`.

Next owns the immutable packaged projection and its localized sidebar through
`src/next/platform/resource_document_catalog_port.cpp`,
`src/next/ui/document_catalog_navigation.{h,cpp}`, and
`src/next/ui/application_shell.{h,cpp}`. The shell reports a selected leaf as
a typed stable `DocumentId`; it does not put document selection in page
history. Candidate #43 owns the viewer lifecycle after that boundary.

## Reproduce

From the repository root, build and run the focused tests and capture target
in the standard macOS Debug tree:

```sh
cmake --build build/macos-clang-debug --target \
  ClassMngrNextDocumentCatalogNavigationTests \
  ClassMngrNextDocumentCatalogVisualCapture \
  ClassMngrNextBootstrapTests --parallel 2

ctest --test-dir build/macos-clang-debug --output-on-failure \
  -R '^(ClassMngrNextDocumentCatalogNavigationTests|ClassMngrNextDocumentCatalogVisualCapture|ClassMngrNextBootstrapTests|ClassMngrNextNavigationTests|ClassMngrNextSharedStaffDirectoryTests)$'

env QT_QPA_PLATFORM=offscreen QT_SCALE_FACTOR=1 \
  build/macos-clang-debug/ClassMngrNextDocumentCatalogVisualCapture \
  docs/qt-rewrite/visual-baseline/next/phase-7/document-catalog-c42
```
