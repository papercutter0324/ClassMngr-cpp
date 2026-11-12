# Phase 8 Roster output parity matrix

When `CLASSMNGR_ROSTER_OUTPUT_PARITY_DIR` is set, the existing
`ClassMngrNextRosterOutputTests::templateParity` cases retain a synthetic
Legacy and Next PDF, each page's rendered PNG, and one `output-evidence.json`
per case. The evidence covers By Day landscape, Daily portrait, and Per Class
in portrait and landscape. Each case records the platform, Qt version,
requested font families, PDF hashes, page text, page geometry, and paired
raster hashes. The Per Class cases each include both generated pages.

The tests require exact Legacy/Next page text, point geometry, orientation, and
1000×1000 `QPdfDocument` raster equality on the same platform. Font fallback
and QtPdf can differ between operating systems, so the artifacts document the
per-platform pair; raster hashes are not compared across hosts. All fixture
values are synthetic `FIXTURE` data and no physical print is performed.

The Refactoring baseline workflow enables the capture during its full CTest
run on Linux x64 and Windows x64. It uploads the paired PDFs, PNGs, and JSON
files in each platform's baseline artifact. The clean macOS universal Debug
candidate runs locally and retains its evidence in
`build/macos-clang-debug/artifacts/normal-access-rerun/roster-output/`.

## Linux x64 Debug evidence

The candidate-2 local incremental build of `ClassMngrNextRosterOutputTests`
passed after
its Legacy reference link was routed through `ClassMngrLegacyRuntimeClosure`
and the test target's existing `ClassMngrClassTransferSupport` / Domain
archive cycle was rescanned. Focused CTest passed 1/1 in 3.87 seconds on
Fedora Linux 44 x86_64 with Qt 6.12.0; the valid release composition uses Qt
Core 6.12 with QtPdf 6.14. All four template/orientation cases passed. They
produced six paired 1000×1000 page rasters with equal text, geometry, and
image hashes.

## macOS universal Debug local evidence

At current source-equivalent HEAD `e651b016`, all four template/orientation
cases passed on macOS: By Day landscape, Daily portrait, and Per Class portrait
and landscape. The cases produced the expected 1, 1, 2, and 2 pages. Paired
Legacy/Next evidence is under
`build/macos-clang-debug/artifacts/normal-access-rerun/roster-output/`.
