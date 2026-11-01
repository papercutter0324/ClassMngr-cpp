# C56 Dynamic Co-Teachers sidebar evidence

These two 1024×720 PNGs were captured by `ClassMngrNextBootstrapTests` through
the production `ApplicationBootstrap` path with a temporary workspace fixture.
The expanded Co-Teachers group displays its assigned teachers in directory
order, using preferred-name labels. The captures cover English with Light theme
and Korean with Dark theme on Qt's offscreen platform; they are examples of
those two combinations, not a complete locale/theme matrix.
If all selected name fields are empty, the row uses the existing localized
fallback: `New Teacher` in English and `새 교사` in Korean.

The corresponding CTest cases are
`ClassMngrNextCoTeacherSidebarVisualCapture_english-light` and
`ClassMngrNextCoTeacherSidebarVisualCapture_korean-dark`. Build outputs are
under `build/macos-clang-debug/co-teachers-visual-captures/`.

`capture-manifest.json` records each retained image's locale, theme, byte
length, and SHA-256 digest.
