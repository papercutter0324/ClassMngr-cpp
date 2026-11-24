param(
    [Parameter(Mandatory=$true)][string]$RepositoryRoot,
    [Parameter(Mandatory=$true)][string]$RunRoot,
    [string]$BuildDirectory = ''
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$RepositoryRoot = [IO.Path]::GetFullPath($RepositoryRoot)
$RunRoot = [IO.Path]::GetFullPath($RunRoot)
if (-not ('P917BuildExecutionContextNative' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Security.Principal;

public sealed class P917BuildExecutionContextFacts
{
    public string Identity { get; set; }
    public string UserSid { get; set; }
    public bool IsElevated { get; set; }
    public bool IsAdministratorRole { get; set; }
    public int IntegrityLevelRid { get; set; }
    public string IntegrityLevel { get; set; }
    public bool UserInteractive { get; set; }
    public int ProcessSessionId { get; set; }
    public uint ActiveConsoleSessionId { get; set; }
}

public static class P917BuildExecutionContextNative
{
    private const uint TokenQuery = 0x0008;
    private const int TokenElevationInformation = 20;
    private const int TokenIntegrityLevelInformation = 25;

    [StructLayout(LayoutKind.Sequential)]
    private struct TokenMandatoryLabel
    {
        public IntPtr Sid;
        public uint Attributes;
    }

    [DllImport("kernel32.dll")]
    private static extern IntPtr GetCurrentProcess();

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool CloseHandle(IntPtr handle);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern uint WTSGetActiveConsoleSessionId();

    [DllImport("advapi32.dll", SetLastError = true)]
    private static extern bool OpenProcessToken(IntPtr process, uint desiredAccess, out IntPtr token);

    [DllImport("advapi32.dll", SetLastError = true)]
    private static extern bool GetTokenInformation(IntPtr token, int informationClass, IntPtr information, int informationLength, out int returnLength);

    [DllImport("advapi32.dll", SetLastError = true)]
    private static extern IntPtr GetSidSubAuthorityCount(IntPtr sid);

    [DllImport("advapi32.dll", SetLastError = true)]
    private static extern IntPtr GetSidSubAuthority(IntPtr sid, uint subAuthorityIndex);

    private static IntPtr ReadTokenInformation(IntPtr token, int informationClass, out int length)
    {
        int required;
        GetTokenInformation(token, informationClass, IntPtr.Zero, 0, out required);
        if (required <= 0)
            throw new Win32Exception(Marshal.GetLastWin32Error(), "Windows did not return the required token-information size.");
        IntPtr buffer = Marshal.AllocHGlobal(required);
        if (!GetTokenInformation(token, informationClass, buffer, required, out length))
        {
            int error = Marshal.GetLastWin32Error();
            Marshal.FreeHGlobal(buffer);
            throw new Win32Exception(error, "Windows could not read the requested token information.");
        }
        return buffer;
    }

    private static int ReadElevation(IntPtr token)
    {
        int length;
        IntPtr buffer = ReadTokenInformation(token, TokenElevationInformation, out length);
        try { return Marshal.ReadInt32(buffer); }
        finally { Marshal.FreeHGlobal(buffer); }
    }

    private static int ReadIntegrityRid(IntPtr token)
    {
        int length;
        IntPtr buffer = ReadTokenInformation(token, TokenIntegrityLevelInformation, out length);
        try
        {
            TokenMandatoryLabel label = (TokenMandatoryLabel)Marshal.PtrToStructure(buffer, typeof(TokenMandatoryLabel));
            if (label.Sid == IntPtr.Zero)
                throw new InvalidOperationException("The token integrity SID is null.");
            IntPtr countAddress = GetSidSubAuthorityCount(label.Sid);
            if (countAddress == IntPtr.Zero)
                throw new Win32Exception(Marshal.GetLastWin32Error(), "Windows could not read the token integrity SID subauthority count.");
            byte count = Marshal.ReadByte(countAddress);
            if (count == 0)
                throw new InvalidOperationException("The token integrity SID has no subauthorities.");
            IntPtr ridAddress = GetSidSubAuthority(label.Sid, (uint)(count - 1));
            if (ridAddress == IntPtr.Zero)
                throw new Win32Exception(Marshal.GetLastWin32Error(), "Windows could not read the token integrity SID level.");
            return Marshal.ReadInt32(ridAddress);
        }
        finally { Marshal.FreeHGlobal(buffer); }
    }

    private static string GetIntegrityLevelName(int integrityRid)
    {
        switch (integrityRid & 0xF000)
        {
            case 0x5000: return "ProtectedProcess";
            case 0x4000: return "System";
            case 0x3000: return "High";
            case 0x2000: return "Medium";
            case 0x1000: return "Low";
            default: return "Untrusted";
        }
    }

    public static P917BuildExecutionContextFacts Read()
    {
        IntPtr token;
        if (!OpenProcessToken(GetCurrentProcess(), TokenQuery, out token))
            throw new Win32Exception(Marshal.GetLastWin32Error(), "Windows could not open the current process token for query.");
        try
        {
            using (WindowsIdentity identity = WindowsIdentity.GetCurrent())
            using (Process process = Process.GetCurrentProcess())
            {
                if (identity.User == null)
                    throw new InvalidOperationException("The current Windows token has no user SID.");
                int integrityRid = ReadIntegrityRid(token);
                WindowsPrincipal principal = new WindowsPrincipal(identity);
                return new P917BuildExecutionContextFacts {
                    Identity = identity.Name,
                    UserSid = identity.User.Value,
                    IsElevated = ReadElevation(token) != 0,
                    IsAdministratorRole = principal.IsInRole(WindowsBuiltInRole.Administrator),
                    IntegrityLevelRid = integrityRid,
                    IntegrityLevel = GetIntegrityLevelName(integrityRid),
                    UserInteractive = Environment.UserInteractive,
                    ProcessSessionId = process.SessionId,
                    ActiveConsoleSessionId = WTSGetActiveConsoleSessionId()
                };
            }
        }
        finally
        {
            if (!CloseHandle(token))
                throw new Win32Exception(Marshal.GetLastWin32Error(), "Windows could not close the queried process-token handle.");
        }
    }
}
'@
}
. (Join-Path $RepositoryRoot 'scripts/phase9/p9_14_packaged_lifecycle_common.ps1')
function Get-P917ExecutionContext {
    $facts = [P917BuildExecutionContextNative]::Read()
    $context = [ordered]@{
        identity = [string]$facts.Identity
        userSid = [string]$facts.UserSid
        isElevated = [bool]$facts.IsElevated
        isAdministratorRole = [bool]$facts.IsAdministratorRole
        integrityLevelRid = [Int64]$facts.IntegrityLevelRid
        integrityLevel = [string]$facts.IntegrityLevel
        userInteractive = [bool]$facts.UserInteractive
        processSessionId = [Int64]$facts.ProcessSessionId
        activeConsoleSessionId = [Int64]$facts.ActiveConsoleSessionId
        observedAtUtc = [DateTimeOffset]::UtcNow.ToString('o')
    }
    if ([string]::IsNullOrWhiteSpace([string]$context.identity) -or [string]::IsNullOrWhiteSpace([string]$context.userSid)) { throw 'Current Windows execution context lacks an identity or user SID.' }
    if ([bool]$context.isElevated -or [bool]$context.isAdministratorRole) { throw 'Refusing to build with an elevated or Administrator-role Windows token.' }
    if ([Int64]$context.integrityLevelRid -ne 8192 -or [string]$context.integrityLevel -cne 'Medium') { throw "Refusing to build outside actual Medium integrity (observed $($context.integrityLevel)/$($context.integrityLevelRid))." }
    if (-not [bool]$context.userInteractive) { throw 'Refusing to build outside a UserInteractive Windows process.' }
    if ([Int64]$context.processSessionId -lt 1 -or [Int64]$context.activeConsoleSessionId -ne [Int64]$context.processSessionId) { throw 'Refusing to build unless the current process session matches the active local console session.' }
    return $context
}

function Assert-P917SameExecutionContext([object]$Before, [object]$After) {
    foreach ($property in @('identity', 'userSid', 'isElevated', 'isAdministratorRole', 'integrityLevelRid', 'integrityLevel', 'userInteractive', 'processSessionId', 'activeConsoleSessionId')) {
        if ([string]$Before[$property] -cne [string]$After[$property]) { throw "Windows execution context changed during the build at '$property'." }
    }
}

$executionContextStart = Get-P917ExecutionContext
$exactBuildEvidenceRoot = [IO.Path]::GetFullPath((Join-Path $RepositoryRoot 'artifacts/phase9/p9-17-exact-release-builds')).TrimEnd([char[]]@('\', '/')) + [IO.Path]::DirectorySeparatorChar
if (-not $RunRoot.StartsWith($exactBuildEvidenceRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'P9-17 build evidence RunRoot must be a fresh child of artifacts/phase9/p9-17-exact-release-builds.' }
if (Test-Path -LiteralPath $RunRoot) { throw "Refusing to overwrite P9-17 build evidence RunRoot: $RunRoot" }
[void][IO.Directory]::CreateDirectory((Join-Path $RunRoot 'logs'))
$standardBuildDirectory = [IO.Path]::GetFullPath((Join-Path $RepositoryRoot 'build/windows-x64-release'))
$saveStageDiagnosticBuildDirectory = [IO.Path]::GetFullPath((Join-Path $RepositoryRoot 'build/p9-17-save-stage-diagnostic-release'))
$expectedGenerator = 'Visual Studio 17 2022'
$expectedGeneratorPlatform = 'x64'
$expectedGeneratorToolset = 'v143'
$expectedGeneratorInstance = [IO.Path]::GetFullPath('C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools')
$expectedToolsetVersion = '14.44.35207'
$expectedCompilerFileVersion = '19.44.35230.0'
$expectedWindowsSdkVersion = '10.0.26100.0'
$buildDirectory = if ([string]::IsNullOrWhiteSpace($BuildDirectory)) {
    $standardBuildDirectory
} elseif ([IO.Path]::IsPathRooted($BuildDirectory)) {
    [IO.Path]::GetFullPath($BuildDirectory)
} else {
    [IO.Path]::GetFullPath((Join-Path $RepositoryRoot $BuildDirectory))
}
if ($buildDirectory -ine $standardBuildDirectory -and
    $buildDirectory -ine $saveStageDiagnosticBuildDirectory) {
    throw 'P9-17 build directory must be the standard Release path or the pinned save-stage diagnostic path.'
}
function Get-CacheValue([string]$Path, [string]$Key) {
    $matches = @(Select-String -LiteralPath $Path -Pattern ('^' + [regex]::Escape($Key) + ':[^=]+='))
    if ($matches.Count -ne 1) { throw "Expected exactly one CMake cache entry '$Key'." }
    return ($matches[0].Line -split '=', 2)[1]
}
function Get-P917CompilerIdentity {
    $versionPath = Join-Path $expectedGeneratorInstance 'VC/Auxiliary/Build/Microsoft.VCToolsVersion.v143.default.txt'
    if (-not (Test-Path -LiteralPath $versionPath -PathType Leaf)) { throw "VS2022 default v143 toolset version file is missing: $versionPath" }
    $toolsetVersion = (Get-Content -LiteralPath $versionPath -Raw -Encoding ASCII).Trim()
    if ($toolsetVersion -cne $expectedToolsetVersion) { throw "Expected VS2022 v143 compiler toolset $expectedToolsetVersion; found $toolsetVersion." }
    $compilerPath = [IO.Path]::GetFullPath((Join-Path $expectedGeneratorInstance "VC/Tools/MSVC/$toolsetVersion/bin/Hostx64/x64/cl.exe"))
    if (-not (Test-Path -LiteralPath $compilerPath -PathType Leaf)) { throw "Expected VS2022 v143 x64 compiler is missing: $compilerPath" }
    $versionInfo = [Diagnostics.FileVersionInfo]::GetVersionInfo($compilerPath)
    if ([string]$versionInfo.FileVersion -cne $expectedCompilerFileVersion) { throw "Expected x64 compiler $expectedCompilerFileVersion; found $($versionInfo.FileVersion)." }
    $sdkRoot = [IO.Path]::GetFullPath((Join-Path 'C:\Program Files (x86)\Windows Kits\10\Include' $expectedWindowsSdkVersion))
    if (-not (Test-Path -LiteralPath $sdkRoot -PathType Container)) { throw "Required Windows SDK $expectedWindowsSdkVersion is missing: $sdkRoot" }
    return [ordered]@{
        path = $compilerPath
        fileVersion = [string]$versionInfo.FileVersion
        productVersion = [string]$versionInfo.ProductVersion
        sha256 = Get-P914FileSha256 $compilerPath
        toolsetVersion = $toolsetVersion
        windowsSdkVersion = $expectedWindowsSdkVersion
        windowsSdkIncludeRoot = $sdkRoot
    }
}
function Get-P917GeneratedProjectToolchain([string]$Directory) {
    $projects = @(Get-ChildItem -LiteralPath $Directory -Filter '*.vcxproj' -File -Recurse)
    if ($projects.Count -eq 0) { throw "Configured Visual Studio projects are missing from the Release build directory: $Directory" }
    $toolsets = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    $sdkVersions = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($project in $projects) {
        $document = [xml]::new()
        $document.Load($project.FullName)
        foreach ($node in $document.SelectNodes("//*[local-name()='PlatformToolset']")) {
            $value = ([string]$node.InnerText).Trim()
            if (-not [string]::IsNullOrWhiteSpace($value)) { [void]$toolsets.Add($value) }
        }
        foreach ($node in $document.SelectNodes("//*[local-name()='WindowsTargetPlatformVersion']")) {
            $value = ([string]$node.InnerText).Trim()
            if (-not [string]::IsNullOrWhiteSpace($value)) { [void]$sdkVersions.Add($value) }
        }
    }
    if ($toolsets.Count -ne 1 -or -not $toolsets.Contains($expectedGeneratorToolset)) {
        throw "Generated Visual Studio projects must use only $expectedGeneratorToolset; found $(@($toolsets) -join ', ')."
    }
    if ($sdkVersions.Count -ne 1 -or -not $sdkVersions.Contains($expectedWindowsSdkVersion)) {
        throw "Generated Visual Studio projects must use only Windows SDK $expectedWindowsSdkVersion; found $(@($sdkVersions) -join ', ')."
    }
    return [ordered]@{
        projectFileCount = [int]$projects.Count
        platformToolset = $expectedGeneratorToolset
        windowsSdkVersion = $expectedWindowsSdkVersion
    }
}
$cachePath = Join-Path $buildDirectory 'CMakeCache.txt'
$acceptedSourcePath = Join-Path $RepositoryRoot 'artifacts/phase9/packaged-next-p9-16-repeated-workspace/20261121T023320451Z-9b576dcbfdc84dd0996f279db5ef67e9/source-files.json'
$acceptedSource = Read-P914Json $acceptedSourcePath
$includedRoots = @($acceptedSource.includedRoots)
$sourceInventory = Get-P914SourceInventory $RepositoryRoot $includedRoots
$sourceInventoryPath = Join-Path $RunRoot 'source-files.json'
Write-P914Json $sourceInventoryPath $sourceInventory
$generator = Get-CacheValue $cachePath 'CMAKE_GENERATOR'
$generatorPlatform = Get-CacheValue $cachePath 'CMAKE_GENERATOR_PLATFORM'
$generatorToolset = Get-CacheValue $cachePath 'CMAKE_GENERATOR_TOOLSET'
$generatorInstance = [IO.Path]::GetFullPath((Get-CacheValue $cachePath 'CMAKE_GENERATOR_INSTANCE'))
$cacheHome = [IO.Path]::GetFullPath((Get-CacheValue $cachePath 'CMAKE_HOME_DIRECTORY'))
$packageOption = Get-CacheValue $cachePath 'CLASSMNGR_NEXT_RELEASE_PACKAGE'
if ($cacheHome -ine $RepositoryRoot -or $generator -cne $expectedGenerator -or $generatorPlatform -cne $expectedGeneratorPlatform -or $generatorToolset -cne $expectedGeneratorToolset -or $generatorInstance -ine $expectedGeneratorInstance -or $packageOption -cne 'ON') {
    throw 'Existing CMake cache does not match the VS2022/v143 Windows x64 Next Release package configuration.'
}
$compilerIdentity = Get-P917CompilerIdentity
$generatedProjectToolchain = Get-P917GeneratedProjectToolchain $buildDirectory
$assetsRoot = Join-Path $buildDirectory 'generated/ClassMngrNext/Resources/assets'
$preflightPath = Join-Path $RunRoot 'preflight.json'
$preflight = [ordered]@{
    schema = 'classmngr-phase9-p9-17-standard-token-build-preflight-v2'
    capturedAtUtc = Get-P914Utc
    executionContext = $executionContextStart
    resourceRoot = [IO.Path]::GetFullPath($assetsRoot)
    expectedBakedAssetFileCount = 192
    resourceOutputsGeneratedByBuild = $true
    sourceInventoryPath = [IO.Path]::GetFullPath($sourceInventoryPath)
    sourceInventoryFileSha256 = Get-P914FileSha256 $sourceInventoryPath
    sourceSha256 = [string]$sourceInventory.sha256
    sourceFileCount = [int]$sourceInventory.fileCount
    cachePath = [IO.Path]::GetFullPath($cachePath)
    cacheSha256 = Get-P914FileSha256 $cachePath
    generator = $generator
    generatorPlatform = $generatorPlatform
    generatorToolset = $generatorToolset
    generatorInstance = $generatorInstance
    windowsSdkVersion = $expectedWindowsSdkVersion
    generatedProjectToolchain = $generatedProjectToolchain
    compiler = $compilerIdentity
    packageOption = 'CLASSMNGR_NEXT_RELEASE_PACKAGE=ON'
    status = 'passed'
}
Write-P914Json $preflightPath $preflight
$cmake = Get-Command cmake.exe -CommandType Application -ErrorAction Stop | Select-Object -First 1
$targets = @('ClassMngrNext', 'ClassMngrNextResourcePackage', 'ClassMngrPhase7NextBlankFixtureTests')
$arguments = @('--build', $buildDirectory, '--config', 'Release', '--verbose', '--target') + $targets + @('--', '/m:4', '/p:TrackFileAccess=false')
$buildLogPath = Join-Path (Join-Path $RunRoot 'logs') 'current-source-release-build.log'
$buildStartedAtUtc = Get-P914Utc
Write-P914Json (Join-Path $RunRoot 'build-command.json') ([ordered]@{
    executable = [IO.Path]::GetFullPath($cmake.Source)
    arguments = $arguments
    workingDirectory = $RepositoryRoot
    startedAtUtc = $buildStartedAtUtc
})
& $cmake.Source @arguments 2>&1 | Out-File -LiteralPath $buildLogPath -Encoding utf8
$buildExitCode = [int]$LASTEXITCODE
$buildCompletedAtUtc = Get-P914Utc
$executionContextCompletion = Get-P917ExecutionContext
Assert-P917SameExecutionContext $executionContextStart $executionContextCompletion
$sourceAfter = Get-P914SourceInventory $RepositoryRoot $includedRoots
if ([int]$sourceAfter.fileCount -ne [int]$sourceInventory.fileCount -or [string]$sourceAfter.sha256 -cne [string]$sourceInventory.sha256) {
    throw 'Source tree changed during the incremental Release build.'
}
$buildLogText = Get-Content -LiteralPath $buildLogPath -Raw -Encoding UTF8
$compilerObservedInBuildLog = $buildLogText.IndexOf([string]$compilerIdentity.path, [StringComparison]::OrdinalIgnoreCase) -ge 0
if ($buildExitCode -eq 0 -and -not $compilerObservedInBuildLog) { throw 'Successful Release build log does not identify the expected VS2022/v143 x64 compiler path.' }
$cacheShaAfter = Get-P914FileSha256 $cachePath
$releaseExecutable = Join-Path $buildDirectory 'next-package-build/Release/ClassMngr.exe'
$fixtureExecutable = Join-Path $buildDirectory 'Release/ClassMngrPhase7NextBlankFixtureTests.exe'
$translationsRoot = Join-Path $buildDirectory 'generated/ClassMngrNext/assets/translations'
$catalogPath = Join-Path $buildDirectory 'generated/ClassMngrNext/resource-catalog.json'
$generatedResources = $null
$releaseExecutableSha256 = $null
$targetArtifacts = @()
if ($buildExitCode -eq 0) {
    if ($cacheShaAfter -cne [string]$preflight.cacheSha256) { throw 'CMake cache changed during the incremental build.' }
    foreach ($path in @($releaseExecutable, $fixtureExecutable, $translationsRoot, $catalogPath)) {
        if (-not (Test-Path -LiteralPath $path)) { throw "Expected Release output is missing: $path" }
    }
    $releaseExecutableSha256 = Get-P914FileSha256 $releaseExecutable
    if ((Get-P914PeMachine $releaseExecutable) -ne 0x8664) { throw 'Release executable is not Windows x64.' }
    $assetInventory = Get-P914CanonicalInventory $assetsRoot
    $translationInventory = Get-P914CanonicalInventory $translationsRoot
    if ([int]$assetInventory.fileCount -ne 192) { throw "Expected 192 generated asset files after the build; found $($assetInventory.fileCount)." }
    $generatedResources = [ordered]@{
        bakedAssetsRoot = [IO.Path]::GetFullPath($assetsRoot)
        bakedAssetsSha256 = [string]$assetInventory.sha256
        bakedAssetsFileCount = [int]$assetInventory.fileCount
        translationsRoot = [IO.Path]::GetFullPath($translationsRoot)
        translationsSha256 = [string]$translationInventory.sha256
        translationsFileCount = [int]$translationInventory.fileCount
        catalogPath = [IO.Path]::GetFullPath($catalogPath)
        catalogSha256 = Get-P914FileSha256 $catalogPath
    }
    $targetArtifacts = @(
        [ordered]@{
            target = 'ClassMngrNext'
            path = [IO.Path]::GetFullPath($releaseExecutable)
            sha256 = $releaseExecutableSha256
            sizeBytes = [Int64](Get-Item -LiteralPath $releaseExecutable).Length
        }
        [ordered]@{
            target = 'ClassMngrPhase7NextBlankFixtureTests'
            path = [IO.Path]::GetFullPath($fixtureExecutable)
            sha256 = Get-P914FileSha256 $fixtureExecutable
            sizeBytes = [Int64](Get-Item -LiteralPath $fixtureExecutable).Length
        }
    )
}
$receiptPath = Join-Path $RunRoot 'release-build-receipt.json'
$receipt = [ordered]@{
    schema = 'classmngr-phase9-p9-17-exact-release-build-v3'
    status = if ($buildExitCode -eq 0) { 'succeeded' } else { 'failed' }
    runId = Split-Path -Leaf $RunRoot
    startedAtUtc = $buildStartedAtUtc
    completedAtUtc = $buildCompletedAtUtc
    repositoryRoot = $RepositoryRoot
    buildDirectory = [IO.Path]::GetFullPath($buildDirectory)
    configuration = 'Release'
    platform = 'x64'
    generator = $generator
    generatorPlatform = $generatorPlatform
    generatorToolset = $generatorToolset
    generatorInstance = $generatorInstance
    windowsSdkVersion = $expectedWindowsSdkVersion
    generatedProjectToolchain = $generatedProjectToolchain
    compiler = $compilerIdentity
    packageOption = 'CLASSMNGR_NEXT_RELEASE_PACKAGE=ON'
    incrementalBuild = $true
    executionContext = [ordered]@{
        start = $executionContextStart
        completion = $executionContextCompletion
        sameIdentityAndConsoleSession = $true
    }
    cachePath = [IO.Path]::GetFullPath($cachePath)
    cacheSha256 = $cacheShaAfter
    sourceInventoryPath = [IO.Path]::GetFullPath($sourceInventoryPath)
    sourceInventoryFileSha256 = Get-P914FileSha256 $sourceInventoryPath
    sourceSha256 = [string]$sourceInventory.sha256
    sourceFileCount = [int]$sourceInventory.fileCount
    targets = $targets
    build = [ordered]@{
        command = '"' + [IO.Path]::GetFullPath($cmake.Source) + '" ' + ($arguments -join ' ')
        startedAtUtc = $buildStartedAtUtc
        completedAtUtc = $buildCompletedAtUtc
        exitCode = $buildExitCode
        logPath = [IO.Path]::GetFullPath($buildLogPath)
        logSha256 = Get-P914FileSha256 $buildLogPath
        actualCompilerPath = [string]$compilerIdentity.path
        compilerObservedInLog = [bool]$compilerObservedInBuildLog
    }
    releaseExecutablePath = [IO.Path]::GetFullPath($releaseExecutable)
    releaseExecutableSha256 = $releaseExecutableSha256
    generatedResources = $generatedResources
    targetArtifacts = $targetArtifacts
    preflightPath = [IO.Path]::GetFullPath($preflightPath)
    preflightSha256 = Get-P914FileSha256 $preflightPath
    buildHelperPath = [IO.Path]::GetFullPath($PSCommandPath)
    buildHelperSha256 = Get-P914FileSha256 $PSCommandPath
}
Write-P914Json $receiptPath $receipt
Write-P914Json (Join-Path $RunRoot 'build-outcome.json') ([ordered]@{
    runId = Split-Path -Leaf $RunRoot
    status = [string]$receipt.status
    exitCode = $buildExitCode
    receiptPath = [IO.Path]::GetFullPath($receiptPath)
    receiptSha256 = Get-P914FileSha256 $receiptPath
    buildLogPath = [IO.Path]::GetFullPath($buildLogPath)
    buildLogSha256 = Get-P914FileSha256 $buildLogPath
})
"identity=$($executionContextCompletion.identity)"
"userSid=$($executionContextCompletion.userSid)"
"isElevated=$($executionContextCompletion.isElevated)"
"isAdministratorRole=$($executionContextCompletion.isAdministratorRole)"
"integrityLevel=$($executionContextCompletion.integrityLevel)"
"processSessionId=$($executionContextCompletion.processSessionId)"
"activeConsoleSessionId=$($executionContextCompletion.activeConsoleSessionId)"
"sourceSha256=$($sourceInventory.sha256)"
"sourceFileCount=$($sourceInventory.fileCount)"
"targetExitCode=$buildExitCode"
"receiptPath=$receiptPath"
"buildLogPath=$buildLogPath"
if ($buildExitCode -ne 0) { exit $buildExitCode }
