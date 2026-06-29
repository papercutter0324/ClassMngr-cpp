# ClassMngr Resource Packs

ClassMngr can update campus information, printable templates, roster designs,
and bundled file categories independently of the application. Packs are Qt
binary resource collections (`.rcc`). They are data-only, platform-neutral, and
mounted read-only.

The known pack ids and their required RCC roots are:

| Pack id | RCC prefix | Compiled fallback |
| --- | --- | --- |
| `campuses` | `/resource-packs/campuses` | `:/assets/campuses` |
| `templates` | `/resource-packs/templates` | `:/assets/templates` |
| `roster-designs` | `/resource-packs/roster-designs` | `:/assets/roster-designs` |
| `book-reports` | `/resource-packs/book-reports` | `:/assets/files/book reports` |
| `essay` | `/resource-packs/essay` | `:/assets/files/essay` |
| `essay-topics` | `/resource-packs/essay-topics` | `:/assets/files/essay_topics` |
| `evaluations` | `/resource-packs/evaluations` | `:/assets/files/evaluations` |
| `guides` | `/resource-packs/guides` | `:/assets/files/guides` |
| `lessons` | `/resource-packs/lessons` | `:/assets/files/lessons` |
| `sub-prep` | `/resource-packs/sub-prep` | `:/assets/files/sub prep` |
| `training` | `/resource-packs/training` | `:/assets/files/training` |

If an installed pack is absent, corrupt, has invalid metadata, or cannot be
mounted, the app uses its compiled fallback. Downloaded updates are staged
atomically and become active on the next launch.

## Building a pack

Create a qrc file whose prefix matches the table. For example:

```xml
<!DOCTYPE RCC>
<RCC version="1.0">
  <qresource prefix="/resource-packs/campuses">
    <file alias="bundang.json">campuses/bundang.json</file>
    <file alias="bundang/bundang_map.png">campuses/bundang/bundang_map.png</file>
  </qresource>
</RCC>
```

The external pack contents should live directly under that pack's resource
root. For example, the `book-reports` pack uses
`/resource-packs/book-reports`, not `/resource-packs/book-reports/book reports`.

Build it with the `rcc` executable from the same Qt major version used by the
application:

```sh
rcc --binary campuses.qrc --output campuses-1.1.0.rcc
sha256sum campuses-1.1.0.rcc
```

Pack versions use strict `x.x.x` format. Embedded fallback versions are defined
per pack in `resource_pack_manager.cpp`; when compiled fallback content changes,
update the matching definition there.

## Server manifest

Publish pack files first. Publish `latest.json` and its detached signature
last. The manifest should include every known pack id, even when a given pack
does not currently have an update available.

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
    "book-reports": {
      "version": "1.0.1",
      "url": "https://example.com/packs/book-reports-1.0.1.rcc",
      "fileName": "book-reports-1.0.1.rcc",
      "sha256": "64 lowercase hexadecimal characters",
      "sizeBytes": 456789
    },
    "essay": {
      "version": "1.0.1",
      "url": "https://example.com/packs/essay-1.0.1.rcc",
      "fileName": "essay-1.0.1.rcc",
      "sha256": "64 lowercase hexadecimal characters",
      "sizeBytes": 567890
    },
    "essay-topics": {
      "version": "1.0.1",
      "url": "https://example.com/packs/essay-topics-1.0.1.rcc",
      "fileName": "essay-topics-1.0.1.rcc",
      "sha256": "64 lowercase hexadecimal characters",
      "sizeBytes": 678901
    },
    "evaluations": {
      "version": "1.0.1",
      "url": "https://example.com/packs/evaluations-1.0.1.rcc",
      "fileName": "evaluations-1.0.1.rcc",
      "sha256": "64 lowercase hexadecimal characters",
      "sizeBytes": 789012
    },
    "guides": {
      "version": "1.0.1",
      "url": "https://example.com/packs/guides-1.0.1.rcc",
      "fileName": "guides-1.0.1.rcc",
      "sha256": "64 lowercase hexadecimal characters",
      "sizeBytes": 890123
    },
    "lessons": {
      "version": "1.0.1",
      "url": "https://example.com/packs/lessons-1.0.1.rcc",
      "fileName": "lessons-1.0.1.rcc",
      "sha256": "64 lowercase hexadecimal characters",
      "sizeBytes": 901234
    },
    "sub-prep": {
      "version": "1.0.1",
      "url": "https://example.com/packs/sub-prep-1.0.1.rcc",
      "fileName": "sub-prep-1.0.1.rcc",
      "sha256": "64 lowercase hexadecimal characters",
      "sizeBytes": 101234
    },
    "training": {
      "version": "1.0.1",
      "url": "https://example.com/packs/training-1.0.1.rcc",
      "fileName": "training-1.0.1.rcc",
      "sha256": "64 lowercase hexadecimal characters",
      "sizeBytes": 112345
    }
  }
}
```

Sign the exact manifest bytes. The resource-pack updater uses the application
update public key unless a separate resource-pack key is configured.

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

`CLASSMNGR_RESOURCE_PACK_REQUIRE_SIGNATURE` and
`CLASSMNGR_RESOURCE_PACK_CHECK_ON_STARTUP` both default to `ON`. If the public
key setting is omitted, `CLASSMNGR_UPDATE_PUBLIC_KEY_PEM` is reused. With no
manifest URL configured, startup checks are skipped.

The signed manifest supplies the artifact checksums. Each download is size- and
SHA-256-verified, checked for the expected RCC root, then installed through an
atomic metadata update. Network or validation failures only log a warning and
never prevent startup.
