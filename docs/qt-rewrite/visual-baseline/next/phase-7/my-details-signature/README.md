# My Details and Signature captures

Source commit: `060aca5c`.

The package contains the eight matched images and manifest from the standard
`build/macos-clang-debug/my-details-signature-visual-capture/` output. The
fixture is Alex Morgan in Bundang with both saved signature modes. Captures use
Fusion/offscreen at 1024×768 in English/light and Korean/dark, for Image and
Type modes. Main and the independent Tester reviewed all captures after the
footer repair.

| Locale/theme and mode | Legacy | Next |
|---|---|---|
| English / light, Image | [PNG](legacy_en_US-light_image.png) | [PNG](next_en_US-light_image.png) |
| English / light, Type | [PNG](legacy_en_US-light_type.png) | [PNG](next_en_US-light_type.png) |
| Korean / dark, Image | [PNG](legacy_ko_KR-dark_image.png) | [PNG](next_ko_KR-dark_image.png) |
| Korean / dark, Type | [PNG](legacy_ko_KR-dark_type.png) | [PNG](next_ko_KR-dark_type.png) |

The normal-workspace My Details and Signature page is separate from Initial
Setup. Normal-workspace empty-name behavior matches legacy, while Initial
Setup's existing validation stays unchanged. Image and typed signatures remain
compatible with saved data; roundtrip and removal are covered. The dirty/save
flow covers automatic and manual save, full-bundle save, failure and retry,
dirty navigation, report readback, and workspace replacement. In normal clean
and dirty states, Next has no persistent footer or page-level Save button.

Accepted visual differences are controls becoming visible about 10–20 px
earlier in Next and more Korean shell/tab localization. Typed previews visibly
render in all four Next captures.

## Verification and limits

After the footer repair, focused CTest passed 5/5, page tests 14/14, and two
targeted Bootstrap scenarios 4/4. CMake source ownership validated 1,782
handwritten sources; resource inventory and `git diff --check` passed. Earlier
focused functional recheck passed CTest 8/8, page tests 13/13, and selected
Bootstrap cases 7/7. No full suite or Windows run was verified. No
EXIF-orientation-specific fixture or exact debounce-timing assertion was run.

The manifest's whole-process macOS snapshots span working set 70,860,800 to
109,772,800 bytes (70.9–109.8 decimal MB; 67.6–104.7 MiB) and peak private
usage 46,613,440 bytes (46.6 MB; 44.5 MiB). They include Qt, the legacy runtime,
loaded resources, and both surfaces. These are lifecycle observations, not
page-specific allocations or a memory budget. The 7A screenshot comparison
and memory evidence and the 7A/Phase 7 exit gates remain open.
