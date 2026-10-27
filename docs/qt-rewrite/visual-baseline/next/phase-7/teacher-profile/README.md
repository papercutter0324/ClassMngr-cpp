# Teacher Profile parity captures

These matched captures compare the legacy and Next Teacher Profile pages at
1024×768 using Qt's Fusion style with `QT_QPA_PLATFORM=offscreen`. The fixed
profile data and capture metadata are in [manifest.json](manifest.json).

| Page | English / light | Korean / dark |
|---|---|---|
| Legacy | [capture](legacy_en_US-light.png) | [capture](legacy_ko_KR-dark.png) |
| Next | [capture](next_en_US-light.png) | [capture](next_ko_KR-dark.png) |

The accepted review found the header, card structure, and field layout aligned.
Residual differences are roughly 2–6 px in card vertical positions, a few
pixels at field edges, and the idle visibility of the birthday clear affordance
in the English/light pair. These feature captures do not close the 7A screenshot
comparison or Phase 7 memory and exit gates.
