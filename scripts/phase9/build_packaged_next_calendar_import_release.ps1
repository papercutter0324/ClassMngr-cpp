<#
.SYNOPSIS
Configures and builds the current-source packaged Next Release inputs.

.DESCRIPTION
Runs only the bounded Release CMake configure/build for P9-10. It captures
native stdout/stderr separately and writes a source-bound receipt that the
non-elevated package/app runner can verify before using the built executable.
#>
[CmdletBinding()]
param(
    [string]$RepositoryRoot = "",
    [string]$BuildDirectory = "build\windows-x64-release",
    [string]$EvidenceRoot = "artifacts\phase9\calendar-import-release-builds"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$TaskId = "P9_10_PACKAGED_CALENDAR_IMPORT_ACCEPTANCE"
$Schema = "classmngr-phase9-calendar-import-release-build-receipt-v1"
$SourceRoots = @(
    "CMakeLists.txt", "CMakePresets.json", "cmake", "resources", "scripts\phase9",
    "src", "tests",
    "docs\qt-rewrite\visual-baseline\release\large-calendar-import-boundary",
    "docs\qt-rewrite\visual-baseline\release\large-calendar-import-error-boundary"
)
if ([string]::IsNullOrWhiteSpace($RepositoryRoot)) { $RepositoryRoot = Join-Path $PSScriptRoot "..\.." }
$RepositoryRoot = [IO.Path]::GetFullPath($RepositoryRoot)
$BuildDirectory = if ([IO.Path]::IsPathRooted($BuildDirectory)) { [IO.Path]::GetFullPath($BuildDirectory) } else { [IO.Path]::GetFullPath((Join-Path $RepositoryRoot $BuildDirectory)) }
$EvidenceRoot = if ([IO.Path]::IsPathRooted($EvidenceRoot)) { [IO.Path]::GetFullPath($EvidenceRoot) } else { [IO.Path]::GetFullPath((Join-Path $RepositoryRoot $EvidenceRoot)) }
$RunId = [DateTimeOffset]::UtcNow.ToString("yyyyMMdd'T'HHmmssfff'Z'") + "-" + [Guid]::NewGuid().ToString("N")
$EvidenceDirectory = Join-Path $EvidenceRoot $RunId
if (Test-Path -LiteralPath $EvidenceDirectory) { throw "Refusing to overwrite build evidence directory $EvidenceDirectory." }
[void][IO.Directory]::CreateDirectory($EvidenceDirectory)

function Get-FileSha256 {
    param([string]$Path)
    (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}
function Get-StringSha256 {
    param([string]$Value)
    $sha = [Security.Cryptography.SHA256]::Create()
    try { ([BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($Value)))).Replace("-", "").ToLowerInvariant() }
    finally { $sha.Dispose() }
}
function Write-JsonFile {
    param([string]$Path, [object]$Value)
    [IO.File]::WriteAllText($Path, ($Value | ConvertTo-Json -Depth 40), [Text.UTF8Encoding]::new($false))
}
function Get-SourceInventory {
    param([string]$Root)
    $rootFull = [IO.Path]::GetFullPath($Root).TrimEnd([char[]]@('\', '/'))
    $prefix = $rootFull + [IO.Path]::DirectorySeparatorChar
    $rows = [Collections.Generic.List[object]]::new()
    foreach ($relativeRoot in $SourceRoots) {
        $path = Join-Path $rootFull $relativeRoot
        if (-not (Test-Path -LiteralPath $path)) { throw "Source inventory root is missing: $relativeRoot" }
        $files = if (Test-Path -LiteralPath $path -PathType Leaf) { @(Get-Item -LiteralPath $path) } else { @(Get-ChildItem -LiteralPath $path -File -Recurse) }
        foreach ($file in $files) {
            if ($file.FullName -match '[\\/](?:__pycache__|\.pytest_cache|\.mypy_cache)[\\/]' -or $file.Extension -in @('.pyc', '.pyo')) { continue }
            $canonical = [IO.Path]::GetFullPath($file.FullName)
            if (-not $canonical.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) { throw "Source inventory path escaped repository: $canonical" }
            $relative = $canonical.Substring($prefix.Length).Replace('\', '/')
            $rows.Add([ordered]@{ path = $relative; sizeBytes = [Int64]$file.Length; sha256 = Get-FileSha256 $canonical })
        }
    }
    $rows.Sort([System.Comparison[object]]{ param($a, $b) [StringComparer]::Ordinal.Compare(([string]$a.path).ToLowerInvariant(), ([string]$b.path).ToLowerInvariant()) })
    $sorted = @($rows.ToArray())
    $canonicalRows = @($sorted | ForEach-Object { "{0}`t{1}`t{2}" -f $_.path, $_.sizeBytes, $_.sha256 }) -join "`n"
    [ordered]@{ root = $rootFull; includedRoots = $SourceRoots; fileCount = $sorted.Count; sha256 = Get-StringSha256 ($canonicalRows + "`n"); files = $sorted }
}
function Get-CanonicalInventory {
    param([string]$Root)
    $rootFull = [IO.Path]::GetFullPath($Root).TrimEnd([char[]]@('\', '/'))
    $prefix = $rootFull + [IO.Path]::DirectorySeparatorChar
    $rows = [Collections.Generic.List[object]]::new()
    foreach ($file in @(Get-ChildItem -LiteralPath $rootFull -File -Recurse)) {
        $canonical = [IO.Path]::GetFullPath($file.FullName)
        if (-not $canonical.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) { throw "Generated resource inventory path escaped root: $canonical" }
        $relative = $canonical.Substring($prefix.Length).Replace('\', '/')
        $rows.Add([ordered]@{ path = $relative; sizeBytes = [Int64]$file.Length; sha256 = Get-FileSha256 $canonical })
    }
    $rows.Sort([System.Comparison[object]]{ param($a, $b) [StringComparer]::Ordinal.Compare(([string]$a.path).ToLowerInvariant(), ([string]$b.path).ToLowerInvariant()) })
    $sorted = @($rows.ToArray())
    $canonicalRows = @($sorted | ForEach-Object { "{0}`t{1}`t{2}" -f $_.path, $_.sizeBytes, $_.sha256 }) -join "`n"
    [ordered]@{ root = $rootFull; fileCount = $sorted.Count; sha256 = Get-StringSha256 ($canonicalRows + "`n"); files = $sorted }
}
function Get-CacheValue {
    param([string[]]$Lines, [string]$Name)
    $matches = @($Lines | Where-Object { $_ -match "^$([regex]::Escape($Name)):[^=]+=" })
    if ($matches.Count -ne 1) { throw "Expected one CMake cache entry '$Name', found $($matches.Count)." }
    ($matches[0] -split "=", 2)[1]
}
function ConvertTo-WindowsProcessArgument {
    param([AllowEmptyString()][string]$Value)
    $builder = [Text.StringBuilder]::new()
    [void]$builder.Append([char]'"')
    $backslashCount = 0
    foreach ($character in $Value.ToCharArray()) {
        if ($character -eq [char]'\') { ++$backslashCount; continue }
        if ($character -eq [char]'"') {
            for ($index = 0; $index -lt (2 * $backslashCount + 1); $index++) { [void]$builder.Append([char]'\') }
            [void]$builder.Append([char]'"')
        } else {
            for ($index = 0; $index -lt $backslashCount; $index++) { [void]$builder.Append([char]'\') }
            [void]$builder.Append($character)
        }
        $backslashCount = 0
    }
    for ($index = 0; $index -lt (2 * $backslashCount); $index++) { [void]$builder.Append([char]'\') }
    [void]$builder.Append([char]'"')
    $builder.ToString()
}
function Invoke-CapturedCommand {
    param([string]$Path, [string[]]$Arguments, [string]$LogPath, [string]$Description)
    $canonicalPath = [IO.Path]::GetFullPath($Path)
    $canonicalLogPath = [IO.Path]::GetFullPath($LogPath)
    $stdoutLogPath = $canonicalLogPath + ".stdout.txt"
    $stderrLogPath = $canonicalLogPath + ".stderr.txt"
    $startInfo = [Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $canonicalPath
    $startInfo.Arguments = (@($Arguments | ForEach-Object { ConvertTo-WindowsProcessArgument ([string]$_) }) -join " ")
    $startInfo.WorkingDirectory = $RepositoryRoot
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    if (-not [string]::IsNullOrWhiteSpace($script:QtBin)) {
        $currentPath = [Environment]::GetEnvironmentVariable("PATH", "Process")
        $startInfo.EnvironmentVariables["PATH"] = "$script:QtBin;$currentPath"
    }
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    try {
        if (-not $process.Start()) { throw "$Description could not be started: $canonicalPath" }
        $stdoutTask = $process.StandardOutput.ReadToEndAsync()
        $stderrTask = $process.StandardError.ReadToEndAsync()
        $process.WaitForExit()
        $exitCode = [int]$process.ExitCode
        $stdout = [string]$stdoutTask.Result
        $stderr = [string]$stderrTask.Result
    } finally { $process.Dispose() }
    [IO.File]::WriteAllText($stdoutLogPath, $stdout, [Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText($stderrLogPath, $stderr, [Text.UTF8Encoding]::new($false))
    $commandLine = (@((ConvertTo-WindowsProcessArgument $canonicalPath)) + @($Arguments | ForEach-Object { ConvertTo-WindowsProcessArgument ([string]$_) })) -join " "
    $summaryLines = @(
        "command: $commandLine", "exitCode: $exitCode",
        "stdoutPath: $stdoutLogPath", "stdoutSha256: $(Get-FileSha256 $stdoutLogPath)",
        "stderrPath: $stderrLogPath", "stderrSha256: $(Get-FileSha256 $stderrLogPath)"
    )
    [IO.File]::WriteAllLines($canonicalLogPath, $summaryLines, [Text.UTF8Encoding]::new($false))
    [ordered]@{
        exitCode = $exitCode
        logPath = $canonicalLogPath; logSha256 = Get-FileSha256 $canonicalLogPath
        stdoutLogPath = $stdoutLogPath; stdoutLogSha256 = Get-FileSha256 $stdoutLogPath
        stderrLogPath = $stderrLogPath; stderrLogSha256 = Get-FileSha256 $stderrLogPath
    }
}
function Get-PeArchitecture {
    param([string]$Path)
    $stream = [IO.File]::OpenRead($Path)
    try {
        $reader = [IO.BinaryReader]::new($stream)
        if ($reader.ReadUInt16() -ne 0x5a4d) { throw "Bad MZ header: $Path" }
        $stream.Position = 0x3c; $offset = $reader.ReadInt32(); $stream.Position = $offset
        if ($reader.ReadUInt32() -ne 0x4550) { throw "Bad PE header: $Path" }
        switch ($reader.ReadUInt16()) { 0x8664 { "x64" } 0xaa64 { "arm64" } default { "unknown" } }
    } finally { $stream.Dispose() }
}

$receiptPath = Join-Path $EvidenceDirectory "release-build-receipt.json"
$sourceInventoryPath = Join-Path $EvidenceDirectory "source-files.json"
$failurePath = Join-Path $EvidenceDirectory "build-attempt.json"
$configureLogPath = Join-Path $EvidenceDirectory "release-configure.log"
$buildLogPath = Join-Path $EvidenceDirectory "release-build.log"
$source = $null
$configureResult = $null
$buildResult = $null
$cachePath = Join-Path $BuildDirectory "CMakeCache.txt"
$releaseExe = Join-Path $BuildDirectory "next-package-build\Release\ClassMngr.exe"

try {
    if ($env:OS -cne "Windows_NT") { throw "P9-10 release build inputs require local Windows." }
    $source = Get-SourceInventory $RepositoryRoot
    Write-JsonFile $sourceInventoryPath ([ordered]@{ files = @($source.files) })
    if (-not (Test-Path -LiteralPath $cachePath -PathType Leaf)) { throw "Existing x64 Release CMake cache is missing: $cachePath" }
    $cacheLines = @(Get-Content -LiteralPath $cachePath)
    if ([IO.Path]::GetFullPath((Get-CacheValue $cacheLines "CMAKE_HOME_DIRECTORY")) -ine $RepositoryRoot) { throw "Release CMake cache points at a different source tree." }
    if ((Get-CacheValue $cacheLines "CLASSMNGR_NEXT_RELEASE_PACKAGE") -cne "ON") { throw "Release CMake tree does not enable the current-source Next package." }
    if ((Get-CacheValue $cacheLines "CLASSMNGR_UPDATE_CHECK_ON_STARTUP") -cne "OFF") { throw "Release package startup update check must be disabled." }
    $generator = Get-CacheValue $cacheLines "CMAKE_GENERATOR"
    if ($generator -notmatch '^Visual Studio' -or (Get-CacheValue $cacheLines "CMAKE_GENERATOR_PLATFORM") -notin @("x64", "")) { throw "Release CMake tree is not a Windows x64 Visual Studio build." }
    $cmake = [IO.Path]::GetFullPath((Get-CacheValue $cacheLines "CMAKE_COMMAND"))
    $qt6Directory = [IO.Path]::GetFullPath((Get-CacheValue $cacheLines "Qt6_DIR"))
    $script:QtBin = Join-Path ([IO.Path]::GetFullPath((Join-Path $qt6Directory "..\..\.."))) "bin"
    $cacheSha256AtStart = Get-FileSha256 $cachePath
    $configureResult = Invoke-CapturedCommand $cmake @(
        "-S", $RepositoryRoot, "-B", $BuildDirectory,
        "-DCLASSMNGR_NEXT_RELEASE_PACKAGE=ON", "-DCLASSMNGR_UPDATE_CHECK_ON_STARTUP=OFF"
    ) $configureLogPath "Release CMake configure"
    if ([int]$configureResult.exitCode -ne 0) { throw "Release CMake configure failed with exit code $($configureResult.exitCode)." }
    $buildResult = Invoke-CapturedCommand $cmake @(
        "--build", $BuildDirectory, "--config", "Release", "--verbose",
        "--target", "ClassMngrNext", "ClassMngrNextResourcePackage", "ClassMngrPhase7NextBlankFixtureTests",
        "--", "/m:4", "/p:TrackFileAccess=false"
    ) $buildLogPath "Current-source packaged Next Release build"
    if ([int]$buildResult.exitCode -ne 0) { throw "Current-source packaged Next Release build failed with exit code $($buildResult.exitCode)." }
    if (-not (Test-Path -LiteralPath $releaseExe -PathType Leaf) -or (Get-PeArchitecture $releaseExe) -cne "x64") { throw "Current-source x64 Next Release executable is missing or has the wrong architecture." }
    $generatedRoot = Join-Path $BuildDirectory "generated\ClassMngrNext"
    $bakedAssetsRoot = Join-Path $generatedRoot "Resources\assets"
    $translationRoot = Join-Path $generatedRoot "assets\translations"
    $catalogPath = Join-Path $generatedRoot "resource-catalog.json"
    $fontLicensePath = Join-Path $RepositoryRoot "licenses\fonts\inter\LICENSE.txt"
    foreach ($requiredPath in @($bakedAssetsRoot, $translationRoot, $catalogPath, $fontLicensePath)) {
        if (-not (Test-Path -LiteralPath $requiredPath)) { throw "Release resource package output is missing: $requiredPath" }
    }
    $bakedAssets = Get-CanonicalInventory $bakedAssetsRoot
    $translations = Get-CanonicalInventory $translationRoot
    if ($bakedAssets.fileCount -le 0 -or $translations.fileCount -le 0) { throw "Release resource package output tree is empty." }
    $resourceStagingRoot = Join-Path $EvidenceDirectory "verified-resource-inputs"
    [void][IO.Directory]::CreateDirectory($resourceStagingRoot)
    $currentIdentity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $currentUserSid = [string]$currentIdentity.User.Value
    $stagingAcl = Get-Acl -LiteralPath $resourceStagingRoot
    $readRule = [Security.AccessControl.FileSystemAccessRule]::new(
        $currentIdentity.User,
        [Security.AccessControl.FileSystemRights]::ReadAndExecute,
        [Security.AccessControl.InheritanceFlags]::ContainerInherit -bor [Security.AccessControl.InheritanceFlags]::ObjectInherit,
        [Security.AccessControl.PropagationFlags]::None,
        [Security.AccessControl.AccessControlType]::Allow
    )
    $stagingAcl.SetAccessRule($readRule)
    Set-Acl -LiteralPath $resourceStagingRoot -AclObject $stagingAcl
    $stagedResourceRoot = Join-Path $resourceStagingRoot "resources"
    $stagedAssetsRoot = Join-Path $stagedResourceRoot "assets"
    $stagedTranslationRoot = Join-Path $stagedAssetsRoot "translations"
    $stagedLicensePath = Join-Path $resourceStagingRoot "licenses\fonts\inter\LICENSE.txt"
    [void][IO.Directory]::CreateDirectory($stagedAssetsRoot)
    [void][IO.Directory]::CreateDirectory($stagedTranslationRoot)
    [void][IO.Directory]::CreateDirectory((Split-Path -Parent $stagedLicensePath))
    foreach ($sourceInventory in @(@{ root = $bakedAssetsRoot; destination = $stagedAssetsRoot }, @{ root = $translationRoot; destination = $stagedTranslationRoot })) {
        $sourcePrefix = [IO.Path]::GetFullPath([string]$sourceInventory.root).TrimEnd([char[]]@('\', '/')) + [IO.Path]::DirectorySeparatorChar
        foreach ($sourceFile in @(Get-ChildItem -LiteralPath ([string]$sourceInventory.root) -File -Recurse)) {
            $relative = [IO.Path]::GetFullPath($sourceFile.FullName).Substring($sourcePrefix.Length)
            $destinationPath = Join-Path ([string]$sourceInventory.destination) $relative
            [void][IO.Directory]::CreateDirectory((Split-Path -Parent $destinationPath))
            Copy-Item -LiteralPath $sourceFile.FullName -Destination $destinationPath -Force
        }
    }
    Copy-Item -LiteralPath $catalogPath -Destination (Join-Path $stagedResourceRoot "resource-catalog.json") -Force
    Copy-Item -LiteralPath $fontLicensePath -Destination $stagedLicensePath -Force
    $stagedResourceInputs = Get-CanonicalInventory $resourceStagingRoot
    $expectedStagedRows = [Collections.Generic.List[object]]::new()
    foreach ($entry in $bakedAssets.files) { $expectedStagedRows.Add([ordered]@{ path = "resources/assets/$($entry.path)"; sizeBytes = [Int64]$entry.sizeBytes; sha256 = [string]$entry.sha256 }) }
    foreach ($entry in $translations.files) { $expectedStagedRows.Add([ordered]@{ path = "resources/assets/translations/$($entry.path)"; sizeBytes = [Int64]$entry.sizeBytes; sha256 = [string]$entry.sha256 }) }
    $expectedStagedRows.Add([ordered]@{ path = "resources/resource-catalog.json"; sizeBytes = [Int64](Get-Item -LiteralPath $catalogPath).Length; sha256 = Get-FileSha256 $catalogPath })
    $expectedStagedRows.Add([ordered]@{ path = "licenses/fonts/inter/LICENSE.txt"; sizeBytes = [Int64](Get-Item -LiteralPath $fontLicensePath).Length; sha256 = Get-FileSha256 $fontLicensePath })
    $expectedStagedRows.Sort([System.Comparison[object]]{ param($a, $b) [StringComparer]::Ordinal.Compare(([string]$a.path).ToLowerInvariant(), ([string]$b.path).ToLowerInvariant()) })
    $expectedRows = @($expectedStagedRows.ToArray())
    $expectedCanonicalRows = @($expectedRows | ForEach-Object { "{0}`t{1}`t{2}" -f $_.path, $_.sizeBytes, $_.sha256 }) -join "`n"
    if ($stagedResourceInputs.sha256 -cne (Get-StringSha256 ($expectedCanonicalRows + "`n")) -or $stagedResourceInputs.fileCount -ne $expectedRows.Count) { throw "Verified staged resource inputs do not match the generated Release resource outputs." }
    for ($index = 0; $index -lt $expectedRows.Count; $index++) {
        if ($stagedResourceInputs.files[$index].path -cne $expectedRows[$index].path -or [Int64]$stagedResourceInputs.files[$index].sizeBytes -ne [Int64]$expectedRows[$index].sizeBytes -or $stagedResourceInputs.files[$index].sha256 -cne $expectedRows[$index].sha256) { throw "Staged resource file differs from generated output at inventory row $index." }
    }
    $receipt = [ordered]@{
        schema = $Schema; taskId = $TaskId; status = "succeeded"
        runId = $RunId; createdAtUtc = [DateTimeOffset]::UtcNow.ToString("o")
        repositoryRoot = $RepositoryRoot; sourceSha256 = $source.sha256; sourceFileCount = $source.fileCount
        sourceInventoryPath = [IO.Path]::GetFullPath($sourceInventoryPath)
        sourceInventoryFileSha256 = Get-FileSha256 $sourceInventoryPath
        buildDirectory = $BuildDirectory; generator = $generator; configuration = "Release"; platform = "x64"
        packageOption = "CLASSMNGR_NEXT_RELEASE_PACKAGE=ON"; startupUpdateCheck = "OFF"
        cachePath = [IO.Path]::GetFullPath($cachePath); cacheSha256AtStart = $cacheSha256AtStart; cacheSha256After = Get-FileSha256 $cachePath
        releaseExecutablePath = [IO.Path]::GetFullPath($releaseExe); releaseExecutableSha256 = Get-FileSha256 $releaseExe; releaseExecutableArchitecture = "x64"
        configure = $configureResult; build = $buildResult
        targets = @("ClassMngrNext", "ClassMngrNextResourcePackage", "ClassMngrPhase7NextBlankFixtureTests")
        generatedResources = [ordered]@{
            bakedAssets = $bakedAssets
            translations = $translations
            catalogPath = [IO.Path]::GetFullPath($catalogPath); catalogSha256 = Get-FileSha256 $catalogPath; catalogSizeBytes = [Int64](Get-Item -LiteralPath $catalogPath).Length
            fontLicensePath = [IO.Path]::GetFullPath($fontLicensePath); fontLicenseSha256 = Get-FileSha256 $fontLicensePath; fontLicenseSizeBytes = [Int64](Get-Item -LiteralPath $fontLicensePath).Length
            verifiedStagingRoot = [IO.Path]::GetFullPath($resourceStagingRoot)
            stagedResourceInputs = $stagedResourceInputs
            currentUserSid = $currentUserSid
        }
    }
    Write-JsonFile $receiptPath $receipt
    $summary = [ordered]@{
        taskId = $TaskId; runId = $RunId; status = "succeeded"; sourceSha256 = $source.sha256
        receiptPath = [IO.Path]::GetFullPath($receiptPath); receiptSha256 = Get-FileSha256 $receiptPath
        releaseExecutablePath = [IO.Path]::GetFullPath($releaseExe); releaseExecutableSha256 = Get-FileSha256 $releaseExe
        configureExitCode = [int]$configureResult.exitCode; buildExitCode = [int]$buildResult.exitCode
    }
    Write-JsonFile $failurePath $summary
    Write-Output "P9-10 current-source Release build succeeded: $receiptPath"
} catch {
    $failure = [ordered]@{
        taskId = $TaskId; runId = $RunId; status = "failed"
        timestampUtc = [DateTimeOffset]::UtcNow.ToString("o"); message = $_.Exception.Message
        sourceSha256 = if ($null -ne $source) { $source.sha256 } else { $null }
        sourceInventoryPath = if (Test-Path -LiteralPath $sourceInventoryPath) { [IO.Path]::GetFullPath($sourceInventoryPath) } else { $null }
        configure = $configureResult; build = $buildResult
        configureLogPath = if (Test-Path -LiteralPath $configureLogPath) { [IO.Path]::GetFullPath($configureLogPath) } else { $null }
        buildLogPath = if (Test-Path -LiteralPath $buildLogPath) { [IO.Path]::GetFullPath($buildLogPath) } else { $null }
    }
    Write-JsonFile $failurePath $failure
    [Console]::Error.WriteLine("P9-10 Release build failed; evidence preserved at $EvidenceDirectory.")
    exit 1
}
