# C44 roster font axis repair

Task ID: `phase7_c44_subprep_implementation_01`.

The embedded Pretendard variable font supports a `wght` axis. Qt 6.12 selected
only the first Pretendard style used by a `QPdfWriter`, even when body and
heading `QFont` instances requested different style names. The minimal
experiment in `experiment.cpp` applies axis values 400 (Regular) and 700
(Bold), then renders both Bold-first and Regular-first. Both PDFs embed
`PretendardVariable-Bold` and `PretendardVariable-Regular`. `experiment.log`
records resolved axis values and styles; the small PDFs preserve the draw-order
evidence.

The C44 roster renderer now sets the Regular axis on its operation-scoped
Pretendard font and updates that axis for bold text. The generated daily PDF
embeds both Pretendard faces. On page 1, the section heading uses Bold and the
student names use Regular. The output regression test asserts both embedded
face names.

The strict package text comparator passes all 16 roster pages without
normalization or oracle changes. The candidate and retained PDFs have known
unmapped separator CIDs; exact extracted-text agreement does not establish
copy, search, or accessibility correctness.

Both PDFs have 16 pages with matching dimensions. At 150 DPI and 1240×1754,
changed-pixel percentages range from 11.0517% to 12.2148%; mean absolute channel
differences range from 5.60799 to 6.81223. The font weight mismatch is reduced,
but full raster parity is not established. See `pixel-comparison.txt` and
`text-comparison.txt` for page-level results.

Fresh macOS Debug checks:

- `ClassMngrSubPrepPrintPdfTests`, `ClassMngrSubPrepRosterOutputParityTests`,
  and `ClassMngrSubPrepPackageServiceTests`: 3/3 CTests passed.
- `largePackageGeneratesOutputReferenceWhenConfigured`: passed with the
  unchanged 96-class fixture and strict reference comparator.
- `ClassMngrNextSubPrepVisualCapture`: passed and regenerated the four
  English/Korean × Light/Dark variants.
- `subPrepRouteUsesTheActiveGatewayAndReleasesAcrossLifecycle`: 3/3 QtTest
  functions passed, including Cancel without output side effects and checks for
  both combo-arrow resources.

Logs and generated PDFs in this directory preserve the raw evidence. The
reference PDF and comparator were not changed.
