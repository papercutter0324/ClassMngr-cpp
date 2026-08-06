# ClassMngr GitHub Release Updates

ClassMngr checks the public GitHub Releases API for published stable releases:

```text
https://api.github.com/repos/papercutter0324/ClassMngr-cpp/releases?per_page=10
```

The application ignores drafts, prereleases, malformed releases, and releases
that do not yet contain a complete asset for the current platform. It selects
the highest compatible strict `vX.Y.Z` version.

## Release assets

The release tag and the version in each versioned asset name must match:

```text
ClassMngr-X.Y.Z-win-x64.exe
ClassMngr-X.Y.Z-win-arm64.exe
ClassMngr-X.Y.Z-macos-universal.dmg
ClassMngr-X.Y.Z-linux-x86_64.tar.gz
```

The current legacy Linux name, `ClassMngr-linux-x86_64.tar.gz`, is also
accepted. GitHub must report a positive size and a `sha256:` digest for the
selected asset. ClassMngr verifies both values after downloading.

Windows ARM64 prefers the native installer and can fall back to the x64
installer. macOS uses the universal disk image. Linux downloads the archive
and reveals it in the system file manager for manual replacement.

## Build configuration

The public API endpoint and automatic startup check are configured with:

```sh
cmake --preset <release-preset> \
  -DCLASSMNGR_UPDATE_API_URL="https://api.github.com/repos/papercutter0324/ClassMngr-cpp/releases?per_page=10" \
  -DCLASSMNGR_UPDATE_CHECK_ON_STARTUP=ON
```

These are already the defaults. Override the endpoint only for development,
testing, or a repository migration.

## Release publishing

The existing publish workflow builds and uploads the platform packages and
their checksum files. A newly published release may be visible before its
assets finish uploading; clients skip that incomplete release and continue
using the newest complete stable release until a later check.

For the smallest incomplete-release window, create releases as drafts, attach
and verify every asset, and publish the release last.
