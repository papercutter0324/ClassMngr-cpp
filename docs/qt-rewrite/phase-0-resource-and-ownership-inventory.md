# Qt Rewrite Phase 0 - Resource and Ownership Inventory

Status: Packaged Release payload/lifetime trace recorded; cross-platform and
remaining feature-ownership evidence are still in progress.
Snapshot: `75755460`

The raw checked-in asset tree contains 226 files totaling 65,729,685 bytes.
The table is a source inventory, not a decoded-memory measurement.

| Asset root | Files | Installed bytes | Current owner/path | Initial v2 classification |
| --- | ---: | ---: | --- | --- |
| `campuses` | 17 | 5,121,304 | `ResourcePackManager`, `ResourcePaths::Campuses`, campus pages/maps | Feature; load on campus entry |
| `documents` | 53 | 34,484,918 | `DocumentCatalog`, `ResourcePaths::Documents`, PDF/document workflows | Startup metadata only; PDF/PPTX bodies on viewer/operation request, with explicit release |
| `files` | 3 | 38,903 | `ResourcePaths::Files` and feature workflows | Operation; load only for owning workflow |
| `fonts` | 6 | 21,098,232 | `FontManager`, typed signatures, speaking reports | Core/startup only for required family; report fonts operation-scoped |
| `icons` | 83 | 313,958 | Qt resources, themed actions, file-dialog style | Core/startup |
| `images` | 1 | 9,861 | `ResourcePaths::DynamicImages` | Feature/operation |
| `splash` | 1 | 130,586 | `ResourcePaths::Splash`, `SplashScreen` | Must be removed in v2 |
| `styles` | 2 | 32,284 | `ThemeService`/global stylesheet | Core/startup |
| `templates` | 55 | 2,750,781 | roster/speaking/report/template services | Feature/operation; speaking sources are build-excluded |
| `translations` | 5 | 1,748,858 | `LanguageService` and Qt Linguist output | Core/startup |
| `roster-designs` | 0 in this checkout | `ResourcePackManager` declaration and `ResourcePaths::RosterDesigns` | Declared optional updateable pack, but no source directory or packaged payload is present; keep this explicit for the Phase 4 packaging decision |

## Current ownership and retention findings

| Object/resource | Current lifetime | Rewrite concern |
| --- | --- | --- |
| `ResourcePackManager` singleton | Process lifetime; discovers/mounts packs and tracks leases | Replace pack mounts/updates with deterministic installed resource tree |
| `ResourcePackLease` | Feature/page scope, sometimes retained as page members | Ensure a typed loader owns only bounded, explicit feature resources |
| `ResourcePaths` | Header-level global access and filesystem fallback | Remove location knowledge from feature pages |
| `PageManager` pages | Instantiated pages remain under `QStackedWidget` | Release/recreate large feature trees without losing state |
| `FontManager`/`QFontDatabase` | Global application fonts | Avoid loading optional report/signature fonts at startup |
| Campus `QPixmap` maps | Campus page/map-preview ownership | Bound decoded image size and release on leaving feature |
| `QPdfDocument`/PDF viewer | Active viewer/operation document lifetime | Never load catalog PDF bodies at startup; load on explicit viewer request and close/release on close, replacement, leave, or page release |
| Calendar event cache/QML objects | Calendar feature lifetime/cache | Keep cache budget and QML object count observable |
| Document catalog | Sidebar/application service lifetime | Keep localized metadata and validated references resident; defer PDF/PPTX bodies and do not cache them without an explicit bounded policy |
| `DataService` and repositories | Open-database/application-service lifetime | Replace broad compatibility facade with explicit workspace store/repositories |

## Current pack/build coupling

`cmake/resources.cmake` builds standalone RCC files for `campuses`,
`documents`, `files`, `images`, `splash`, and `templates`. The executable also
embeds fonts, icons, styles, translations, and other non-scoped assets. Runtime
code can resolve `:/` resources, mounted pack roots, application-adjacent
filesystem paths, and the source tree. The rewrite must preserve installed
resource availability while deleting the independent pack mount/update path.

## Packaged Release trace

The heavy packaged Windows x64 Release route now runs
`--startup-performance-resource-trace` after full large-workspace navigation,
the PDF open/render/release/reopen path, and return to My Workspace. The
retained evidence is under
`docs/qt-rewrite/visual-baseline/release/large-resource-trace-boundary/`.

The trace enumerated 189 payloads totaling `62,781,401` logical installed
bytes. It measured potential decoded image residency for 34 images totaling
`147,851,916` bytes, while leaving all PDF and PPTX entries at zero decoded
bytes and classifying 83 entries as on-demand. The six required RCC packs were
acquired and returned to their pre-trace mount state. The declared
`roster-designs` pack remains explicitly unavailable (`required=false`) because
this checkout contains neither its source directory nor a packaged `.rcc`;
the trace records that discrepancy instead of silently treating it as a
loaded resource.

This is a payload/lifetime baseline, not a claim that all decoded images are
simultaneously resident: the image values are per-resource potential sizes and
the process working-set report remains the authority for actual resident
memory.

## Trace points to add or retain

- Resource catalog initialization and per-classification load time.
- Decoded image dimensions and retained bytes.
- Font registration/removal and family names.
- PDF load/status/page-render/open and close/release events, document size, and active loaded-document count.
- Document catalog entry count and content opens.
- Page creation/destruction, enter/leave transitions, and retained widget
  counts.
- Cache hits, misses, evictions, and explicit byte/item budgets.
