# Candidate #34 Calendar import captures

Task ID: `phase7-c34-documentation-20261028`.

This package contains the 20 Preferences/import-state captures from the
candidate #34 build manifest. They cover English/Korean × Light/Dark and
Preferences, Loading, Success, Error, and Canceled. Each image is 720×720.
The four #33 Calendar page captures remain in the parent [`next/`](../next/)
package and are not duplicated here.

| Language | Theme | Preferences | Loading | Success | Error | Canceled |
|---|---|---|---|---|---|---|
| English | Light | [image](calendar-preferences-en-light.png) | [image](calendar-import-loading-en-light.png) | [image](calendar-import-success-en-light.png) | [image](calendar-import-error-en-light.png) | [image](calendar-import-canceled-en-light.png) |
| English | Dark | [image](calendar-preferences-en-dark.png) | [image](calendar-import-loading-en-dark.png) | [image](calendar-import-success-en-dark.png) | [image](calendar-import-error-en-dark.png) | [image](calendar-import-canceled-en-dark.png) |
| Korean | Light | [image](calendar-preferences-ko-light.png) | [image](calendar-import-loading-ko-light.png) | [image](calendar-import-success-ko-light.png) | [image](calendar-import-error-ko-light.png) | [image](calendar-import-canceled-ko-light.png) |
| Korean | Dark | [image](calendar-preferences-ko-dark.png) | [image](calendar-import-loading-ko-dark.png) | [image](calendar-import-success-ko-dark.png) | [image](calendar-import-error-ko-dark.png) | [image](calendar-import-canceled-ko-dark.png) |

The [filtered manifest](calendar-import-captures.json) maps these images to
their language, theme, and state. The build source is
`build/macos-clang-debug/calendar-captures/calendar-captures.json`; the capture
selector is `ClassMngrNextCalendarPageTests
captureEnglishAndKoreanLightAndDark`.

No clipping was observed. The success capture shows 4 imported/1 skipped;
integration tests separately verify 3 imported/2 skipped when one duplicate
already exists. Korean status prefixes are translated; the detailed parser
error remains English as inherited Legacy behavior. These captures are
presentation evidence, not pixel-parity evidence. Whole-process memory
observations do not establish a per-import budget or Phase 9 acceptance.
