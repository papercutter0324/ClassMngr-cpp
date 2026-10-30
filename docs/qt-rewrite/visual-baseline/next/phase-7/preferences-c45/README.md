# Next Preferences C45 Visual Review

Captured the production `Edit → Preferences…` action in the bootstrapped Next
shell, then captured the dialog with its Appearance tab selected.

- Window: fixed 680 × 620 px.
- Scale: `QT_SCALE_FACTOR=1`; captured with Qt's offscreen platform.
- Reviewed: translated tab and field labels, Close button, theme/language/font
  selections, light/dark contrast, and control visibility without clipping.
- The menu-route interaction test also checks visible English and Korean labels
  on Documents and AI Comments, plus both localized Custom Website summaries.

| Locale | Theme | Capture | Review |
| --- | --- | --- | --- |
| English | Light | [english-light.png](english-light.png) | Light controls and English labels visible. |
| English | Dark | [english-dark.png](english-dark.png) | Dark controls and English labels visible. |
| Korean | Light | [korean-light.png](korean-light.png) | Light controls and Korean labels visible. |
| Korean | Dark | [korean-dark.png](korean-dark.png) | Dark controls and Korean labels visible. |

Each capture shows the theme and language matching its row and the `Normal`
font-size option. The selected Appearance tab and Close button remain visible in
all four images.
