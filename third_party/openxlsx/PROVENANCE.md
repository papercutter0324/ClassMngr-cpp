# OpenXLSX Dependency Provenance

**Status:** Phase 2 dependency bridge complete; workbook reader approval remains
with Phases 3-7.

## Candidate Source

| Field | Recorded value |
| --- | --- |
| Project | OpenXLSX |
| Authoritative project URL | `https://codeberg.org/lars_uffmann/OpenXLSX` |
| Local evaluation path | `C:\Git\openxlsx` |
| Evaluation date | 2026-09-13 |
| Local Git metadata | absent; the directory is not a Git working tree |
| Matching upstream ref | `refs/heads/development-aral` |
| Matching upstream commit | `ece329af84b370a8b77f5a3ee0e30509ad0f0bf9` |
| Repository source path | `third_party/openxlsx/source` (Git submodule, detached at the pinned commit) |
| Primary license | BSD-3-Clause |
| Product source policy | pinned Git submodule revision |

The local path is an input to this audit only. Product configuration must not
resolve headers or libraries from `C:\Git\openxlsx`; it must use the pinned
submodule path.

## Version Evidence

The local source does not present one unambiguous release identity:

- `CMakeLists.txt` declares `OpenXLSX` version `0.5.2`.
- The top of `README.md` describes 0.5.2 as work in progress from a development
  branch and describes 0.5.1 as the latest release in the local copy.
- `vcpkg.json` declares version `0.5.1` and names a GitHub homepage mirror.

Therefore this tree is not recorded as “OpenXLSX 0.5.2 release.” The local
content was compared against a temporary shallow clone of the upstream
`development-aral` ref and matched its commit
`ece329af84b370a8b77f5a3ee0e30509ad0f0bf9` exactly. The repository now records
that commit as the `third_party/openxlsx/source` submodule gitlink.

The remote `v0.5.1` tag resolves to `85e4c1862d1ade5cdcaa001d990d26023a517dc2`;
it is not the source currently evaluated here. Selecting the development commit
is provisional until Phase 5 confirms that its workbook behavior is suitable
for the application. A release tag remains the preferred fallback if the
development-only changes are not required.

## Local Tree Fingerprint

The following identifies the evaluated local tree as it existed on the audit
date:

| Measurement | Value |
| --- | --- |
| Regular files | 154 |
| Total bytes | 2,848,369 |
| Tree SHA-256 | `5b7dc6ef2fafb60dd214281b4fc46f94ddd4de9f083cf3d13bdebf8f61ff4fa3` |
| `CMakeLists.txt` SHA-256 | `9fa4cb2e53a571ff317eeeb564c90ec351de54e734ed78367d4775c96e6e38ee` |
| `LICENSE.md` SHA-256 | `58a42c5572cc6382c1dfd1a16122594477449ae8fbbc7d8e97d33b0174b02407` |
| `vcpkg.json` SHA-256 | `24557d952c630b460374f3074c9f33612ed242db0d48663b31ab092118e00bfd` |

The tree fingerprint is computed by enumerating every regular file recursively,
sorting by normalized relative path, emitting UTF-8 records of the form
`relative/path<TAB>lowercase-file-sha256<LF>`, and hashing the concatenated
records with SHA-256. It is a local evidence fingerprint, not a substitute for
an upstream commit or a distributable source archive. The fingerprint matches
the clean checkout of the recorded upstream commit.

## Candidate Product Build Policy

The product dependency configuration should begin with these explicit settings,
then be validated in Phase 2 against the real WinUI/MSBuild route:

```text
BUILD_SHARED_LIBS=OFF
OPENXLSX_CREATE_DOCS=OFF
OPENXLSX_BUILD_SAMPLES=OFF
OPENXLSX_INSTALL_SAMPLE_SOURCES=OFF
OPENXLSX_BUILD_TESTS=OFF
OPENXLSX_BUILD_BENCHMARKS=OFF
OPENXLSX_ENABLE_LIBZIP=OFF
OPENXLSX_NOWIDE_STANDALONE=ON
OPENXLSX_LOCAL_PACKAGES_ONLY=ON
FETCH_DEPS_AUTO=OFF
FORCE_FETCH_ALL=OFF
```

This prevents product configuration from silently downloading or building
documentation, examples, benchmarks, or upstream tests. It also selects the
default miniz/Zippy ZIP path for the initial build spike. The static-library
link model and whether `OPENXLSX_MONOLITHIC_LIBRARY` is useful remain Phase 2
decisions.

## Declared Dependencies

The local CMake source and manifest identify these dependency alternatives:

