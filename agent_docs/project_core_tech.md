# Project Core Technologies

## Languages and Runtimes

- C++23 with C++ extensions disabled.
- Qt 6.12 or newer for the retained desktop product.
- A native Windows WinUI 3 lane, built separately from the Qt desktop target.

## Frameworks and Libraries

- CMake 3.25 or newer is the project build-system baseline.
- The Qt product uses Concurrent, Core, Gui, Network, Pdf, PdfWidgets, PrintSupport, Qml, Quick, QuickControls2, QuickWidgets, Sql, Widgets, and LinguistTools; Qt Test is required for the test suite.
- The engine provides SQLite-backed persistence and uses zlib through the system library or Qt's Windows fallback.
- The Windows WinUI lane uses the Windows App SDK, C++/WinRT, and Windows SDK Build Tools packages declared under `src/platform/windows/winui`.

## Build, Test, and Development Tools

- CMake configure/build presets and CTest organize platform builds and tests.
- Ninja is used by the Linux and macOS presets; Windows presets use Visual Studio 18 2026 with the `v145` toolset.
- Release presets use Qt deployment tooling where applicable so installed products carry their runtime and resources.

## External Services and Infrastructure

- Application update checks use a configurable GitHub Releases API endpoint.
- Resource-pack updates use optional HTTPS manifest and detached-signature endpoints with a configured public key.
- The AI-assisted comment workflow opens a configured AI website; the application does not send observations to an AI API itself.

## Important Technical Constraints

- `ClassMngrEngine` is intentionally Qt-free and is configured before Qt package discovery.
- At least one of the Qt desktop or Windows WinUI products must be enabled at configure time.
- Windows CMake builds require the Visual Studio 18 2026 generator; build directories created with another generator must be freshly reconfigured.
- A build machine needs the compiler/toolchain and Qt where the Qt product is built; deployed release users do not need a separate Qt installation.
