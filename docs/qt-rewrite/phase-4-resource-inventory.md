# Phase 4 Effective Resource Inventory

The companion [machine-readable inventory](phase-4-resource-inventory.json)
records every tracked file under `resources/`, plus the two tracked Calendar
QML module inputs located under `src/`. It gives each file a stable logical
identity, exact source byte count and SHA-256, v2 packaging disposition and
reason, optional owning feature, startup need, loading tier and method, expected
lifetime and release policy, streamability status, and an installed/resident
size measurement method.

## Refreshed source baseline

At source revision `58de51bf`, the tree has 226 tracked `resources/assets`
files totaling **65,707,627 exact source bytes**. The total includes the 28
speaking-evaluation source inputs under `templates/speaking-eval/sources/`;
those inputs total 1,350,933 bytes and remain build-only. There are three
additional tracked Windows packaging templates under `resources/windows/`
totaling 1,186 bytes, so 229 tracked paths are recorded under `resources/`; all
229 total **65,708,813 exact source bytes**. The two Calendar QML module
sources are outside that tree and are listed separately (two files, 35,154
bytes). F1482 changed four packaging dispositions without changing any source
path, size, or hash.

| Asset group | Files | Exact source bytes |
| --- | ---: | ---: |
| Campuses | 17 | 5,119,172 |
| Documents, including catalog metadata and one Office lock file | 53 | 34,484,308 |
| Roster workflow files | 3 | 38,903 |
| Fonts | 6 | 21,098,232 |
| Icons | 83 | 313,845 |
| Images | 1 | 9,861 |
| Legacy splash | 1 | 130,586 |
| Styles | 2 | 30,777 |
| Translations (`.ts` sources) | 5 | 1,732,872 |
| Templates, including excluded speaking-art sources | 55 | 2,749,071 |
| **Total `resources/assets`** | **226** | **65,707,627** |

These are exact tracked source-file sizes, not an installed-package or decoded
memory measurement. The earlier Phase 0 inventory is an older snapshot; use
this Phase 4 inventory for the refreshed source baseline. The 65,707,627-byte
figure is for `resources/assets`; the all-tracked-resources total also includes
the three Windows templates. Calendar QML source bytes are counted separately.

## Packaging and loading disposition

Among the 226 tracked asset files, 192 are intended for v2 runtime delivery
(187 direct payloads and five generated translation outputs), 28 speaking-art
inputs are build-only, and six assets are `excluded_v2`: the splash image, the
Office lock file, `dyb.png`, and three callerless roster workflow files. The
three Windows templates are additional build-only inputs under `resources/`;
the two Calendar QML sources are separate generated runtime inputs outside it.
The JSON contains 231 source records: 187 direct runtime resources, seven
generated runtime resources (five translations and two Calendar QML
components), six v2 exclusions, and 31 build-only inputs. The runtime catalog
has 194 entries: 187 direct files, five generated QMs, and two compiled QML
keys.

- `resources/assets/documents/documents.json` is recorded in
  `document_catalog_metadata`, separate from the 51 `document_bodies` records.
  Catalog startup reads metadata only; a selected body opens for a viewer or
  operation request.
- The 28 files under `templates/speaking-eval/sources/` and three files under
  `resources/windows/` are recorded as build inputs, with reasons for their
  non-loader status. The 165-byte tracked Office lock file is included by the
  legacy documents RCC glob and appears with `installedPayloadBytes: 165` in
  [the retained Phase 0 resource trace](visual-baseline/release/large-resource-trace-boundary/resource-trace.json).
  The v2 canonical tree excludes it as a transient lock artifact.
- The legacy splash image is recorded but excluded from the v2 runtime package
  under Phase 4's splash-removal decision.
- Five tracked translation sources produce runtime `.qm` files. The two
  Calendar QML source files are tracked outside `resources/` and compile into
  the Qt QML module. Their generated installed sizes are not claimed here.
- No `roster-designs` source payload exists in this checkout, and the checked
  application-owned installed resource-pack directory was empty. The
  machine-readable inventory records this explicitly instead of creating
  placeholder assets.
- F1482 classifies four tracked callerless assets—`dyb.png`, `Class Register.odt`,
  `Roster Template 1 - By Day.ods`, and `Roster Template 1 - By Day.xlsx`—as
  `excluded_v2`. Their exact IDs, source paths, sizes, and hashes remain in the
  inventory for provenance; the files remain in the legacy resource-pack
  oracle and are omitted from the v2 runtime package and catalog. These records
  do not invent feature owners. The splash image and Office lock file remain
  the other two `excluded_v2` assets.

## Size and streamability method

For every tracked input, `source_file.bytes` is the exact current file length
and `source_file.sha256` hashes those same bytes. The v2 installed resource
tree has not been produced, so `installed_size.bytes` remains null and points
to a method for measuring the staged direct or generated output. This keeps
source bytes distinct from generated `.qm`/QML sizes and from RCC, executable,
or application-container overhead.

Decoded/resident sizes remain unknown until measured through the real runtime
path. The inventory points raster images to Qt's decoded image allocation,
fonts to per-platform registration/resident measurements, PDFs to active
document and rendered-page measurements, and Office files to their actual
consumer/import/export path. These methods avoid treating compressed source
bytes as memory estimates. PDF streaming is marked as a file-backed candidate;
Office document streaming remains consumer-dependent pending adapter
validation. The four `excluded_v2` source records preserve provenance without
assigning feature owners where the production tree has no caller.

## Reproducibility

From the repository root, regenerate or check the JSON with:

```sh
python3 scripts/phase4/generate_resource_inventory.py
python3 scripts/phase4/generate_resource_inventory.py --check
```

The focused utility test is:

```sh
python3 -m unittest -v scripts/phase4/test_generate_resource_inventory.py
```
