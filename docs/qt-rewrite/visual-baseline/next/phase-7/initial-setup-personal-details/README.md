# Initial Setup Personal Details — Phase 7

Eight Legacy/Next captures compare the Initial Setup personal-details screen in
English (`en_US`) and Korean (`ko_KR`), in light and dark themes. Each capture
is a fixed-size 1024×720 offscreen widget render.

| Locale / theme | Legacy capture (SHA-256) | Next capture (SHA-256) |
|---|---|---|
| `en_US` / light | [PNG](legacy-initial-setup-personal-details-en_US-light.png) — `7f3d3530ecc0e5e9ec89175ff98536b50d2c9eb42176348c28638365f65f54fc` | [PNG](next-initial-setup-personal-details-en_US-light.png) — `665ddcbf9a093190cdeb768d773dc647b10febfd4bf9d5dd0c0ef81297bd425e` |
| `en_US` / dark | [PNG](legacy-initial-setup-personal-details-en_US-dark.png) — `e8b4a450efcba24b96322bf8b0195f21082c94ffb2c2369d7fe235d4bf21d2f1` | [PNG](next-initial-setup-personal-details-en_US-dark.png) — `d9cf20f9ebbe57464f2149320dd93177033909fd9a3f2d2d222bacec2ac9972c` |
| `ko_KR` / light | [PNG](legacy-initial-setup-personal-details-ko_KR-light.png) — `7f3d3530ecc0e5e9ec89175ff98536b50d2c9eb42176348c28638365f65f54fc` | [PNG](next-initial-setup-personal-details-ko_KR-light.png) — `0e271aff5fef1816f76946abf98df926e40614c8e534ce5f834a4ba474e061ac` |
| `ko_KR` / dark | [PNG](legacy-initial-setup-personal-details-ko_KR-dark.png) — `e8b4a450efcba24b96322bf8b0195f21082c94ffb2c2369d7fe235d4bf21d2f1` | [PNG](next-initial-setup-personal-details-ko_KR-dark.png) — `c1357af31458cf8c3a9d01d3ed23fedd771b40106f187c54002e33dc4d4aa9d0` |

The captures came from
`build/macos-clang-debug/phase7-setup-visual-captures.hI1s6J/` and were copied
unchanged. Next labels vary by locale. Legacy shows English labels in both
locales because this page has no Korean catalog entries.

An independent visual Tester passed this screen's review after the Next dark
signature-canvas contrast fix. Separate save/reopen and signature-processor
tests passed; signature storage remains unchanged and transparent pixel bytes
are preserved. This closes the personal-details screenshot subgate only. These
offscreen widget renders do not establish native desktop-window behavior or
close the broader 7A or Phase 7 gates.
