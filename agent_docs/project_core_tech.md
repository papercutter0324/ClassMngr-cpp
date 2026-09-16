# Project Core Technologies

## Languages and Runtimes

- C++23, required by the shared CMake build settings.
- Qt 6 desktop runtime. The top-level CMake configuration currently requires
  Qt 6.12.0 or newer for the application; Qt Test is additionally required
  when `BUILD_TESTING` is enabled.
- Supported build targets described by the presets are Windows x64/ARM64,
  Linux GCC, and a macOS universal `arm64;x86_64` bundle targeting macOS
  14.4 or newer.

## Frameworks and Libraries

The application links Qt Concurrent, Core, Gui, Network, Pdf, PdfWidgets,
PrintSupport, Qml, Quick, QuickControls2, QuickWidgets, Sql, Widgets, and
LinguistTools. It uses Qt Widgets and Qt Quick/QML, including QML for the
calendar UI. SQLite is accessed through Qt SQL (`QSQLITE`), and zlib is a
required dependency with a Qt bundled fallback available on Windows.

## Build, Test, and Development Tools

CMake 3.25+ and the checked-in `CMakePresets.json` define configuration,
build, install, and selected artifact workflows for Debug and Release only.
Windows multi-config generators are constrained to `Debug;Release`; Linux
and macOS retain single-config `CMAKE_BUILD_TYPE`. Ninja is used by the Linux
and macOS presets; Windows uses the Visual Studio/MSVC generator. Qt deployment
tools package release runtimes, and CTest runs the registered Qt test suite.
Linux release deployment also uses `patchelf`.

## External Services and Infrastructure

The default application update endpoint is the GitHub Releases API. Resource
pack updates use separately configured HTTPS manifest/signature endpoints and
a PEM public key. The AI-assisted comment workflow launches a configured
website rather than calling an AI API. (CMakeLists.txt, README.md)

## Important Technical Constraints

- Native workspace output uses `.tps`; legacy `.db` input remains supported.
- Release distribution must include the installed Qt deployment output rather
  than relying on Qt being installed on the user's machine.
- Qt kit paths are supplied through platform-specific environment variables or
  `CMAKE_PREFIX_PATH`, and must match the compiler and target architecture.
- Resource-pack signature enforcement defaults to enabled, while the dormant
  resource-pack startup check defaults to disabled. (CMakeLists.txt)
- `CMakeLists.txt`, `BUILDING.md`, active CI workflows, and the macOS release
  helper now use or require Qt 6.12.0.
