param(
    [Parameter(Mandatory=$true)][string]$RepositoryRoot,
    [Parameter(Mandatory=$true)][string]$RunRoot
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$RepositoryRoot = [IO.Path]::GetFullPath($RepositoryRoot)
$RunRoot = [IO.Path]::GetFullPath($RunRoot)
. (Join-Path $RepositoryRoot 'scripts/phase9/p9_14_packaged_lifecycle_common.ps1')

$exactRoot = [IO.Path]::GetFullPath((Join-Path $RepositoryRoot 'artifacts/phase9/p9-18-exact-release-builds')).TrimEnd([char[]]@('\','/')) + [IO.Path]::DirectorySeparatorChar
if (-not $RunRoot.StartsWith($exactRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'P9-18 build RunRoot must be a fresh child of artifacts/phase9/p9-18-exact-release-builds.' }
if (Test-Path -LiteralPath $RunRoot) { throw "Refusing to overwrite P9-18 exact-source build evidence: $RunRoot" }
[void][IO.Directory]::CreateDirectory((Join-Path $RunRoot 'logs'))
$buildDirectory = Join-Path $RunRoot 'build'
$standardRelease = [IO.Path]::GetFullPath((Join-Path $RepositoryRoot 'build/windows-x64-release'))
$preservedPath = $standardRelease.TrimEnd([char[]]@('\','/')) + [IO.Path]::DirectorySeparatorChar
if ([IO.Path]::GetFullPath($buildDirectory).StartsWith($preservedPath, [StringComparison]::OrdinalIgnoreCase)) { throw 'P9-18 build output cannot use or modify the preserved build/windows-x64-release tree.' }
$expectedGenerator = 'Visual Studio 17 2022'
$expectedPlatform = 'x64'
$expectedToolset = 'v143'
$expectedInstance = [IO.Path]::GetFullPath('C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools')
$expectedToolsetVersion = '14.44.35207'
$expectedCompilerVersion = '19.44.35230.0'
$expectedSdkVersion = '10.0.26100.0'
$includedRoots = @('CMakeLists.txt','CMakePresets.json','cmake','docs/qt-rewrite/phase-4-resource-inventory.json','licenses','resources','scripts/phase4','src','tests','third_party')
$source = Get-P914SourceInventory $RepositoryRoot $includedRoots
$sourcePath = Join-Path $RunRoot 'source-files.json'
Write-P914Json $sourcePath $source
$harnessPaths = @(
    'scripts/phase9/p9_14_packaged_lifecycle_common.ps1',
    'scripts/phase9/p9_18_build_current_source_release.ps1',
    'scripts/phase9/prepare_p9_18_bundang_fixture.py',
    'scripts/phase9/run_packaged_next_p9_18_campus_map.ps1',
    'scripts/phase9/validate_packaged_next_p9_18_campus_map.py',
    'scripts/phase9/test_prepare_p9_18_bundang_fixture.py',
    'scripts/phase9/test_validate_packaged_next_p9_18_campus_map.py'
)
$harnessRows = [Collections.Generic.List[object]]::new()
foreach ($relative in $harnessPaths) {
    $path = Join-Path $RepositoryRoot $relative
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Required P9-18 harness source is missing before the exact build: $relative" }
    $harnessRows.Add([ordered]@{ path = $relative; sizeBytes = [Int64](Get-Item -LiteralPath $path).Length; sha256 = Get-P914FileSha256 $path })
}
$harnessFiles = @($harnessRows.ToArray())
$harnessInventory = [ordered]@{ repositoryRoot = $RepositoryRoot; fileCount = $harnessFiles.Count; sha256 = Get-P914InventoryFingerprint $harnessFiles; files = $harnessFiles }
$harnessInventoryPath = Join-Path $RunRoot 'harness-files.json'
Write-P914Json $harnessInventoryPath $harnessInventory
$cmakeCachePath = Join-Path $standardRelease 'CMakeCache.txt'
if (-not (Test-Path -LiteralPath $cmakeCachePath -PathType Leaf)) { throw "Read-only Qt configuration source is missing from the preserved Release tree: $cmakeCachePath" }
function Get-P918CacheValue([string]$Path, [string]$Key) {
    $matches = @(Select-String -LiteralPath $Path -Pattern ('^' + [regex]::Escape($Key) + ':[^=]+='))
    if ($matches.Count -ne 1) { throw "Expected exactly one CMake cache entry '$Key' in the read-only toolchain reference." }
    return ($matches[0].Line -split '=', 2)[1]
}
function Invoke-P918Command([string]$Executable, [string[]]$Arguments, [string]$LogName, [string]$WorkingDirectory) {
    $logPath = Join-Path (Join-Path $RunRoot 'logs') $LogName
    $savedErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $lines = @(& $Executable @Arguments 2>&1 | ForEach-Object { [string]$_ })
        $code = [int]$LASTEXITCODE
    } finally { $ErrorActionPreference = $savedErrorActionPreference }
    [IO.File]::WriteAllLines($logPath, $lines, [Text.UTF8Encoding]::new($false))
    [ordered]@{ executable = [IO.Path]::GetFullPath($Executable); arguments = $Arguments; workingDirectory = $WorkingDirectory; logPath = [IO.Path]::GetFullPath($logPath); logSha256 = Get-P914FileSha256 $logPath; exitCode = $code }
}
function Get-P918GeneratedProjectToolchain([string]$Directory) {
    $projects = @(Get-ChildItem -LiteralPath $Directory -Filter '*.vcxproj' -File -Recurse)
    if ($projects.Count -eq 0) { throw "No generated VS projects found under $Directory" }
    $toolsets = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    $sdks = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($project in $projects) {
        $document = [xml]::new(); $document.Load($project.FullName)
        foreach ($node in $document.SelectNodes("//*[local-name()='PlatformToolset']")) { if (-not [string]::IsNullOrWhiteSpace([string]$node.InnerText)) { [void]$toolsets.Add(([string]$node.InnerText).Trim()) } }
        foreach ($node in $document.SelectNodes("//*[local-name()='WindowsTargetPlatformVersion']")) { if (-not [string]::IsNullOrWhiteSpace([string]$node.InnerText)) { [void]$sdks.Add(([string]$node.InnerText).Trim()) } }
    }
    if ($toolsets.Count -ne 1 -or -not $toolsets.Contains($expectedToolset)) { throw "Generated projects do not consistently use ${expectedToolset}: $(@($toolsets) -join ', ')" }
    if ($sdks.Count -ne 1 -or -not $sdks.Contains($expectedSdkVersion)) { throw "Generated projects do not consistently use Windows SDK ${expectedSdkVersion}: $(@($sdks) -join ', ')" }
    [ordered]@{ projectFileCount = [int]$projects.Count; platformToolset = $expectedToolset; windowsSdkVersion = $expectedSdkVersion }
}

$qt6Directory = [IO.Path]::GetFullPath((Get-P918CacheValue $cmakeCachePath 'Qt6_DIR'))
$qt6Config = Join-Path $qt6Directory 'Qt6Config.cmake'
if (-not (Test-Path -LiteralPath $qt6Config -PathType Leaf)) { throw "Qt6 CMake config referenced by the preserved Release cache is missing: $qt6Config" }
$vcRedistReferencePath = [IO.Path]::GetFullPath((Get-P918CacheValue $cmakeCachePath 'CLASSMNGR_INSTALLER_VC_REDIST_FILE'))
$vcRedistPath = [IO.Path]::GetFullPath((Join-Path $expectedInstance 'VC/Redist/MSVC/v143/vc_redist.x64.exe'))
$vcRedistPrefix = [IO.Path]::GetFullPath((Join-Path $expectedInstance 'VC/Redist/MSVC')).TrimEnd([char[]]@('\','/')) + [IO.Path]::DirectorySeparatorChar
if (-not $vcRedistPath.StartsWith($vcRedistPrefix, [StringComparison]::OrdinalIgnoreCase) -or [IO.Path]::GetFileName($vcRedistPath) -cne 'vc_redist.x64.exe' -or -not (Test-Path -LiteralPath $vcRedistPath -PathType Leaf)) { throw "The expected current VS2022 v143 x64 redistributable is missing: $vcRedistPath" }
if (-not $vcRedistReferencePath.StartsWith($vcRedistPrefix, [StringComparison]::OrdinalIgnoreCase) -or [IO.Path]::GetFileName($vcRedistReferencePath) -cne 'vc_redist.x64.exe' -or -not (Test-Path -LiteralPath $vcRedistReferencePath -PathType Leaf)) { throw "The preserved VS2022 Release cache redist reference is missing or outside the VS2022 redist tree: $vcRedistReferencePath" }
$vcRedistSignature = Get-AuthenticodeSignature -LiteralPath $vcRedistPath
if ([string]$vcRedistSignature.Status -cne 'Valid' -or [string]$vcRedistSignature.SignerCertificate.Subject -notmatch '^CN=Microsoft Corporation,') { throw "The VS2022 x64 redistributable is not signed by Microsoft: $vcRedistPath" }
$vcRedistVersion = [Diagnostics.FileVersionInfo]::GetVersionInfo($vcRedistPath)
$vcRedistSha256 = Get-P914FileSha256 $vcRedistPath
if ((Get-P914FileSha256 $vcRedistReferencePath) -cne $vcRedistSha256) { throw 'The installed VS2022 v143 redist and the preserved cache redist reference have different bytes.' }
$toolsetVersionPath = Join-Path $expectedInstance 'VC/Auxiliary/Build/Microsoft.VCToolsVersion.v143.default.txt'
if (-not (Test-Path -LiteralPath $toolsetVersionPath -PathType Leaf)) { throw "VS2022 v143 toolset version file is missing: $toolsetVersionPath" }
$toolsetVersion = (Get-Content -LiteralPath $toolsetVersionPath -Raw -Encoding ASCII).Trim()
if ($toolsetVersion -cne $expectedToolsetVersion) { throw "Expected toolset $expectedToolsetVersion but found $toolsetVersion." }
$compilerPath = [IO.Path]::GetFullPath((Join-Path $expectedInstance "VC/Tools/MSVC/$toolsetVersion/bin/Hostx64/x64/cl.exe"))
if (-not (Test-Path -LiteralPath $compilerPath -PathType Leaf)) { throw "Expected VS2022 x64 compiler is missing: $compilerPath" }
$compiler = [Diagnostics.FileVersionInfo]::GetVersionInfo($compilerPath)
if ([string]$compiler.FileVersion -cne $expectedCompilerVersion) { throw "Expected compiler $expectedCompilerVersion but found $($compiler.FileVersion)." }
$sdkRoot = [IO.Path]::GetFullPath((Join-Path 'C:\Program Files (x86)\Windows Kits\10\Include' $expectedSdkVersion))
if (-not (Test-Path -LiteralPath $sdkRoot -PathType Container)) { throw "Expected Windows SDK $expectedSdkVersion is missing: $sdkRoot" }
$cmakeCommand = Get-Command cmake.exe -CommandType Application -ErrorAction Stop | Select-Object -First 1
$cmake = [IO.Path]::GetFullPath($cmakeCommand.Source)
$configureArguments = @('-S',$RepositoryRoot,'-B',$buildDirectory,'-G',$expectedGenerator,'-A',$expectedPlatform,'-T',$expectedToolset,
    "-DCMAKE_GENERATOR_INSTANCE=$expectedInstance", "-DCMAKE_SYSTEM_VERSION=$expectedSdkVersion", "-DQt6_DIR=$qt6Directory", "-DCLASSMNGR_INSTALLER_VC_REDIST_FILE=$vcRedistPath",
    '-DBUILD_TESTING=ON','-DCLASSMNGR_NEXT_RELEASE_PACKAGE=ON','-DCLASSMNGR_PREFER_QT_ZLIB_FALLBACK=ON',
    '-DCLASSMNGR_RUN_WINDEPLOYQT=OFF','-DCLASSMNGR_RESOURCE_PACK_CHECK_ON_STARTUP=OFF',
    "-DCMAKE_INSTALL_PREFIX=$(Join-Path $RunRoot 'package')")
$buildStartedAtUtc = Get-P914Utc
$configure = Invoke-P918Command $cmake $configureArguments 'configure.log' $RepositoryRoot
$build = $null
$focusedTestBuild = $null
$focusedTests = $null
$generatedProjectToolchain = $null
$releaseExecutable = Join-Path $buildDirectory 'next-package-build/Release/ClassMngr.exe'
$releaseSha = $null
$assetsRoot = Join-Path $buildDirectory 'generated/ClassMngrNext/Resources/assets'
$translationsRoot = Join-Path $buildDirectory 'generated/ClassMngrNext/assets/translations'
$catalogPath = Join-Path $buildDirectory 'generated/ClassMngrNext/resource-catalog.json'
$resourceEvidence = $null
$status = 'failed'
$failure = $null
try {
    if ([int]$configure.exitCode -ne 0) { throw "Fresh exact-tree CMake configure failed with exit code $($configure.exitCode)." }
    $cachePath = Join-Path $buildDirectory 'CMakeCache.txt'
    foreach ($entry in @(@('CMAKE_GENERATOR',$expectedGenerator),@('CMAKE_GENERATOR_PLATFORM',$expectedPlatform),@('CMAKE_GENERATOR_TOOLSET',$expectedToolset),@('CMAKE_GENERATOR_INSTANCE',$expectedInstance),@('CMAKE_SYSTEM_VERSION',$expectedSdkVersion),@('CMAKE_HOME_DIRECTORY',$RepositoryRoot),@('Qt6_DIR',$qt6Directory),@('CLASSMNGR_NEXT_RELEASE_PACKAGE','ON'),@('BUILD_TESTING','ON'))) {
        $actual = Get-P918CacheValue $cachePath ([string]$entry[0])
        if ([string]$entry[0] -in @('CMAKE_GENERATOR_INSTANCE','CMAKE_HOME_DIRECTORY','Qt6_DIR')) { $actual = [IO.Path]::GetFullPath([string]$actual); $expectedValue = [IO.Path]::GetFullPath([string]$entry[1]) } else { $expectedValue = [string]$entry[1] }
        if ([string]$actual -ine $expectedValue) { throw "Fresh CMake cache field $($entry[0]) has unexpected value '$actual'." }
    }
    $generatedProjectToolchain = Get-P918GeneratedProjectToolchain $buildDirectory
    $buildArguments = @('--build',$buildDirectory,'--config','Release','--verbose','--target','ClassMngrNext','ClassMngrNextResourcePackage','--','/m:4','/p:TrackFileAccess=false')
    $build = Invoke-P918Command $cmake $buildArguments 'release-build.log' $RepositoryRoot
    if ([int]$build.exitCode -ne 0) { throw "Fresh exact-tree VS2022 x64 Release build failed with exit code $($build.exitCode)." }
    $focusedTargets = @('ClassMngrNextPageHostTests','ClassMngrNextClassesNavigatorPageTests','ClassMngrNextRosterEditorTests','ClassMngrNextApplicationRosterSaveUseCaseTests','ClassMngrNextPlatformApplicationServicesRosterSavePortTests')
    $testBuildArguments = @('--build',$buildDirectory,'--config','Release','--verbose','--target') + $focusedTargets + @('--','/m:4','/p:TrackFileAccess=false')
    $focusedTestBuild = Invoke-P918Command $cmake $testBuildArguments 'focused-test-build.log' $RepositoryRoot
    $ctest = Get-Command ctest.exe -CommandType Application -ErrorAction Stop | Select-Object -First 1
    $ctestArguments = @('--test-dir',$buildDirectory,'-C','Release','--output-on-failure','-R','^(ClassMngrNextPageHostTests|ClassMngrNextClassesNavigatorPageTests|ClassMngrNextRosterEditorTests|ClassMngrNextApplicationRosterSaveUseCaseTests|ClassMngrNextPlatformApplicationServicesRosterSavePortTests)$')
    if ([int]$focusedTestBuild.exitCode -eq 0) {
        $focusedTests = Invoke-P918Command ([IO.Path]::GetFullPath($ctest.Source)) $ctestArguments 'focused-ctest.log' $RepositoryRoot
        $focusedTests.testNames = $focusedTargets
    } else {
        $focusedTests = [ordered]@{ executable = [IO.Path]::GetFullPath($ctest.Source); arguments = $ctestArguments; workingDirectory = $RepositoryRoot; logPath = $null; logSha256 = $null; exitCode = [int]$focusedTestBuild.exitCode; testNames = $focusedTargets; blockedBy = 'focused C++ target build failed' }
    }
    if (-not (Test-Path -LiteralPath $releaseExecutable -PathType Leaf)) { throw "Release executable is missing: $releaseExecutable" }
    $releaseSha = Get-P914FileSha256 $releaseExecutable
    if ((Get-P914PeMachine $releaseExecutable) -ne 0x8664) { throw 'Built Release executable is not Windows x64.' }
    if ((Get-Content -LiteralPath $build.logPath -Raw -Encoding UTF8).IndexOf($compilerPath, [StringComparison]::OrdinalIgnoreCase) -lt 0) { throw 'Release build log does not identify the pinned VS2022 x64 compiler path.' }
    foreach ($path in @($assetsRoot,$translationsRoot,$catalogPath)) { if (-not (Test-Path -LiteralPath $path)) { throw "Required generated Next resource output is missing: $path" } }
    $assets = Get-P914CanonicalInventory $assetsRoot
    $translations = Get-P914CanonicalInventory $translationsRoot
    $resourceEvidence = [ordered]@{
        assetsRoot = [IO.Path]::GetFullPath($assetsRoot); assetsSha256 = [string]$assets.sha256; assetsFileCount = [int]$assets.fileCount
        translationsRoot = [IO.Path]::GetFullPath($translationsRoot); translationsSha256 = [string]$translations.sha256; translationsFileCount = [int]$translations.fileCount
        catalogPath = [IO.Path]::GetFullPath($catalogPath); catalogSha256 = Get-P914FileSha256 $catalogPath
    }
    $sourceAfter = Get-P914SourceInventory $RepositoryRoot $includedRoots
    if ([string]$source.sha256 -cne [string]$sourceAfter.sha256 -or [int]$source.fileCount -ne [int]$sourceAfter.fileCount) { throw 'Current product source changed during the exact-source Release build.' }
    if ((Get-P914FileSha256 $vcRedistPath) -cne $vcRedistSha256) { throw 'The signed VS2022 x64 redistributable changed during the exact-source Release build.' }
    $status = 'succeeded'
} catch {
    $failure = [ordered]@{ message = $_.Exception.Message; capturedAtUtc = Get-P914Utc; category = 'build-or-toolchain' }
}

$receipt = [ordered]@{
    schema = 'classmngr-phase9-p9-18-exact-release-build-v1'
    taskId = 'P9_18_PACKAGED_NEXT_CAMPUS_MAP_OPEN_CLOSE'
    status = $status
    startedAtUtc = $buildStartedAtUtc
    completedAtUtc = Get-P914Utc
    repositoryRoot = $RepositoryRoot
    sourceInventoryPath = [IO.Path]::GetFullPath($sourcePath)
    sourceInventoryFileSha256 = Get-P914FileSha256 $sourcePath
    sourceSha256 = [string]$source.sha256
    sourceFileCount = [int]$source.fileCount
    sourceIncludedRoots = $includedRoots
    harnessInventoryPath = [IO.Path]::GetFullPath($harnessInventoryPath)
    harnessInventoryFileSha256 = Get-P914FileSha256 $harnessInventoryPath
    harnessSourceSha256 = [string]$harnessInventory.sha256
    harnessSourceFileCount = [int]$harnessInventory.fileCount
    buildHelperPath = [IO.Path]::GetFullPath($PSCommandPath)
    buildHelperSha256 = Get-P914FileSha256 $PSCommandPath
    preservedStandardReleasePath = $standardRelease
    buildDirectory = [IO.Path]::GetFullPath($buildDirectory)
    configuration = 'Release'
    platform = 'x64'
    generator = $expectedGenerator
    generatorPlatform = $expectedPlatform
    generatorToolset = $expectedToolset
    generatorInstance = $expectedInstance
    windowsSdkVersion = $expectedSdkVersion
    compiler = [ordered]@{ path = $compilerPath; fileVersion = [string]$compiler.FileVersion; productVersion = [string]$compiler.ProductVersion; sha256 = Get-P914FileSha256 $compilerPath; toolsetVersion = $toolsetVersion }
    generatedProjectToolchain = $generatedProjectToolchain
    qt6Directory = $qt6Directory
    qt6ConfigSha256 = Get-P914FileSha256 $qt6Config
    vcRedist = [ordered]@{ path = $vcRedistPath; fileVersion = [string]$vcRedistVersion.FileVersion; productVersion = [string]$vcRedistVersion.ProductVersion; sha256 = $vcRedistSha256; signatureStatus = [string]$vcRedistSignature.Status; signer = [string]$vcRedistSignature.SignerCertificate.Subject; signerThumbprint = [string]$vcRedistSignature.SignerCertificate.Thumbprint; verifiedAtUtc = Get-P914Utc; verificationMethod = 'Installed VS2022 v143 path; Get-AuthenticodeSignature status Valid and Microsoft Corporation signer; file hash checked before and after build'; cacheReferencePath = $cmakeCachePath; cacheReferenceValuePath = $vcRedistReferencePath; cacheReferenceValueSha256 = Get-P914FileSha256 $vcRedistReferencePath }
    cmakeExecutable = $cmake
    configure = $configure
    build = $build
    focusedTestBuild = $focusedTestBuild
    focusedTests = $focusedTests
    releaseExecutablePath = [IO.Path]::GetFullPath($releaseExecutable)
    releaseExecutableSha256 = $releaseSha
    releaseExecutableMachine = if ($releaseSha) { [int](Get-P914PeMachine $releaseExecutable) } else { $null }
    generatedResources = $resourceEvidence
    failure = $failure
}
$receiptPath = Join-Path $RunRoot 'release-build-receipt.json'
Write-P914Json $receiptPath $receipt
if ($status -cne 'succeeded') { throw "P9-18 exact-source Release build failed: $($failure.message). See $receiptPath" }
Write-Host (ConvertTo-Json -Compress ([ordered]@{ status = $status; runRoot = $RunRoot; receipt = $receiptPath; sourceSha256 = [string]$source.sha256; releaseExecutableSha256 = $releaseSha; generator = $expectedGenerator; toolset = $expectedToolset; compilerVersion = [string]$compiler.FileVersion; windowsSdkVersion = $expectedSdkVersion }))
