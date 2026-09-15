# Phase 4 — Resource Loader and Packaging

## Status

- Status: Not started
- Default route: Heavy
- Depends on: Phase 1
- Blocks: Bootstrap, feature migration, packaging, and memory gates
- Owner: Unassigned
- Last updated: 2026-09-15
- Current note: Replace dynamic resource packs with deterministic installed resources and explicit lifetimes.

## Objective

Remove the resource-pack architecture and create a typed, observable resource loader that loads only what is required for the current application state.

## Product decision

Resources are delivered as part of the normal application package. A normal application update replaces the executable and its installed resources together.

There will be:

- No independently mounted RCC packs.
- No runtime resource-pack directory.
- No resource-pack manifest download.
- No resource-pack signature download.
- No resource-pack update check.
- No resource-pack preferences.
- No resource-pack leases.
- No production source-directory fallback.

## Work packages

### 4.1 Effective-resource inventory

Inventory all current resources:

- Icons.
- Styles.
- Translations.
- Fonts.
- Documents.
- Campus data.
- Campus maps.
- Templates.
- Roster designs.
- Speaking-evaluation report art.
- Calendar QML.
- Splash assets.

For every resource record:

- Logical identity.
- Installed size.
- Expected decoded or resident size.
- Owning feature.
- Startup necessity.
- Loading method.
- Expected lifetime.
- Whether it can be streamed.
- Whether it can be discarded after use.

### 4.2 Offline resource baking

Before deleting the runtime pack system, create an offline build utility that:

1. Resolves the currently effective baseline and approved installed resource content.
2. Extracts the content into canonical files.
3. Validates required paths and types.
4. Produces a build-time resource inventory.
5. Makes the canonical resources part of the application release.

This preserves content that users may currently receive through resource packs without preserving the runtime update mechanism.

### 4.3 Installed resource tree

Use a deterministic application-owned tree such as:

    resources/
    ├── core/icons
    ├── core/styles
    ├── core/translations
    ├── core/fonts
    ├── campuses
    ├── documents
    ├── templates
    ├── roster-designs
    ├── speaking-eval
    └── calendar

Use the correct platform installation location:

- Windows application resources.
- macOS bundle Resources.
- Linux application data directory.

The loader must resolve only known package-relative paths. It must not search the source tree in a packaged build.

### 4.4 Loader interfaces

Create:

- ResourceId: typed logical identifier.
- ResourceDescriptor: path, category, expected type, and loading policy.
- ResourceCatalog: generated inventory of installed resources.
- ResourceLoader: text, JSON, image, font, stylesheet, translation, and stream loading.
- ResourceScope: startup, page, feature, and operation lifetime.
- ResourceCache: explicit byte budget and LRU eviction.
- ResourceDiagnostics: load time, decoded size, cache residency, and owner.

Return owning objects or handles with explicit lifetimes. Do not return a path whose validity depends on hidden mounting state.

### 4.5 Loading tiers

Core startup resources:

- Application icon.
- Selected translation.
- Active theme stylesheet.
- Required UI fonts.
- Small navigation and action icons.
- Minimal catalog metadata.
- Minimal initial-workspace metadata.

Feature resources:

- Campus details and maps.
- Document bodies.
- Roster designs.
- Speaking report artwork.
- Calendar QML assets.
- Feature-specific templates.

Operation resources:

- PDF source content.
- Print-only assets.
- Report export images.
- PowerPoint workspaces.
- Temporary import/export buffers.

Optional decorative fonts must be loaded only when a feature uses them.

### 4.6 Build and deployment removal

Remove production code, tests, options, and deployment behavior for:

- Resource-pack manifests.
- Resource-pack signatures.
- Resource-pack storage.
- Runtime RCC registration.
- Resource-pack leases.
- Resource-pack downloads.
- Resource-pack startup checks.
- Resource-pack settings.
- Resource-pack update dialogs.
- Splash resources and splash paths.

### 4.7 Existing installation handling

The new application must not mount old resource packs. Initially, old application-owned pack directories may be left untouched and ignored.

If an installer migration later removes them, it must:

- Target only the known application-owned directory.
- Verify canonical resources are installed first.
- Log what was removed.
- Never touch workspace files or user documents.

## Deliverables

- ResourceCatalog.
- ResourceLoader.
- ResourceCache.
- ResourceDiagnostics.
- Offline resource-baking utility.
- Canonical installed resource tree.
- Updated CMake and deployment rules.
- Resource-loader unit and integration tests.
- Resource-pack removal checklist.

## Exit gate

The application starts without mounting a resource pack, performs no resource update request, and displays the same resource-backed content as the baseline.

Large resources are not loaded or decoded until their owning feature or operation requests them.

## Heavy-route requirements

- Do not replace resource packs with another hidden network-backed pack format.
- Do not preload all documents, templates, maps, or fonts.
- Do not keep both raw bytes and decoded objects beyond the required lifetime.
- Do not use source-directory fallback in a packaged build.
- Do not remove visual assets merely to meet the memory target.
