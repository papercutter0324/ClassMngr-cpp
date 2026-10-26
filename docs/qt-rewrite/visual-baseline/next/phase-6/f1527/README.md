# Phase 6 Next shell appearance captures

These client-area images show the workspace-closed Next shell on the
`campus_information` placeholder route. Each capture uses a 1024×720 window,
1× scale, Normal font preference, and keyboard focus on the selected
Information navigation item. Campus Directory is expanded; Useful Links is
collapsed. Back and Forward are disabled because the initial route has no
history. Placeholder routes intentionally have no icons.

| Language | Light | Dark |
|---|---|---|
| English (`en_US`) | [Image](shell-en_US-light.png) | [Image](shell-en_US-dark.png) |
| Korean (`ko_KR`) | [Image](shell-ko_KR-light.png) | [Image](shell-ko_KR-dark.png) |

[manifest.json](manifest.json) records the source revision/state, resolved
stylesheet and compiled translation hashes, Qt/platform, font, size, scale,
route, focus target, and selection for each image. The revision is the base
commit used with the F1527 implementation working tree. These are Debug Qt
offscreen review images; they are not native-window or pixel-equality CI
baselines. Full feature rendering and cross-platform parity remain later
phase gates. The existing legacy reference images are retained separately.

The capture review exposed missing Korean translation contexts, clipped
English placeholder text, and a Next navigation tree that missed the shared
sidebar border rule. The final images include the explicit Next translation
contexts, a full-width centered description, and the existing sidebar frame
(2px solid border, 8px radius, and shared theme colors). Typography, margins,
spacing, palette, and selection colors are
unchanged.

From the repository root, the capture command is:

```sh
cmake --build build/macos-clang-debug --target ClassMngrNextBootstrapTests -j 4
env QT_QPA_PLATFORM=offscreen QT_SCALE_FACTOR=1 \
  CLASSMNGR_NEXT_APPEARANCE_CAPTURE_DIR=docs/qt-rewrite/visual-baseline/next/phase-6/f1527 \
  CLASSMNGR_NEXT_APPEARANCE_REVISION=a402bba5393c99466432f9f4986699ffb31a7278 \
  CLASSMNGR_NEXT_APPEARANCE_SOURCE_STATE='F1527 implementation working tree' \
  build/macos-clang-debug/ClassMngrNextBootstrapTests \
  shellAppearanceUsesSelectedThemeAndLanguage
```

Set the revision/state values to match the source being captured when
regenerating. The four data cases also verify actual translated shell text,
theme loading, selected route/item, focus, icon absence, description fit, and
history action state. Ordinary CTest runs exercise those assertions without
writing images unless the capture output environment variable is supplied.
SystemDefault theme mapping is verified by the focused ThemePresenter tests
with controlled light and dark scheme providers.
