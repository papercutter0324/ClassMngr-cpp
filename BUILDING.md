# Building ClassMngr

ClassMngr is built with CMake, C++23, and Qt 6. Release installs run Qt's deployment step, so the finished app folder or bundle includes the Qt runtime files it needs. The computer that builds ClassMngr must have the build tools and Qt installed; the computer that runs the finished application does not need a separate Qt installation.

## Table of Contents

* [Requirements](#requirements)
* [Windows](#windows)
* [macOS](#macos)
* [Linux](#linux)
* [Tests](#tests)
* [Notes](#notes)

## Requirements

* CMake 3.25 or newer
* Qt 6.11.1 or newer, built for the compiler you are using
* A C++23 compiler
* zlib development files (Windows builds can use Qt's bundled fallback)
* Ninja for Linux and macOS preset builds
* Linux only: `patchelf`, used by Qt's deployment tooling
* Linux only: a configured CUPS or system printer stack for printing
* Windows only: Visual Studio or Build Tools with the Desktop development with C++ workload

When configuring CMake, set `CMAKE_PREFIX_PATH` to the Qt kit directory that contains `lib/cmake/Qt6`. Common examples are:

```text
C:/Qt/6.11.1/msvc2022_64
/Users/you/Qt/6.11.1/macos
$HOME/Qt/6.11.1/gcc_64
```

## Windows

VS Code is the editor for this workflow, but the compiler and Windows SDK still come from Visual Studio or Visual Studio Build Tools.

Install the base tools. You can install them from the official websites, or use `winget` from PowerShell:

```powershell
winget install --id Microsoft.VisualStudioCode -e
winget install --id Git.Git -e
winget install --id Kitware.CMake -e
winget install --id Ninja-build.Ninja -e
winget install --id JRSoftware.InnoSetup -e
```

Install Visual Studio or Visual Studio Build Tools with the C++ workload. In the installer, select:

* Desktop development with C++
* MSVC C++ compiler for your target architecture
* Windows SDK
* C++ CMake tools for Windows

With `winget`, start the Build Tools installation with:

```powershell
winget install --id Microsoft.VisualStudio.2022.BuildTools -e --override "--wait --passive --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
```

Open VS Code and install these extensions:

* C/C++ (`ms-vscode.cpptools`)
* CMake Tools (`ms-vscode.cmake-tools`)

Install the Qt Windows desktop kit next. The recommended route is the Qt Online Installer or Qt Maintenance Tool with the Qt 6.11.1 MSVC desktop kit matching your compiler, such as `msvc2022_64`. Make sure the kit includes Qt Concurrent, Core, Gui, Widgets, Network, Pdf, PdfWidgets, PrintSupport, Sql, Qml, Quick, QuickControls2, QuickWidgets, and LinguistTools. Development builds also require Qt Test.

Set `QT_MSVC_X64_PREFIX` to the Qt kit directory on each Windows machine. The same presets work regardless of where that kit is installed:

```powershell
$qtX64Prefix = "D:\Development\Qt\6.11.1\msvc2022_64"
[Environment]::SetEnvironmentVariable(
    "QT_MSVC_X64_PREFIX",
    $qtX64Prefix,
    "User"
)
$env:QT_MSVC_X64_PREFIX = $qtX64Prefix
```

Use the local path for the machine, such as `C:\Qt\6.11.1\msvc2022_64` on a different installation. On a resource-constrained machine, set `CMAKE_BUILD_PARALLEL_LEVEL` locally or pass `--parallel 4` to `cmake --build`; the shared presets do not impose a job limit.

Open the project from an x64 Native Tools Command Prompt or Developer PowerShell so MSVC is available to VS Code:

```powershell
cd C:\path\to\ClassMngr-cpp
code .
```

In VS Code, use `CMake: Select Configure Preset` and choose `windows-x64-release`. To create a standalone release folder from the terminal, configure, build, and install the release preset:

```powershell
cmake --preset windows-x64-release
cmake --build --preset windows-x64-release
cmake --build --preset windows-x64-release-install
```

Release presets run `windeployqt` after building. The install preset copies the standalone app under `dist/ClassMngr-windows-x64`. Run and distribute the whole installed directory, keeping `ClassMngr.exe` together with the copied Qt DLLs, plugins, QML files, and license files.

ClassMngr supports x64 and ARM64 Windows; it does not produce a Win32 build. Build an x64 installer with:

```powershell
cmake --build --preset windows-x64-release-installer
```

The native ARM64 installer uses `windows-arm64-release-installer` and requires `QT_MSVC_ARM64_PREFIX` to point at the Qt ARM64 kit and `QT_MSVC_X64_PREFIX` to point at the matching x64 host-tools kit. The x64 installer remains usable on Windows 11 ARM through x64 emulation; no Win32 installer is produced.

To build every locally available Windows installer and its SHA-256 checksum file, run:

```powershell
.\scripts\build_release_windows.ps1
```

Installers are written under `dist` as `ClassMngr-<version>-win-{x64,arm64}.exe`. They install the matching Visual C++ runtime prerequisite and support in-place upgrades through a shared application identity.

The Windows presets use the Visual Studio generator configured in `CMakePresets.json`. If CMake reports that the generator is not installed, either install the matching Visual Studio or Build Tools version, or update the preset to the Visual Studio generator installed on your machine.

## macOS

Install and configure Xcode first:

1. Install the current Xcode release from the App Store or Apple Developer.
2. Open Xcode once so it can install any required platform components.
3. Select Xcode as the active developer directory and accept the license:

```sh
sudo xcode-select --switch /Applications/Xcode.app/Contents/Developer
sudo xcodebuild -license accept
xcodebuild -version
xcrun clang++ --version
```

Install the command-line build tools used by the CMake presets. Homebrew is the simplest option:

```sh
brew install cmake ninja git
```

Install the Qt macOS desktop kit next. The recommended route is the Qt Online Installer or Qt Maintenance Tool with the Qt 6.11.1 `macos` desktop kit. If you use another Qt installation, make sure it is Qt 6.11.1 or newer and includes Qt Concurrent, Core, Gui, Widgets, Network, Pdf, PdfWidgets, PrintSupport, Sql, Qml, Quick, QuickControls2, QuickWidgets, and LinguistTools. Development builds also require Qt Test.

Set `QT_MACOS_PREFIX` to that Qt kit. The same presets then work regardless of where Qt is installed. The macOS release preset builds a universal `arm64;x86_64` app bundle targeting macOS 13.0 or newer.

```sh
export QT_MACOS_PREFIX="$HOME/Qt/6.11.1/macos"

cmake --preset macos-clang-release
cmake --build --preset macos-clang-release
cmake --build --preset macos-clang-release-install
```

The standalone app bundle is `dist/ClassMngr-macos/ClassMngr.app`. Build the versioned universal DMG with:

```sh
cmake --build --preset macos-clang-release-installer
```

To configure from scratch, build the DMG, verify its universal app and code signature, and write `dist/checksums-macos.txt` in one step, run `scripts/build_release_macos.sh`. The disk image is written as `dist/ClassMngr-<version>-macos-universal.dmg`.

Do not distribute or launch the release bundle from `build/macos-clang-release` after running deployment tools against it. That build-tree app can mix Qt from the local Qt installation with plugins copied into the bundle, which can make Qt abort while loading the macOS Cocoa platform plugin. Use the installed bundle in `dist/ClassMngr-macos`, or run `scripts/deploy_release_macos.sh` to refresh and validate that installed bundle.

## Linux

Install the native build tools and the common X11/OpenGL development libraries used by Qt desktop apps.

Debian and Ubuntu:

```sh
sudo apt update
sudo apt install \
  build-essential \
  cmake \
  git \
  ninja-build \
  patchelf \
  zlib1g-dev \
  libgl1-mesa-dev \
  libxkbcommon-dev \
  libxkbcommon-x11-dev \
  libxcb1-dev \
  libxcb-cursor-dev \
  libxcb-icccm4-dev \
  libxcb-image0-dev \
  libxcb-keysyms1-dev \
  libxcb-randr0-dev \
  libxcb-render-util0-dev \
  libxcb-shape0-dev \
  libxcb-sync-dev \
  libxcb-xfixes0-dev \
  libxcb-xinerama0-dev \
  libxcb-xkb-dev
```

Fedora:

```sh
sudo dnf install \
  gcc-c++ \
  cmake \
  git \
  ninja-build \
  patchelf \
  zlib-devel \
  mesa-libGL-devel \
  libxkbcommon-devel \
  libxkbcommon-x11-devel \
  libxcb-devel \
  xcb-util-cursor-devel \
  xcb-util-image-devel \
  xcb-util-keysyms-devel \
  xcb-util-renderutil-devel \
  xcb-util-wm-devel
```

Arch Linux and Manjaro:

```sh
sudo pacman -S --needed \
  base-devel \
  cmake \
  git \
  ninja \
  patchelf \
  mesa \
  zlib \
  libxkbcommon \
  libxkbcommon-x11 \
  libxcb \
  xcb-util-cursor \
  xcb-util-image \
  xcb-util-keysyms \
  xcb-util-renderutil \
  xcb-util-wm
```

openSUSE:

```sh
sudo zypper install -t pattern devel_basis
sudo zypper install \
  gcc-c++ \
  cmake \
  git \
  ninja \
  patchelf \
  zlib-devel \
  Mesa-libGL-devel \
  libxkbcommon-devel \
  libxkbcommon-x11-devel \
  libxcb-devel \
  xcb-util-cursor-devel \
  xcb-util-image-devel \
  xcb-util-keysyms-devel \
  xcb-util-renderutil-devel \
  xcb-util-wm-devel
```

Install the Qt Linux desktop kit next. The recommended route is the Qt Online Installer or Qt Maintenance Tool with the Qt 6.11.1 `gcc_64` desktop kit, because many distro repositories ship an older Qt 6 than this project requires. If your distro provides Qt 6.11.1 or newer, distro Qt packages are fine too as long as they include Qt Concurrent, Core, Gui, Widgets, Network, Pdf, PdfWidgets, PrintSupport, Sql, Qml, Quick, QuickControls2, QuickWidgets, and LinguistTools. Development builds also require Qt Test.

The checked-in Linux preset contains a maintainer-specific default Qt path. Override it with the Qt kit installed on your machine:

```sh
cmake --preset linux-gcc-release \
  -DCMAKE_PREFIX_PATH="$HOME/Qt/6.11.1/gcc_64"
cmake --build --preset linux-gcc-release

cmake --install build/linux-gcc-release \
  --prefix dist/ClassMngr-linux-x86_64
```

The standalone app is installed under `dist/ClassMngr-linux-x86_64`. It includes the Qt libraries, plugins, QML files, and license files copied by Qt's deployment tooling, so users do not need Qt installed separately. Distribute the whole installed directory:

```sh
tar -czf dist/ClassMngr-linux-x86_64.tar.gz \
  -C dist \
  ClassMngr-linux-x86_64
```

For a single-file Linux release, create an AppImage from the installed directory with your preferred AppImage packaging tool. Linux packages may still rely on baseline system libraries such as glibc, graphics drivers, and the user's desktop display stack; they should not require a separate Qt installation.

## Tests

Debug presets build the test suite by default. Configure and build the debug preset for your platform, then run CTest from its build directory:

```powershell
cmake --preset windows-x64-debug
cmake --build --preset windows-x64-debug
ctest --test-dir build/windows-x64-debug -C Debug --output-on-failure
```

On Linux or macOS, use the corresponding `linux-gcc-debug` or `macos-clang-debug` preset and omit `-C Debug` because those presets use a single-configuration Ninja generator.

## Notes

* Use release presets when creating distributable builds.
* Run the app from the installed directory or bundle, not directly from a partially copied build tree.
* If CMake cannot find Qt, check that `QT_MACOS_PREFIX`, `QT_MSVC_X64_PREFIX`, `QT_MSVC_ARM64_PREFIX`, or `CMAKE_PREFIX_PATH` points at the matching Qt kit for your compiler and CPU architecture.

