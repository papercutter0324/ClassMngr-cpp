# Campus Directory — candidate #41

The paired evidence uses the packaged Bundang campus record (`id=bundang`) and its campus and Cheonggu map images. Legacy loads the JSON through the read-only `campuses` RCC; Next loads it through the packaged resource catalog. Both views use the same locale, theme, campus, and 1024×768 viewport. These pairs support visual review and do not claim pixel parity: Legacy keeps the five sections as tabs in one dashboard, while Next exposes five separate routes with different form grouping and page chrome.

## Captures

The [manifest](campus_directory_paired_capture.json) records all 20 Legacy/Next route pairs (40 PNGs), the rendering configuration, and the map lifecycle memory observations.

| Route | en_US · light | en_US · dark | ko_KR · light | ko_KR · dark |
|---|---|---|---|---|
| Information | [Legacy](campus_information_en_US_light_legacy.png) / [Next](campus_information_en_US_light_next.png) | [Legacy](campus_information_en_US_dark_legacy.png) / [Next](campus_information_en_US_dark_next.png) | [Legacy](campus_information_ko_KR_light_legacy.png) / [Next](campus_information_ko_KR_light_next.png) | [Legacy](campus_information_ko_KR_dark_legacy.png) / [Next](campus_information_ko_KR_dark_next.png) |
| Directions | [Legacy](campus_directions_en_US_light_legacy.png) / [Next](campus_directions_en_US_light_next.png) | [Legacy](campus_directions_en_US_dark_legacy.png) / [Next](campus_directions_en_US_dark_next.png) | [Legacy](campus_directions_ko_KR_light_legacy.png) / [Next](campus_directions_ko_KR_light_next.png) | [Legacy](campus_directions_ko_KR_dark_legacy.png) / [Next](campus_directions_ko_KR_dark_next.png) |
| Address | [Legacy](campus_address_en_US_light_legacy.png) / [Next](campus_address_en_US_light_next.png) | [Legacy](campus_address_en_US_dark_legacy.png) / [Next](campus_address_en_US_dark_next.png) | [Legacy](campus_address_ko_KR_light_legacy.png) / [Next](campus_address_ko_KR_light_next.png) | [Legacy](campus_address_ko_KR_dark_legacy.png) / [Next](campus_address_ko_KR_dark_next.png) |
| Housing | [Legacy](campus_housing_en_US_light_legacy.png) / [Next](campus_housing_en_US_light_next.png) | [Legacy](campus_housing_en_US_dark_legacy.png) / [Next](campus_housing_en_US_dark_next.png) | [Legacy](campus_housing_ko_KR_light_legacy.png) / [Next](campus_housing_ko_KR_light_next.png) | [Legacy](campus_housing_ko_KR_dark_legacy.png) / [Next](campus_housing_ko_KR_dark_next.png) |
| Maps | [Legacy](campus_maps_en_US_light_legacy.png) / [Next](campus_maps_en_US_light_next.png) | [Legacy](campus_maps_en_US_dark_legacy.png) / [Next](campus_maps_en_US_dark_next.png) | [Legacy](campus_maps_ko_KR_light_legacy.png) / [Next](campus_maps_ko_KR_light_next.png) | [Legacy](campus_maps_ko_KR_dark_legacy.png) / [Next](campus_maps_ko_KR_dark_next.png) |

## Map memory and release

The capture harness uses `PlatformProcessMemorySnapshotProvider`. On macOS, `workingSetBytes` is resident memory and `privateUsageBytes` is physical footprint. These are whole-process observations, not page-attributed allocations or a memory budget. The sample was taken on macOS arm64 with Qt 6.12.0, the offscreen platform plugin, Fusion, 1024×768, English light, and the packaged Bundang record. The values are recorded in the manifest.

| Stage | Resident working set | Physical footprint | Loaded map resources | Decoded dimensions |
|---|---:|---:|---:|---|
| Before map entry | 157,515,776 B | 92,898,336 B | 0 | — |
| After opening Maps | 158,990,336 B | 94,340,128 B | 2 | 1230×1150, 1225×859 |
| After leave, suspension, and page release | 158,990,336 B | 94,340,128 B | 0 | — |
| After reopening Maps | 163,217,408 B | 98,567,200 B | 2 | 1230×1150, 1225×859 |

The map page releases both decoded images when suspended and is destroyed before the reopen sample. The process resident set did not shrink at release time; the measurements describe the process allocator and Qt runtime as well as the page. The accepted 1520-pixel per-axis decode cap is respected by both images. No Phase 9 memory threshold is claimed.

## Rendering and compatibility limits

Captures use Qt's offscreen platform plugin because this environment has no available screen; `QT_SCALE_FACTOR=1` and packaged light/dark stylesheets are used. The images omit native window decorations. The Legacy record retains historical `:/assets/campuses/...` map references while its current RCC is mounted at `:/resource-packs/campuses`. The capture target provides a read-only qrc alias for the same packaged Bundang PNG bytes so those references render in Legacy; this alias exists only in the capture target and does not change the application resources. The page groupings, status text, scrolling, and chrome differ, so paired captures are not pixel-parity evidence.

Reproduce with the standard macOS build tree:

```sh
cmake --build build/macos-clang-debug --target ClassMngrNextCampusDirectoryVisualCapture --parallel 2
env QT_QPA_PLATFORM=offscreen QT_SCALE_FACTOR=1 build/macos-clang-debug/ClassMngrNextCampusDirectoryVisualCapture docs/qt-rewrite/visual-baseline/next/phase-7/campus-directory-c41
```