| Dependency | Local evidence | Phase 1 disposition |
| --- | --- | --- |
| PugiXML | CMake requires version 1.14; upstream commit `db78afc2b7d8f043b4bc6b185635d949ea2ed2a8` | MIT notice recorded at [`licenses/openxlsx/pugixml/LICENSE.md`](../../licenses/openxlsx/pugixml/LICENSE.md) |
| miniz through Zippy | Used when `OPENXLSX_ENABLE_LIBZIP=OFF`; upstream commit `293d4db1b7d0ffee9756d035b9ac6f7431ef8492` is the CMake `3.0.2` ref | MIT-style notice recorded at [`licenses/openxlsx/miniz/LICENSE`](../../licenses/openxlsx/miniz/LICENSE) |
| Windows Unicode support | Standalone archive `nowide_standalone_v11.3.1.tar.gz`, SHA-256 `eaec4d331e3961f5eeb10c46a11691d62047900a7a40765b0f23cdd3181e6ca6` | Boost Software License 1.0 notice recorded at [`licenses/openxlsx/nowide/LICENSE`](../../licenses/openxlsx/nowide/LICENSE) |
| libzip | Optional when `OPENXLSX_ENABLE_LIBZIP=ON` | Not selected for the initial product build |

The dependency list is an inventory, not an assertion that all alternatives
will ship. The first three entries have exact source/license evidence; their
build and link behavior remains subject to the Phase 2 spike.

## Repository Build Materialization

Phase 2 materializes the selected transitive sources as repository-owned
submodules so a clean checkout can build without a package manager or network
access:

| Source | Repository path | Pinned commit |
| --- | --- | --- |
| PugiXML 1.14 | `third_party/openxlsx/dependencies/pugixml` | `db78afc2b7d8f043b4bc6b185635d949ea2ed2a8` |
| miniz 3.0.2 | `third_party/openxlsx/dependencies/miniz` | `293d4db1b7d0ffee9756d035b9ac6f7431ef8492` |
| standalone nowide v11.3.1-fixed | `third_party/openxlsx/dependencies/nowide` | `66617576eb15c65749cddede010258b693c9a2e7` |

`cmake/platform/windows_winui_openxlsx.cmake` adds those local targets before
configuring OpenXLSX and forces the product-safe options above. Because the
root WinUI project is C++-only while miniz is a C library, the bridge keeps the
miniz sources in C language mode with MSVC `/TC`; no forked source is created.
The static archives are emitted under the active CMake build tree's
`openxlsx/<Configuration>` directory.

`cmake/platform/windows_winui_openxlsx.props.in` is configured into a generated
MSBuild property sheet. The existing WinUI PowerShell wrapper passes that sheet
to the project, which imports it only for the WinUI application. The sheet
contains the OpenXLSX, PugiXML, miniz, and nowide include paths and exact
configuration-local library inputs, so the application cannot silently resolve
an arbitrary developer installation.

## License Record

The upstream `LICENSE.md` has been copied verbatim to
[`../../licenses/openxlsx/LICENSE.md`](../../licenses/openxlsx/LICENSE.md).
It identifies the OpenXLSX code as BSD-3-Clause and credits Kenneth Troldal
Balslev. The upstream CMake SPDX header uses a different copyright year from
the license file; retain both upstream notices while the source revision is
reconciled.

The selected PugiXML, miniz/Zippy, and standalone-nowide notices are recorded
in the files linked above. If Phase 2 selects Boost.Nowide or libzip instead,
their exact source and notice must be added before that alternative is enabled.

## Decision and Remaining Gates

The Phase 1 source-policy decision is to use the repository-owned submodule
pinned to the recorded upstream commit. The local developer directory is not
acceptable as the final input, and the project will not enable
OpenXLSX’s automatic fetch behavior. The initial Windows Unicode policy is the
standalone nowide archive declared by the local OpenXLSX CMake configuration;
the `vcpkg.json` Boost.Nowide alternative is not selected.

Before Phase 1 is complete:

1. record any local patch list, which is currently empty;
2. verify the selected source and dependency artifacts can be configured without
   network access.

## Offline Probe Result

On 2026-09-13, a temporary Ninja configuration used MSVC and the product-safe
options above with `OPENXLSX_LOCAL_PACKAGES_ONLY=ON`, `FETCH_DEPS_AUTO=OFF`, and
`FORCE_FETCH_ALL=OFF`. Compiler detection succeeded. Configuration then failed
at OpenXLSX’s missing local `nowide` target. No dependency was downloaded. The
policy is therefore confirmed, but the final Phase 1 offline-build gate remains
open until Phase 2 provisions and links the pinned native dependency targets.
