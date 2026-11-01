# Whole-profile file workflow captures

This package adds visible Legacy/Next evidence for the Phase 7A whole-profile
file actions. It captures 18 matched state pairs (9 states in each of two
appearance variants), plus two Next-only captures of the New Profile
confirmation. The paired screenshots are 1024×720. The generator writes
`capture-manifest.json` with image dimensions, byte sizes, SHA-256 digests, and
the synthetic fixture file metadata.

## Captured states

| State | What is visible |
|---|---|
| `new-replacement` | New profile save dialog with the existing synthetic `profile.tps` selected as the replacement target. Next also retains its preceding New Profile confirmation as a supplemental capture. |
| `open-profile` | Open dialog with the shared synthetic `.tps` profile selected. |
| `dirty-close-resolution` | Close action while the My Workspace name field contains the same unsaved synthetic draft in both applications. |
| `save-as` | Save As dialog with a synthetic copy path selected. |
| `export` | Export dialog with a synthetic export path selected. |
| `recent-populated` | The production File menu and populated Recent Files submenu. |
| `recent-cleared` | The same menus after Clear Recent Files leaves the empty history state visible. |
| `recovery-selection` | Recovery action with a synthetic interrupted-create recovery candidate. |
| `schema-v6-db-open` | Open dialog with the Legacy Teacher Profile `.db` filter and the schema-v6 `.db` fixture selected. |

Every paired workflow uses one disposable synthetic profile path for Legacy
and Next. The generator creates the profile with Legacy
`ApplicationServices::openDatabase`, adds one synthetic teacher and the name
`Synthetic Profile`, then copies it to the `.db`, Recent Files, and recovery
fixture paths. The `.db` input has the same schema-v6 content as the `.tps`
profile. No user profile or external file is read. The temporary workspace and
settings directories are removed when the capture process exits.

The save/open dialogs are actual production Qt presenters invoked by their
production shell actions; the harness selects a deterministic path and
cancels before changing the profile. The dirty-close and recovery screenshots
show the actual production prompt surfaces and cancel them after capture. The
recent-history screenshots show the production menus and exercise the Clear
action. The extra Next confirmation capture documents a screen with no direct
Legacy equivalent; Legacy reaches its New Profile file dialog without that
confirmation. The matched New/replacement comparison therefore shows the
selected existing destination, while the supplemental image records Next's
additional step.

## Appearance variants and review

- English / light: `en_US` locale and light theme.
- Korean / dark: `ko_KR` locale and dark theme.
- Legacy and Next images have the same viewport and the same synthetic file
  paths within each pair.
- These captures support visual review. They do not claim pixel parity, and
  this package does not make a measured pixel comparison.

Visual review confirms the requested filenames are selected in the open and
save dialogs, both Recent Files menus show the three synthetic entries and the
empty state after clearing, and the interrupted-create candidate is visible in
both recovery surfaces. The file dialogs and production shells differ in
layout and control sizing. The Legacy `.db` capture uses a separate
`Legacy Teacher Profile (*.db)` filter; Next shows `.tps` and `.db` together
in its profile filter. The dirty-close capture has Legacy's My Details tab and
Next's My Schedule tab behind the prompt, although the same synthetic My
Workspace name draft is dirty in both.

The Korean/dark captures also show translation differences: Legacy translates
the dirty-close prompt and its file filters, while Next leaves the dirty-close
and new-profile confirmation text in English. Both applications display the
interrupted-create recovery message in English. The supplemental New Profile
confirmation has no matching Legacy screen; Legacy opens its new-profile file
dialog directly.

The package uses `QT_QPA_PLATFORM=offscreen` and Qt's Fusion style. macOS
native file panels are not rendered by the offscreen platform, so the visible
dialogs here are the production Qt widget dialogs. This does not cover native
AppKit panel framing, system navigation controls, or window-manager effects.

## Reproduce

From the repository root, build and run only the dedicated capture target:

```sh
cmake --build build/macos-clang-debug --target ClassMngrProfileFileWorkflowVisualCapture
ctest --test-dir build/macos-clang-debug --output-on-failure -R '^ClassMngrProfileFileWorkflowVisualCapture$'
```

CTest writes the PNGs and manifest into this directory. The capture target is
not a substitute for the existing behavior tests, and the full test suite is
not required to reproduce this evidence package.
