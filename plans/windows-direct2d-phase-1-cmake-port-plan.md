# Phase 1 detailed plan — split the CMake products without changing behavior

## Purpose and completion definition

Phase 0 is accepted; Phase 1 establishes the build and delivery boundaries
needed to start the Windows-native port safely. It does **not** change a
user-visible workflow, move a screen to Direct2D, replace Qt SQL, or publish a
native Windows release.

Phase 1 is complete only when all of the following are true:

- the existing Qt products remain buildable and shippable with their current
  public names, package formats, settings, resources, translations, and
  release workflows;
- the same source tree can configure, build, install, and launch a minimal
  x64 `ClassMngrWindowsNative` executable without a Qt installation, Qt CMake
  package discovery, Qt-generated resources, or Qt binaries in its install
  tree;
- the native build enforces the Windows SDK, Direct2D 1.3 capability, manifest,
  and x64-only contracts recorded in
  [ADR 0001](../docs/porting/adr/0001-windows-native-port-foundations.md);
- `ClassMngrEngine` is a true Qt-free static library with one behaviorally
  tested production slice, and the retained Qt code consumes that slice through
  an adapter; and
- every preservation and native-only check in the validation matrix below is
  green. No Phase 0 visual or parity cell is promoted merely because this
  phase builds.

The detailed plan implements the six Phase 1 bullets in the parent
[port plan](windows-direct2d-directcomposition-port-plan.md#phase-1--split-the-build-without-changing-products).
The accepted Phase 0 evidence and remaining native-parity work remain in the
[current status record](../docs/porting/windows-direct2d/current-status.md).

## Guardrails

- Native Windows is **x64 only** in this phase. The existing Qt ARM64 presets,
  packages, and CI job remain informational compatibility work; do not add a
  native ARM64 preset, package, or release gate.
- Do not rename the installed Qt application, its `.tps` association, updater
  channel, or Inno Setup artifact. A transition target may have an internal
  CMake name, but its installed executable remains `ClassMngr.exe`.
- The native executable has a distinct target name, output name, app identity,
  and development settings namespace. It does not open a real user profile or
  a database in Phase 1.
- Do not broadly move `src/core`, `src/data`, `src/domain`, or feature folders.
  Their present Qt coupling is intentional input to Phase 2. Move only the
  selected semantic-version slice and its tests.
- Do not make `find_package(Qt6)`, `qt_*` commands, `rcc`, QML tooling,
  `lrelease`, `windeployqt`, or a Qt deployment script reachable from a native
  configuration. A native build that happens to succeed on a machine where Qt
  is installed is insufficient evidence.
- Keep the Phase 0 fixture corpus, capture target, parity matrix, and retained
  Qt tests intact. Native-specific output, focus, IME, and performance evidence
  stays carry-forward work for later implementation slices.

## Baseline and current seams

| Concern | Current owner | Phase 1 consequence |
| --- | --- | --- |
| Top-level configuration | `CMakeLists.txt` calls `find_package(Qt6 ...)` before choosing a product target | Target selection must happen before any Qt discovery or Qt command. |
| Production code | `cmake/sources.cmake` links all six object libraries through `ClassMngrBuildSettings`, which carries every Qt module and ZLIB | Split common C++ settings from Qt settings; preserve the Qt runtime until slices move to the engine. |
| Qt executable | `src/main.cpp` and target `ClassMngr` | Keep it as a retained Qt desktop executable with output name `ClassMngr`; give its Windows transition target a distinct internal target name. |
| Resources and translations | `cmake/resources.cmake` owns RCC packs, `qt_add_resources`, QML, and `qt_add_translations` | Extract one source asset/translation catalog, then generate Qt outputs only for Qt targets and a non-Qt native manifest only for native targets. |
| Deployment | `cmake/deployment.cmake` and `cmake/platform/deployment.cmake` invoke Qt deployment; `cmake/platform/windows.cmake` stages the legacy installer | Make Qt deployment explicitly Qt-only. Add native install rules but leave the existing installer and release artifact target bound to the Qt transition product. |
| Windows constraints | `resources/windows/ClassMngr.rc.in`; `ClassMngrQtVisualCapture.manifest`; ADR 0001 | Add a native resource/manifest pair and an executable test that verifies the embedded manifest, not merely the source template. |
| Test topology | `cmake/tests.cmake` and the Phase 0 capture target assume `ClassMngrRuntime` and Qt | Retain this graph for Qt builds. Add a small ordinary CTest executable for engine/native checks that is available in a no-Qt configuration. |
| Windows CI | `.github/workflows/windows-release.yml` installs Qt and publishes Qt x64/ARM64 installers | Leave it as the legacy release route. Add a separate native x64 validation workflow that never installs Qt and never uploads a release artifact. |

## Intended target graph

```text
                         ClassMngrCommonBuildSettings
                           |                     |
                           |                     +-- ClassMngrWindowsNativeSdk
                           |                              |
                     ClassMngrEngine                       |
                 (no Qt or platform UI types)              |
                           |                                |
         +-----------------+----------------+               |
         |                                  |               |
ClassMngrQtRuntime                  ClassMngrEngineTests     |
(retained object libraries,                                      |
 Qt adapters)                                                    |
         |                                                       |
         +-- ClassMngrQtDesktop (macOS/Linux)                   |
         +-- ClassMngrWindowsQtTransition (Windows)             |
               output: ClassMngr.exe                             |
                                                                  |
                                            ClassMngrWindowsNative
                                            output: ClassMngrNative.exe
```

`ClassMngrWindowsQtTransition` exists only to make CMake dependencies
unambiguous during the port. Its output file, install layout, Inno Setup
input, package name, and update behavior remain those of the present Qt
product. `ClassMngrWindowsNative` remains a development executable and is
never selected by `ClassMngrInstaller`, the updater, or release publishing in
this phase.

## Build-selection contract

Introduce these cache options in a small target-selection module, evaluated
before any Qt or ZLIB discovery:

| Option | Default | Meaning |
| --- | --- | --- |
| `CLASSMNGR_BUILD_QT_DESKTOP` | `ON` | Build the retained Qt product and its Qt tests. Required on macOS/Linux in Phase 1. |
| `CLASSMNGR_BUILD_WINDOWS_NATIVE` | `OFF` | Build the Windows-native shell, engine tests, and native install rules. Fatal outside Windows. |
| `CLASSMNGR_BUILD_WINDOWS_QT_TRANSITION` | `ON` on Windows when the Qt desktop is enabled | Select the internally named Windows Qt transition executable. Fatal if enabled without `CLASSMNGR_BUILD_QT_DESKTOP`. |

The valid configurations are deliberately narrow:

- macOS/Linux: Qt desktop on, native off. These remain the normal presets.
- Windows Qt transition: Qt desktop on, transition on, native optional. The
  existing `windows-*` presets retain this behavior.
- Windows native-only: Qt desktop off, transition off, native on. This is the
  proof configuration and may not reference a Qt prefix.
- Windows dual build: both products on, permitted for local transition work
  only. The two executables must have unique target and output names. It is not
  the no-Qt proof and is not a release configuration.

Fail configuration with an actionable error for every other combination,
including all-products-off and native enabled off Windows. Make the selected
product shape visible in the CMake configure summary.

## Implementation sequence

### 1. Establish the CMake selection boundary

**Files:** `CMakeLists.txt`; new `cmake/targets.cmake`; include updates in
`cmake/sources.cmake`, `cmake/resources.cmake`, `cmake/deployment.cmake`, and
`cmake/tests.cmake`.

1. Move only target-neutral setup ahead of product selection: project version,
   C++23 policy, CTest, generated `build_info.h`, Git revision, and generic
   compile options. Retain `enable_language(RC)` for Windows because both
   Windows products embed resources.
2. Evaluate the three options and validate their combinations before
   `find_package(Qt6)`, ZLIB lookup, `qt_standard_project_setup`, or inclusion
   of a file that executes a `qt_*` command.
3. Gate the current Qt cache-cleanup block, `find_package(Qt6 ...)`, zlib
   fallback, Qt project setup, Qt resources, Qt deployment, and Qt tests behind
   `CLASSMNGR_BUILD_QT_DESKTOP`. A native-only cache must have neither
   `Qt6_DIR` nor any `Qt6*_*_DIR` entry created by this project.
4. Include the native platform and native target modules only when
   `CLASSMNGR_BUILD_WINDOWS_NATIVE` is on. They must use ordinary CMake
   commands and MSVC/Windows SDK facilities only.
5. Keep default configure behavior compatible with the existing presets. A
   developer using `windows-x64-debug`, `linux-gcc-debug`, or
   `macos-clang-debug` still receives the retained Qt product; no platform
   product is silently switched to native.

**Checks:** configure each valid shape from a fresh build directory; assert
that the invalid combinations fail at configuration with the documented error.
Review `CMakeCache.txt` from a native-only configure for absent Qt entries.

### 2. Split common, engine, and Qt build settings without broad source moves

**Files:** refactor `cmake/sources.cmake`; add `cmake/engine.cmake` and
`cmake/qt_desktop.cmake`; add `src/engine/` and `tests/engine/`.

1. Replace the globally Qt-linked `ClassMngrBuildSettings` with:
   - `ClassMngrCommonBuildSettings`: C++23, source/generated include roots,
     warnings/definitions that are genuinely common, and no linked libraries;
   - `ClassMngrQtBuildSettings`: the retained Qt modules, ZLIB, Qt-specific
     definitions, and `ClassMngrCommonBuildSettings`; and
   - `ClassMngrWindowsNativeSdk`: Windows include definitions and explicit SDK
     libraries, described in step 4.
2. Retain the existing object-library source grouping initially, but make it
   explicitly Qt-owned (`ClassMngrQtCore`, `ClassMngrQtData`,
   `ClassMngrQtDomain`, `ClassMngrQtUiShared`, `ClassMngrQtFeatures`, and
   `ClassMngrQtAppServices`). The grouping is transitional; no source path
   changes are required.
3. Assemble those objects into `ClassMngrQtRuntime`, not an engine target.
   Link it to `ClassMngrQtBuildSettings` and, after step 3, to
   `ClassMngrEngine`. Preserve the current Apple test-runtime/interposition
   handling with the renamed runtime target; do not weaken test-double support.
4. Create `ClassMngrEngine` as a static library using only
   `ClassMngrCommonBuildSettings`. Its public headers live below
   `src/engine/include/classmngr/engine/`; private implementation stays below
   `src/engine/`. No engine public header may include project Qt headers,
   `<Qt...>`, or Win32 headers.
5. Add an ordinary (non-QtTest) `ClassMngrEngineTests` CTest executable. It
   must link only `ClassMngrEngine` and the C++ runtime. A configuration-time
   target audit rejects any `Qt6::` item in the direct or transitive engine
   link interface; a source audit rejects Qt includes in engine sources and
   public headers.

**Checks:** build the retained Qt runtime and all current tests on Windows,
Linux, and macOS. In the native-only tree, build `ClassMngrEngineTests` and
confirm its link command contains no Qt library.

### 3. Move one proven behavior into the engine

**Files:** replace the semantic implementation beneath
`src/core/updater/version.h/.cpp`; add
`src/engine/include/classmngr/engine/semantic_version.h`,
`src/engine/semantic_version.cpp`, `tests/engine/semantic_version_tests.cpp`,
and a focused Qt adapter-contract test.

The initial slice is the updater semantic-version parser and comparator. It
is small, deterministic, and currently isolated enough to expose the boundary
without pulling in persistence, UI, network, or translation behavior.

1. Define `classmngr::engine::SemanticVersion` with UTF-8 `std::string` input
   and output, explicit parse errors, immutable numeric components, and the
   existing comparison semantics. Use standard C++ parsing rather than Qt
   regular expressions.
2. Keep the present Qt-facing `Version` API as a thin adapter during the
   transition. It converts `QString` at the boundary, delegates parsing and
   comparison to `SemanticVersion`, and converts its result back without
   changing caller-visible success, failure, whitespace, or ordering behavior.
   It must not own a second parser.
3. Build a table of accepted values, rejected values, whitespace handling,
   very large/invalid components, formatting, and all comparison relations
   from existing updater tests. Run the same table against the engine and the
   Qt adapter, proving semantic equivalence before deleting the duplicated Qt
   implementation.
4. Leave updater networking, signature verification, Qt dialogs, and update
   installation in the retained Qt runtime. Their interfaces cross to the
   engine only in later vertical slices.

**Checks:** engine tests run in native-only CTest; the focused adapter test and
all existing updater tests pass in Qt configurations. The public Qt header
continues to compile for current callers, so this refactor changes no product
contract.

### 4. Add the native Windows SDK and ABI guard

**Files:** add `cmake/platform/windows_native.cmake`,
`src/platform/windows/native/sdk_capability.cpp`, and
`tests/windows/native_sdk_capability_tests.cpp` (or one test executable with
the capability source); update `CMakePresets.json` and native CI.

1. Require Visual Studio's Windows SDK `10.0.26100.0` for a native configure.
   Use `CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION` in the native presets and
   fail configuration if that exact installed SDK cannot be selected.
2. Before building the shell, compile and link a native capability target that
   includes `sdkddkver.h` and `d2d1_3.h`, asserts
   `NTDDI_VERSION >= NTDDI_WIN10_RS2`, and names
   `ID2D1Factory3`, `ID2D1Device2`, and `ID2D1DeviceContext2`. Failure to find
   the header or interfaces is a hard error; do not substitute a newer or
   older Direct2D interface.
3. Make `ClassMngrWindowsNativeSdk` link the native target explicitly to
   `d2d1`, `dwrite`, `d3d11`, `dxgi`, `dcomp`, `windowscodecs`, `ole32`,
   `uuid`, `shcore`, `bcrypt`, `crypt32`, and `psapi`. This declares the
   intended Windows foundation without bringing them through a Qt target.
4. Apply `UNICODE`, `_UNICODE`, `WIN32_LEAN_AND_MEAN`,
   `_WIN32_WINNT=0x0A00`, and `NTDDI_VERSION=NTDDI_WIN10_RS2` consistently to
   native compilations. The manifest and launcher enforce the more precise
   Windows 10 version 1703 runtime floor.
5. Expose the selected SDK version and capability-test result in configure and
   CI logs. The test is an ABI/build guard only; graphics-device creation,
   fallback, composition, and recovery remain Phase 3.

**Checks:** native Debug and Release configurations reject an absent/wrong SDK
or missing `d2d1_3.h`; the capability executable builds and passes on the
Windows runner.

### 5. Create the isolated minimal native shell

**Files:** add `src/platform/windows/native/main.cpp`,
`native_application.{h,cpp}`, `native_identity.h`, and
`resources/windows/ClassMngrNative.rc.in` plus
`resources/windows/ClassMngrNative.manifest.in`.

1. Create the `WIN32` executable target `ClassMngrWindowsNative`, linked only
   to `ClassMngrEngine`, `ClassMngrCommonBuildSettings`, and
   `ClassMngrWindowsNativeSdk`. Its output is `ClassMngrNative.exe`, so it can
   coexist with `ClassMngr.exe` in a dual build without installer ambiguity.
2. Implement only the shell necessary to prove a native launch: COM apartment
   initialization, UTF-16 command-line parsing, a registered Win32 window
   class, a minimal message loop, orderly close, and a deterministic
   `--phase1-smoke-test` path that creates then destroys the HWND and exits
   successfully. Do not add a Direct2D renderer, navigation, database open,
   updater, resource pack mount, or hidden Qt dependency.
3. Put the native development identifiers in one header: a distinct
   AppUserModelID and a distinct `HKCU\\Software\\PaperCloud\\ClassMngrNative`
   settings root. The Phase 1 smoke path must neither read nor write that root
   or user databases; defining it now prevents later accidental collision with
   the Qt product.
4. Generate a dedicated RC file that embeds the existing app icon and a
   native manifest. The manifest must contain the Windows 10/11 compatibility
   GUID, `PerMonitorV2, PerMonitor, System`, `longPathAware=true`, and
   `asInvoker`, exactly as ADR 0001 requires.
5. Add a native shell smoke test that launches the installed executable with
   `--phase1-smoke-test`. Add a separate embedded-manifest test that reads the
   executable's `RT_MANIFEST` resource and asserts each required declaration.
   Test the binary resource, not just the `.in` file.
6. Keep the runtime version guard intentionally small: before normal native
   UI startup, reject Windows 10 builds below 15063 with a clear message and a
   nonzero exit. The smoke-test mode may expose the detected version in its
   report so CI can validate the code path without requiring an obsolete host.

**Checks:** the native executable launches in a clean install directory,
passes the smoke test, has the expected x64 PE machine type, contains the
manifest declarations, and imports no `Qt6*.dll`.

### 6. Split resource, translation, install, and installer ownership

**Files:** split `cmake/resources.cmake` into a shared catalog plus
`cmake/qt_resources.cmake` and `cmake/native_resources.cmake`; split
`cmake/deployment.cmake`; minimally parameterize
`cmake/platform/windows.cmake`; add a native resource-manifest verifier.

1. Extract the currently scattered asset globs, explicit embedded assets,
   scoped resource-pack inputs, QML inputs, and `.ts` inputs into one
   target-neutral catalog. This catalog is data only: it may not call a Qt
   command or assume an executable target named `ClassMngr`.
2. Keep the present RCC packs, `qt_add_resources`, QML module, and
   `qt_add_translations` behavior in `qt_resources.cmake`, invoked only for
   the Qt product. Preserve Qt resource aliases and installed pack layout.
3. Add `native_resources.cmake`, which reads the same catalog and writes a
   deterministic UTF-8 native asset manifest. Every entry records the
   project-relative source path, logical resource key, size, and SHA-256.
   It records translation source files as inputs but does not run Qt Linguist
   or choose a native translation runtime in this phase.
4. Install the native executable, its native asset manifest, icon/license, and
   any assets required by the shell to a separate native staging prefix. The
   manifest generation and install must work with no `rcc`, QML tool, Qt
   Linguist, or Qt package present. Later phases may add a native pack reader
   without changing the catalog contract.
5. Make `windeployqt`, `qt6_generate_deploy_script`, QML deployment, and Qt
   translation deployment explicit responsibilities of the Qt deployment
   module. They must not be evaluated by native install rules.
6. Preserve `ClassMngrInstaller` as the legacy Qt installer target, still
   staging `ClassMngr.exe`, existing resources, Qt runtime, VC redistributable,
   associations, and Inno Setup output name. If a native installer target is
   useful for developer smoke testing, name it
   `ClassMngrWindowsNativeDevPackage`, stage it separately, and do not add it
   to release workflows or update metadata.
7. Verify the native manifest against its source catalog in CTest so a new
   asset cannot silently appear only in one product's future resource list.

**Checks:** build/install native Release in a directory with no Qt files;
assert the native manifest hashes and paths match the catalog; run the native
smoke test from the staged install. Build the current Qt installer and verify
that its staging layout and Inno Setup input remain unchanged.

### 7. Add isolated presets and CI proof

**Files:** `CMakePresets.json`; new
`.github/workflows/windows-native-build.yml`; limited documentation updates in
the port status record after results exist.

1. Add `windows-x64-native-debug` and `windows-x64-native-release` configure
   presets inheriting only `windows-msvc-base` and `base`, not
   `qt-windows-x64`. Set x64 architecture, SDK `10.0.26100.0`,
   `CLASSMNGR_BUILD_QT_DESKTOP=OFF`,
   `CLASSMNGR_BUILD_WINDOWS_QT_TRANSITION=OFF`, and
   `CLASSMNGR_BUILD_WINDOWS_NATIVE=ON`.
2. Add matching build and install presets. Use a native-only staging directory
   distinct from `dist/ClassMngr-windows-x64`, which remains reserved for the
   Qt release installer.
3. Leave all existing Qt presets in place, including `windows-x64-debug`,
   `windows-x64-release`, the Phase 0 visual preset, and ARM64 presets. They
   remain the retained product and evidence routes.
4. Create a Windows x64 native CI workflow that installs Visual Studio/CMake
   tooling but deliberately does **not** install Qt or set a Qt prefix. It
   configures native Debug and Release from fresh directories; builds;
   runs engine, capability, manifest, and shell smoke tests; installs; checks
   the PE architecture/import list; and fails if the stage contains Qt DLLs,
   plugin directories, QML, `qt.conf`, or `ClassMngr.exe`.
5. Retain `.github/workflows/windows-release.yml` as the existing Qt x64/ARM64
   release job and leave `publish-binaries.yml` expecting those existing eight
   release assets. Do not advertise or upload native Phase 1 builds.
6. Ensure the native workflow's path filters include top-level CMake, presets,
   `cmake/**`, `src/engine/**`, `src/platform/windows/**`,
   `resources/windows/**`, resources/catalog files, tests, and the workflow.
   Retained Qt release workflows continue to cover their existing paths.

**Checks:** CI logs prove that the native job did not download Qt and that a
fresh native cache contains no Qt package entries. CI also continues to build
the retained macOS/Linux Qt applications and Windows Qt release product from
the same commit.

### 8. Complete the preservation sweep and record the handoff

**Files:** tests affected by runtime-target renaming; this plan; then
`docs/porting/windows-direct2d/current-status.md` only after measured results
exist.

1. Replace hard-coded CMake references to the old `ClassMngr` runtime target
only where required by the target split. Check test helpers, deployment,
resource-pack dependencies, installer dependencies, macOS test-runtime
interposition, and CI scripts. Do not perform cosmetic renames outside this
set.
2. Run the existing Phase 0 contract validator unchanged. Keep the opt-in Qt
visual capture target buildable under the retained Windows Qt configuration;
it need not run in native CI.
3. Execute the retained-platform builds and suites on their supported hosts:
   Windows Qt Debug test suite (including the fixture verifier), Linux Qt
   Debug/Release tests and install, and macOS Qt Debug/Release tests and
   universal build. Record any unrelated pre-existing failure separately; do
   not weaken a test to close Phase 1.
4. Once the checks are green, update the current status record with the native
   source revision, selected SDK, CI run links/identifiers, native install
   smoke result, and explicit statement that only the build boundary—not
   feature parity—has passed.

## Validation matrix and commands

| Gate | Evidence |
| --- | --- |
| Native configuration is Qt-free | Native workflow installs no Qt; fresh configure has no `Qt6_DIR`/component cache entries and log has no `find_package(Qt6)` result. |
| Required Windows baseline | SDK `10.0.26100.0` selected; capability target compiles `d2d1_3.h` and the three required interfaces with RS2-or-newer macros. |
| Engine boundary | `ClassMngrEngineTests` builds/runs in native-only CTest; target/source audits show no Qt types, includes, or linked Qt targets. |
| Behavior preservation | Semantic-version engine and Qt adapter use the same test table; retained updater tests remain green. |
| Native binary | x64 PE check; actual embedded-manifest test; no Qt imports; `--phase1-smoke-test` passes before and after install. |
| Native install | Native catalog manifest verifies; installed stage contains no `Qt6*.dll`, `plugins`, `qml`, `translations`, or `qt.conf`. |
| Windows Qt transition | Existing `windows-x64-debug` suite and opt-in Phase 0 target still configure; existing x64 installer build/stage smoke test remains green. |
| macOS/Linux retention | Existing Qt Debug/Release configure, test, install, and package workflows remain unchanged and green. |
| Release isolation | Existing Qt x64/ARM64 artifacts remain the only artifacts published; no native build appears in updater metadata or `publish-binaries.yml`. |

The exact command names are added with their presets, but acceptance must be
equivalent to the following:

```powershell
# Native-only: do not set CMAKE_PREFIX_PATH or any QT_* variable.
cmake --preset windows-x64-native-debug
cmake --build --preset windows-x64-native-debug --parallel 2
ctest --test-dir build/windows-x64-native-debug -C Debug --output-on-failure

cmake --preset windows-x64-native-release
cmake --build --preset windows-x64-native-release --parallel 2
cmake --install build/windows-x64-native-release --config Release
```

```powershell
# Retained Windows Qt regression route; still requires the present Qt kit.
cmake --preset windows-x64-debug
cmake --build build/windows-x64-debug --config Debug --parallel 2
ctest --test-dir build/windows-x64-debug -C Debug -LE visual --output-on-failure

# Existing Phase 0 evidence contract remains valid and unchanged.
.\scripts\porting\windows\validate_phase0_contracts.ps1 -ProjectRoot .
```

Run the Linux and macOS commands recorded in the
[current status record](../docs/porting/windows-direct2d/current-status.md)
on their supported hosts. The native-only job is additional evidence, not a
substitute for those retained-platform gates.

## Ordered review checkpoints

1. **Selection review:** fresh native-only configure has no Qt cache entries;
   fresh Qt configure still builds the current target.
2. **Boundary review:** engine link/source audit passes and the semantic-version
   adapter demonstrates unchanged Qt behavior.
3. **Windows foundation review:** SDK capability and binary manifest checks pass
   on Windows 10/11 x64; the shell has no renderer or data side effects.
4. **Packaging review:** native install has no Qt files, while a legacy Qt
   installer stage still launches with its existing resources and runtime.
5. **Cross-platform review:** retained Windows, Linux, and macOS validation is
   green; status evidence is recorded with source revision and host details.

Do not start Phase 2 until checkpoint 5 is complete. If a later Phase 1 step
reveals a need to copy product rules into the native shell, stop and extract a
proper engine interface instead; duplicated behavior is outside this phase.

## Risks and responses

| Risk | Response and blocking signal |
| --- | --- |
| CMake cache preserves a Qt component from an earlier configuration | Require `--fresh` native CI, inspect native cache, and keep Qt discovery behind the selection boundary. A Qt cache entry blocks the gate. |
| A global `ClassMngr` reference silently redirects packaging/tests to the native target | Parameterize target ownership, retain explicit Qt transition target dependencies, and test both installer staging and native staging. Any collision blocks the gate. |
| The engine becomes a second implementation of existing rules | Move the selected semantic-version implementation once; force Qt adapter tests against the same cases. No copied parser remains after the slice. |
| Native resource lists diverge from Qt resource inputs | One neutral catalog plus deterministic manifest/hash verification. A source asset missing from one catalog consumer blocks the gate. |
| The Windows SDK is accepted but lacks the required Direct2D interfaces | Compile the required types from `d2d1_3.h` in CI and fail configuration/build immediately. |
| A native test only validates templates, not the shipped binary | Read `RT_MANIFEST` from `ClassMngrNative.exe`, smoke-test the installed executable, and inspect its PE imports. |
| A provisional native build leaks into release/updater paths | Use a unique output/staging name, leave release workflows unchanged, and assert no native artifact is uploaded or referenced in update metadata. |

## Explicitly deferred

- Direct3D/Direct2D device creation, DirectComposition visuals, rendering,
  device-loss recovery, DPI-change handling, frame scheduling, and renderer
  capture tests (Phase 3).
- SQLite C API, data migrations, use cases, filesystem/network/service
  interfaces, reports, resource-pack readers, and most domain extraction
  (Phase 2).
- Native navigation, controls, editable text, Korean IME validation, dialogs,
  printing, PDF, import/export, updater UI, PowerPoint, and feature parity.
- Native installer/updater channel publication, official ARM64 native support,
  UI Automation, Narrator, and high-contrast support.

These deferrals are intentional. Passing the Phase 1 gate proves an isolated,
buildable native foundation and protects the current products; it does not
claim native application parity.
