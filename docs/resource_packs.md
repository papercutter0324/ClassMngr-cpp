# ClassMngr Resource Packs

ClassMngr can update campus information, printable templates, roster designs,
and the Documents catalog independently of the application. Packs are Qt binary
resource collections (`.rcc`). They are data-only, platform-neutral, and
mounted read-only.

The known pack ids and their required RCC roots are:

| Pack id | RCC prefix | Compiled fallback |
| --- | --- | --- |
| `campuses` | `/resource-packs/campuses` | `:/assets/campuses` |
| `templates` | `/resource-packs/templates` | `:/assets/templates` |
| `roster-designs` | `/resource-packs/roster-designs` | `:/assets/roster-designs` |
| `documents` | `/resource-packs/documents` | `:/assets/documents` |

If an installed pack is absent, corrupt, has invalid metadata, or cannot be
mounted, the app uses its compiled fallback. Downloaded updates are staged
atomically and become active on the next launch.

The `documents` pack must contain `documents.json` and every file referenced
by that catalog under the same resource root. Keeping the catalog and its files
in one pack ensures sidebar changes and document binaries activate together.

## Building a pack

Create a qrc file whose prefix matches the table. A Documents pack resembles:

```xml
<!DOCTYPE RCC>
<RCC version="1.0">
  <qresource prefix="/resource-packs/documents">
    <file alias="documents.json">documents/documents.json</file>
    <file alias="Guides/DYB Lesson Planning Guide.pdf">documents/Guides/DYB Lesson Planning Guide.pdf</file>
  </qresource>
</RCC>
```

Build it with the `rcc` executable from the same Qt major version used by the
application:

```sh
rcc --binary documents.qrc --output documents-1.1.0.rcc
sha256sum documents-1.1.0.rcc
```

Pack versions use strict `x.x.x` format. Embedded fallback versions are
defined in `resource_pack_manager.cpp`; increment the corresponding version
when compiled fallback content changes.

## Server manifest

Publish pack files first. Publish `latest.json` and its detached signature
last. The manifest must include every pack id required by supported clients.

```json
{
  "schemaVersion": 1,
  "packs": {
    "campuses": {
      "version": "1.1.0",
      "url": "https://example.com/packs/campuses-1.1.0.rcc",
      "fileName": "campuses-1.1.0.rcc",
      "sha256": "64 lowercase hexadecimal characters",
      "sizeBytes": 123456
    },
    "templates": {
      "version": "1.0.1",
      "url": "https://example.com/packs/templates-1.0.1.rcc",
      "fileName": "templates-1.0.1.rcc",
      "sha256": "64 lowercase hexadecimal characters",
      "sizeBytes": 234567
    },
    "roster-designs": {
      "version": "1.2.0",
      "url": "https://example.com/packs/roster-designs-1.2.0.rcc",
      "fileName": "roster-designs-1.2.0.rcc",
      "sha256": "64 lowercase hexadecimal characters",
      "sizeBytes": 345678
    },
    "documents": {
      "version": "1.1.0",
      "url": "https://example.com/packs/documents-1.1.0.rcc",
      "fileName": "documents-1.1.0.rcc",
      "sha256": "64 lowercase hexadecimal characters",
      "sizeBytes": 456789
    }
  }
}
```

During migration, keep the retired `book-reports`, `essay`, `essay-topics`,
`evaluations`, `guides`, `lessons`, `sub-prep`, `training`, and
`vacation` entries alongside `documents`. Older clients require those ids;
newer clients accept them as extra manifest entries. Remove them only after old
clients are no longer supported.

Sign the exact manifest bytes:

```sh
openssl dgst -sha256 -sign private.pem -out latest.sig latest.json
```

## Build configuration

```sh
cmake --preset linux-gcc-release \
  -DCLASSMNGR_RESOURCE_PACK_MANIFEST_URL="https://example.com/packs/latest.json" \
  -DCLASSMNGR_RESOURCE_PACK_SIGNATURE_URL="https://example.com/packs/latest.sig" \
  -DCLASSMNGR_RESOURCE_PACK_PUBLIC_KEY_PEM="-----BEGIN PUBLIC KEY-----...-----END PUBLIC KEY-----"
```

`CLASSMNGR_RESOURCE_PACK_REQUIRE_SIGNATURE` defaults to `ON`.
`CLASSMNGR_RESOURCE_PACK_CHECK_ON_STARTUP` defaults to `OFF` while the
resource-pack update interface remains disabled. Configure a dedicated
resource-pack public key when signature verification is enabled. With no
manifest URL configured, resource-pack checks are skipped.

Each download is size- and SHA-256-verified, checked for its expected RCC root,
then installed through an atomic metadata update. Network or validation failures
only log warnings and never prevent startup.
