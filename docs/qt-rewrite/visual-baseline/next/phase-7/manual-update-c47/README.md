# C47 — Next manual update evidence

Task ID: `phase7_c47_manual_update_implementation_01`.

## Route and behavior

Next **Help → Check for Updates…** opens the shared updater dialog through a
bootstrap-owned coordinator. Startup and manual checks use one service, and the
manual action remains available when automatic checks are disabled. Coverage
includes retry, typed skip/unskip preferences, shared cache and busy results,
and download pause, discard, cancel, and offline saved-fixture verification.

The shared dialog keeps its legacy resource-pack placeholder shown by default.
Next selects the typed omission option, so the obsolete frame and label are not
constructed; the coordinator uses no private widget lookup.

## Capture review

`captures.json` lists 36 Qt Widgets captures: nine states across English and
Korean, each in light and dark themes. They use Fusion, the offscreen platform,
and scale factor 1 on macOS. The states are checking, configuration error,
available, skipped, downloading, paused, install readiness, refresh error, and
up to date. The download and install captures use fake services and deterministic
signals.

A fresh recapture matched all 36 PNGs and the capture manifest byte-for-byte;
the images were independently reviewed. All Close and Not Now controls are
present. The earlier missing-button report was corrected after original-
resolution review. `SHA256SUMS` covers the captures and images.

## Verification and limits

The build, seven focused CTests, and fresh integration cases passed. Resource
inventory `--check` passed against the working-tree translation bytes. The
Korean change is limited to `Released: %1` and `Latest Version: %1%2`; the
inventory is a working-tree validation snapshot and will be refreshed after the
translation source is committed.

No real network request or platform installer process was launched. Windows and
the full test suite were not run. An optional legacy loopback probe failed to
bind before sending a request; Next's fake-based coverage passed.

To regenerate the captures after building the Next resource package and test
target:

```sh
QT_QPA_PLATFORM=offscreen QT_SCALE_FACTOR=1 \
CLASSMNGR_C47_CAPTURE_DIR="$PWD/docs/qt-rewrite/visual-baseline/next/phase-7/manual-update-c47" \
build/macos-clang-debug/ClassMngrNextUpdateCoordinatorTests bilingualThemeCaptures
```
