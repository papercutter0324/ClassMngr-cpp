# Speaking-evaluation template artwork

The files in this directory are the authoring sources for ClassMngr's internal
speaking-evaluation renderer. The SVG maps use a `0 0 540 780` view box where
one map unit is one typographic point (`1/72` inch). The physical report page is
therefore 7.5 by 10.833333 inches.

Run the generator from the repository root after replacing artwork:

```powershell
pwsh -NoProfile -File scripts/speaking_eval/generate_internal_template_assets.ps1
```

Verify that the committed runtime assets match the sources:

```powershell
pwsh -NoProfile -File scripts/speaking_eval/generate_internal_template_assets.ps1 --check
```

## High-resolution replacements

Replace these 24 placeholder PNGs without renaming them:

### Standard

1. `standard/background-clean.png`
2. `standard/score-label-aplus.png`
3. `standard/score-label-a.png`
4. `standard/score-label-bplus.png`
5. `standard/score-label-b.png`
6. `standard/score-label-c.png`
7. `standard/overall-grade-aplus.png`
8. `standard/overall-grade-a.png`
9. `standard/overall-grade-bplus.png`
10. `standard/overall-grade-b.png`
11. `standard/overall-grade-c.png`
12. `standard/overall-grade-na.png`

### Advanced

13. `advanced/background-clean.png`
14. `advanced/score-label-aplus.png`
15. `advanced/score-label-a.png`
16. `advanced/score-label-bplus.png`
17. `advanced/score-label-b.png`
18. `advanced/score-label-c.png`
19. `advanced/overall-grade-aplus.png`
20. `advanced/overall-grade-a.png`
21. `advanced/overall-grade-bplus.png`
22. `advanced/overall-grade-b.png`
23. `advanced/overall-grade-c.png`
24. `advanced/overall-grade-na.png`

Background PNGs should be lossless, cover the complete page, and contain all
fixed artwork and rubric text. They must not contain student data, score
labels, yellow selections, an overall-grade value, or a signature. The minimum
recommended 300-DPI size is 2250 by 3250 pixels.

Label PNGs must use RGBA transparency, be tightly cropped around the visible
glyphs, and contain no yellow or gray cell background. The minimum recommended
pixel dimensions and fixed rendered point sizes are recorded in
`asset-spec.json`. A replacement may contain more pixels than the minimum; it
will still render at the recorded physical size.

The Standard and Advanced label files are intentionally separate because the
two templates use different typography.
