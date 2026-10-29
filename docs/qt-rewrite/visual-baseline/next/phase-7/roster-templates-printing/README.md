# Roster templates and printing captures

Candidate #37 (`098f1de7`) presentation captures. All 16 PNGs use deterministic synthetic `FIXTURE` data; no real roster data or fixture PDFs are included. Captures cover English (`en_US`) and Korean (`ko_KR`) in Light and Dark themes.

| State | Size | English Light | English Dark | Korean Light | Korean Dark |
|---|---:|---|---|---|---|
| Roster template chooser | 680 × 500 | [PNG](roster_templates_en_US_light.png) | [PNG](roster_templates_en_US_dark.png) | [PNG](roster_templates_ko_KR_light.png) | [PNG](roster_templates_ko_KR_dark.png) |
| Roster preview | 900 × 760 | [PNG](roster_preview_en_US_light.png) | [PNG](roster_preview_en_US_dark.png) | [PNG](roster_preview_ko_KR_light.png) | [PNG](roster_preview_ko_KR_dark.png) |
| Testing roster template chooser | 680 × 500 | [PNG](testing_roster_templates_en_US_light.png) | [PNG](testing_roster_templates_en_US_dark.png) | [PNG](testing_roster_templates_ko_KR_light.png) | [PNG](testing_roster_templates_ko_KR_dark.png) |
| Testing roster preview | 900 × 760 | [PNG](testing_roster_preview_en_US_light.png) | [PNG](testing_roster_preview_en_US_dark.png) | [PNG](testing_roster_preview_ko_KR_light.png) | [PNG](testing_roster_preview_ko_KR_dark.png) |

These captures support human review of template selection and preview presentation across the locale/theme matrix. They do not establish strict pixel parity.

## Regular preview regression evidence

After an earlier observation of blank regular previews, five fresh capture processes produced 20 regular-preview comparisons, including 10 Dark cases. The source of the earlier observation is unknown; no production omission was reproduced. The English/Dark and Korean/Dark regular preview images contain Monday, `FIXTURE S0`, and `FIXTURE S1`: PDF text checks passed, each rendered PDF page had non-white pixels, and each visible screenshot page matched its PDF raster pixel-for-pixel. Their SHA-256 values are `737b42b3b47b700a7128a2791c655ce2934533bd825c37f37eea9a80b1212f05` (English/Dark) and `1d543808458b333c6122b2c367c5d1a9ee38ee28da69ff514c9a9f65e01d127d` (Korean/Dark).

This regression check covers those two regular Dark preview captures; it does not prove pixel parity with Legacy, physical-printer output, or broader cross-platform behavior. No full-suite, Windows, or Phase 9 memory result is claimed.
