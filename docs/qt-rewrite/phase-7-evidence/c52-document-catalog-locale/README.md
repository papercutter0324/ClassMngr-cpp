# C52 Document Catalog locale capture evidence

These captures exercise the production `ApplicationBootstrap` Preferences
language switch while the packaged `document_guides_lesson_planning` PDF is
open in the Documents viewer. The three states show English before the switch,
Korean after switching, and English restored. Each image is 1024×720 and was
captured with Qt 6.12.0 using the offscreen platform.

`capture-manifest.json` records the root, folder, selected row, viewer title,
visible controls, selected document ID, content reference, PDF readiness,
current page, zoom, and collapsed `document_training` folder for every image.
The viewer retained the same page and PDF objects, ready content snapshot,
active stream, page 2, and 144% zoom through both changes. Starting from the
one successful initial typed selection, the test observed zero later body-open
lifecycle events, tree selection signals, or PDF status signals during the
locale switches. The PDF viewport is not used as a pixel-rendering assertion in
this capture; the adjacent
`ClassMngrNextDocumentViewerVisualCapture` test covers rendered PDF states.

The capture files were produced by
`ClassMngrNextBootstrapTests::preferencesRouteRetranslatesActiveDocumentCatalogAndViewer`
with:

```sh
env QT_QPA_PLATFORM=offscreen QT_SCALE_FACTOR=1 \
  CLASSMNGR_NEXT_DOCUMENT_CATALOG_LOCALE_CAPTURE_DIR="$PWD/build/macos-clang-debug/document-catalog-locale-captures" \
  ./build/macos-clang-debug/ClassMngrNextBootstrapTests \
  preferencesRouteRetranslatesActiveDocumentCatalogAndViewer
```

The CTest regressions were `ClassMngrNextDocumentCatalogNavigationTests`,
`ClassMngrNextDocumentViewerTests`, and
`ClassMngrNextDocumentViewerVisualCapture`.
