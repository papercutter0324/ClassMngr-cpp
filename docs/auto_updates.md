# ClassMngr Auto-Update Releases

ClassMngr uses a single channel manifest as the source of truth. Versioned
folders are for humans, rollback, and audit history.

Canonical updater base folder (`ClassMngr/`):
https://drive.google.com/drive/folders/14EtU_WYBcN-d-vMi3ItjsiGfQ3POH8sB?usp=sharing

```text
ClassMngr/
  channels/
    stable/
      latest.json
      latest.sig
    beta/
      latest.json
      latest.sig

  releases/
    0.1.1/
      release.json
      release-notes.md
      checksums.txt
      windows/
        ClassMngr-0.1.1-win-x64.exe
        ClassMngr-0.1.1-win-arm64.exe
        ClassMngr-0.1.1-win-x86.exe  # optional
      macos/
        ClassMngr-0.1.1-macos-universal.dmg
      linux/
        ClassMngr-0.1.1-linux-x86_64.AppImage
```

The app fetches only the direct URL for `channels/stable/latest.json`. Upload
release artifacts first, then publish `latest.json` and `latest.sig` last. A
shared folder URL is useful for humans, but release builds still need direct
HTTPS URLs for `latest.json` and `latest.sig`.

## Build Configuration

Configure updater endpoints at build time:

```sh
cmake --preset macos-clang-release \
  -DCLASSMNGR_UPDATE_MANIFEST_URL="https://..." \
  -DCLASSMNGR_UPDATE_SIGNATURE_URL="https://..." \
  -DCLASSMNGR_UPDATE_PUBLIC_KEY_PEM="-----BEGIN PUBLIC KEY-----...-----END PUBLIC KEY-----"
```

The Windows release presets are intended to run on a Windows MSVC runner or VM.
For local Windows builds, Qt 6.11.1 is installed at `D:\Development\Qt\6.11.1`:

```powershell
$env:QT_MSVC_X64_PREFIX="D:\Development\Qt\6.11.1\msvc2022_64"
$env:QT_MSVC_ARM64_PREFIX="D:\Development\Qt\6.11.1\msvc2022_arm64"
```

VS Code supplies these values automatically, and
`scripts/build_release_windows.ps1` uses them as defaults. Explicit environment
variables still take precedence, which allows CI to use its runner-specific Qt
installation under `C:\Qt`.

`QT_MSVC_X86_PREFIX` is optional and should only be set when a compatible
32-bit Qt MSVC kit is available.

`CLASSMNGR_UPDATE_REQUIRE_SIGNATURE` defaults to `ON`. With that default,
ClassMngr refuses to parse update URLs unless the manifest signature verifies
with the embedded public key.

## Manifest

Use strict `x.x.x` versions.

```json
{
  "schemaVersion": 1,
  "channel": "stable",
  "latestVersion": "0.1.1",
  "minimumSupportedVersion": "0.1.0",
  "releaseDate": "2026-06-18",
  "notesUrl": "https://example.com/release-notes",
  "platforms": {
    "windows-x64": {
      "url": "https://example.com/ClassMngr-0.1.1-win-x64.exe",
      "fileName": "ClassMngr-0.1.1-win-x64.exe",
      "sha256": "64 lowercase hex characters",
      "sizeBytes": 12345678
    },
    "windows-arm64": {
      "url": "https://example.com/ClassMngr-0.1.1-win-arm64.exe",
      "fileName": "ClassMngr-0.1.1-win-arm64.exe",
      "sha256": "64 lowercase hex characters",
      "sizeBytes": 12345678
    },
    "macos-universal": {
      "url": "https://example.com/ClassMngr-0.1.1-macos-universal.dmg",
      "fileName": "ClassMngr-0.1.1-macos-universal.dmg",
      "sha256": "64 lowercase hex characters",
      "sizeBytes": 12345678
    },
    "linux-x86_64": {
      "url": "https://example.com/ClassMngr-0.1.1-linux-x86_64.AppImage",
      "fileName": "ClassMngr-0.1.1-linux-x86_64.AppImage",
      "sha256": "64 lowercase hex characters",
      "sizeBytes": 12345678
    }
  }
}
```

Generate checksums with:

```sh
shasum -a 256 path/to/artifact
```

Sign the exact `latest.json` bytes with an RSA private key that matches the
embedded public key:

```sh
openssl dgst -sha256 -sign private.pem -out latest.sig latest.json
```

On Windows, if `openssl` is not on `PATH`, Git for Windows commonly includes
it. In PowerShell, call it with the full path:

```powershell
& "C:\Program Files\Git\usr\bin\openssl.exe" dgst -sha256 -sign private.pem -out latest.sig latest.json
```
