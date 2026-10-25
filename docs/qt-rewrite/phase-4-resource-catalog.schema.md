# Phase 4 Runtime Resource Catalog

`generate_resource_catalog.py` combines the Phase 4 inventory with the
`resource-manifest.json` produced by `bake_resources.py`. It writes a
deterministic `ClassMngrNext.ResourceCatalog` JSON document with one record for
each runtime resource. Build-only inputs and v2 exclusions stay in the bake
manifest and do not become catalog records.

## Path contract

Every `package_relative_path` is a canonical POSIX path. Direct resource files
use the baker's `relative_path`, such as `assets/campuses/bundang.json`. The
base is the logical `ClassMngrNext::Resources` package resource root; F1467
chooses its platform install location.

Generated translations use
`assets/translations/<translation-source-stem>.qm`. Generated Calendar module
components use `qt/qml/ClassMngr/Calendar/<source-filename>.qml`; their Qt
resource URL is `qrc:/` followed by that path. The Calendar path follows the
existing `ClassMngr.Calendar` module URI, `/qt/qml` resource prefix, and QML
file aliases. The two module components and five translation outputs have
explicit, non-colliding names. The generated catalog defaults to a sibling of
the baked `Resources` directory so it does not add an untracked file to the
baker-owned tree.

## Record fields

- `logical_id`, `category`, and `resource_kind` preserve the inventory identity
  and classification.
- `expected_type` describes the packaged form: direct file types use stable
  lowercase names (`json`, `pdf`, `svg`, `ttf`, and so on; QSS uses
  `stylesheet`); translations use `qm`; Calendar components use
  `qml_module_component`.
- `loading.tier`, `loading.policy`, `loading.lifetime`, and
  `loading.release_policy` map to the inventory's `loading_tier`,
  `loading_method`, `expected_lifetime`, and `release_policy`. The inventory's
  startup necessity and streamability values are retained explicitly.
- `provenance.source` carries only the source path, byte count, and SHA-256 for
  this runtime record. Direct records also carry verified `provenance.baked`
  byte and SHA-256 values. Generated records carry a `generation` expectation
  and do not claim an installed output size or hash before Qt builds it.
- `location` is `resource_file` for package files and `qt_resource` for the
  compiled Calendar module resource.

Document catalog metadata (`document.catalog-metadata`) and the 51 document
body IDs remain separate. Bodies retain their viewer/operation loading tier
and `not_required_until_document_request` startup classification; catalog
generation verifies their copied bytes without opening or decoding them.

From the repository root, generate the catalog after baking with:

```sh
python3 scripts/phase4/generate_resource_catalog.py \
  --bake-manifest build/ClassMngrNext/Resources/resource-manifest.json \
  --baked-root build/ClassMngrNext/Resources
```
