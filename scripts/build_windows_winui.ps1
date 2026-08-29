[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $ProjectFile,

    [Parameter(Mandatory = $true)]
    [string] $ProjectDirectory,

    [Parameter(Mandatory = $true)]
    [ValidateSet('Debug', 'Release')]
    [string] $Configuration,

    [Parameter(Mandatory = $true)]
    [ValidateSet('x64', 'Win32')]
    [string] $Platform,

    [Parameter(Mandatory = $true)]
    [string] $PackagesDirectory,

    [Parameter(Mandatory = $true)]
    [string] $NuGetConfigFile,

    [Parameter(Mandatory = $true)]
    [string] $OutputDirectory,

    [Parameter(Mandatory = $true)]
    [string] $IntermediateDirectory,

    [Parameter(Mandatory = $true)]
    [string] $EngineLibrary,

    [Parameter(Mandatory = $true)]
    [string] $EngineIncludeDirectory,

    [Parameter(Mandatory = $true)]
    [string] $GeneratedIncludeDirectory,

    [Parameter(Mandatory = $true)]
    [string] $ResourceFile,

    [Parameter(Mandatory = $true)]
    [string] $ResourceManifest,

    [Parameter(Mandatory = $true)]
    [string] $ProjectRoot,

    [Parameter(Mandatory = $true)]
    [string] $MinimumWindowsVersion,

    [Parameter(Mandatory = $true)]
    [string] $WindowsAppSdkVersion,

    [Parameter(Mandatory = $true)]
    [string] $WindowsAppSdkBaseVersion,

    [Parameter(Mandatory = $true)]
    [string] $WindowsAppSdkFoundationVersion,

    [Parameter(Mandatory = $true)]
    [string] $WindowsAppSdkInteractiveVersion,

    [Parameter(Mandatory = $true)]
    [string] $WindowsAppSdkWinUIVersion,

    [Parameter(Mandatory = $true)]
    [string] $WindowsAppSdkRuntimeVersion,

    [Parameter(Mandatory = $true)]
    [string] $WebView2Version,

    [Parameter(Mandatory = $true)]
    [string] $WindowsSdkMsixVersion,

    [Parameter(Mandatory = $true)]
    [string] $CppWinRTVersion,

    [Parameter(Mandatory = $true)]
    [string] $BuildToolsVersion
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Get-AbsolutePath {
    param([Parameter(Mandatory = $true)][string] $Path)

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }

    return [System.IO.Path]::GetFullPath(
        (Join-Path -Path (Get-Location).Path -ChildPath $Path)
    )
}

function Resolve-ExistingPath {
    param(
        [Parameter(Mandatory = $true)][string] $Path,
        [Parameter(Mandatory = $true)][string] $Description
    )

    $absolutePath = Get-AbsolutePath -Path $Path
    if (-not (Test-Path -LiteralPath $absolutePath)) {
        throw "$Description was not found: $absolutePath"
    }

    return $absolutePath
}

function Ensure-Directory {
    param([Parameter(Mandatory = $true)][string] $Path)

    if (-not (Test-Path -LiteralPath $Path)) {
        New-Item -ItemType Directory -Path $Path -Force | Out-Null
    }
}

function Resolve-CommandPath {
    param([Parameter(Mandatory = $true)][string[]] $Names)

    foreach ($name in $Names) {
        $command = Get-Command -Name $name -ErrorAction SilentlyContinue |
            Select-Object -First 1
        if ($null -ne $command) {
            if ($command.PSObject.Properties.Name -contains 'Path') {
                return $command.Path
            }
            return $command.Source
        }
    }

    return $null
}

function Resolve-MSBuildPath {
    $fromPath = Resolve-CommandPath -Names @('msbuild.exe', 'MSBuild')
    if ($null -ne $fromPath) {
        return $fromPath
    }

    $programFilesX86 = [Environment]::GetEnvironmentVariable('ProgramFiles(x86)')
    if ([string]::IsNullOrWhiteSpace($programFilesX86)) {
        $programFilesX86 = [Environment]::GetEnvironmentVariable('ProgramFiles')
    }
    if ([string]::IsNullOrWhiteSpace($programFilesX86)) {
        throw 'Could not locate Program Files to resolve Visual Studio.'
    }

    $vswhere = Join-Path $programFilesX86 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) {
        $installationPath = & $vswhere `
            -latest `
            -products * `
            -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
            -property installationPath |
            Select-Object -First 1

        if (-not [string]::IsNullOrWhiteSpace([string] $installationPath)) {
            $candidate = Join-Path ([string] $installationPath).Trim() 'MSBuild\Current\Bin\MSBuild.exe'
            if (Test-Path -LiteralPath $candidate) {
                return $candidate
            }
        }
    }

    throw 'MSBuild was not found. Open a Visual Studio Developer Command Prompt or install the C++ workload.'
}

function Assert-PackageFiles {
    param(
        [Parameter(Mandatory = $true)][string] $PackageId,
        [Parameter(Mandatory = $true)][string] $PackageVersion,
        [Parameter(Mandatory = $true)][string[]] $RelativeFiles
    )

    $packageRoot = Join-Path $PackagesDirectory "$PackageId.$PackageVersion"
    if (-not (Test-Path -LiteralPath $packageRoot)) {
        throw "Restored package is missing: $packageRoot"
    }

    foreach ($relativeFile in $RelativeFiles) {
        $filePath = Join-Path $packageRoot $relativeFile
        if (-not (Test-Path -LiteralPath $filePath)) {
            throw "Restored package file is missing: $filePath"
        }
    }
}

$projectFilePath = Resolve-ExistingPath -Path $ProjectFile -Description 'WinUI project file'
$projectDirectoryPath = Resolve-ExistingPath -Path $ProjectDirectory -Description 'WinUI project directory'
$nugetConfigPath = Resolve-ExistingPath -Path $NuGetConfigFile -Description 'NuGet configuration'
$engineLibraryPath = Resolve-ExistingPath -Path $EngineLibrary -Description 'ClassMngrEngine library'
$engineIncludePath = Resolve-ExistingPath -Path $EngineIncludeDirectory -Description 'ClassMngrEngine include directory'
$generatedIncludePath = Resolve-ExistingPath -Path $GeneratedIncludeDirectory -Description 'generated WinUI include directory'
$resourceFilePath = Resolve-ExistingPath -Path $ResourceFile -Description 'generated WinUI resource file'
$resourceManifestPath = Resolve-ExistingPath -Path $ResourceManifest -Description 'native resource manifest'
$projectRootPath = Resolve-ExistingPath -Path $ProjectRoot -Description 'project root'

$packagesPath = Get-AbsolutePath -Path $PackagesDirectory
$outputPath = Get-AbsolutePath -Path $OutputDirectory
$intermediatePath = Get-AbsolutePath -Path $IntermediateDirectory
$generatedFilesPath = Join-Path $intermediatePath 'generated'

Ensure-Directory -Path $packagesPath
Ensure-Directory -Path $outputPath
Ensure-Directory -Path $intermediatePath
Ensure-Directory -Path $generatedFilesPath

$nuget = Resolve-CommandPath -Names @('nuget.exe', 'nuget')
if ($null -eq $nuget) {
    throw 'nuget.exe was not found. Install NuGet or run the GitHub Actions setup-nuget step.'
}

$msbuild = Resolve-MSBuildPath

Write-Host "Restoring pinned WinUI packages into $packagesPath"
& $nuget restore $projectFilePath `
    -PackagesDirectory $packagesPath `
    -ConfigFile $nugetConfigPath `
    -DependencyVersion Lowest `
    -NonInteractive
if ($LASTEXITCODE -ne 0) {
    throw "NuGet restore failed with exit code $LASTEXITCODE."
}

Assert-PackageFiles `
    -PackageId 'Microsoft.WindowsAppSDK' `
    -PackageVersion $WindowsAppSdkVersion `
    -RelativeFiles @(
        'build\native\Microsoft.WindowsAppSDK.props',
        'build\native\Microsoft.WindowsAppSDK.targets'
    )
Assert-PackageFiles `
    -PackageId 'Microsoft.WindowsAppSDK.Base' `
    -PackageVersion $WindowsAppSdkBaseVersion `
    -RelativeFiles @(
        'build\native\Microsoft.WindowsAppSDK.Base.props',
        'build\native\Microsoft.WindowsAppSDK.Base.targets'
    )
Assert-PackageFiles `
    -PackageId 'Microsoft.WindowsAppSDK.Foundation' `
    -PackageVersion $WindowsAppSdkFoundationVersion `
    -RelativeFiles @(
        'build\native\Microsoft.WindowsAppSDK.Foundation.props',
        'build\native\Microsoft.WindowsAppSDK.Foundation.targets'
    )
Assert-PackageFiles `
    -PackageId 'Microsoft.WindowsAppSDK.InteractiveExperiences' `
    -PackageVersion $WindowsAppSdkInteractiveVersion `
    -RelativeFiles @(
        'build\native\Microsoft.WindowsAppSDK.InteractiveExperiences.props',
        'build\native\Microsoft.WindowsAppSDK.InteractiveExperiences.targets'
    )
Assert-PackageFiles `
    -PackageId 'Microsoft.WindowsAppSDK.WinUI' `
    -PackageVersion $WindowsAppSdkWinUIVersion `
    -RelativeFiles @(
        'build\native\Microsoft.WindowsAppSDK.WinUI.props',
        'build\native\Microsoft.WindowsAppSDK.WinUI.targets'
    )
Assert-PackageFiles `
    -PackageId 'Microsoft.WindowsAppSDK.Runtime' `
    -PackageVersion $WindowsAppSdkRuntimeVersion `
    -RelativeFiles @(
        'build\native\Microsoft.WindowsAppSDK.Runtime.props',
        'build\native\Microsoft.WindowsAppSDK.Runtime.targets'
    )
Assert-PackageFiles `
    -PackageId 'Microsoft.Web.WebView2' `
    -PackageVersion $WebView2Version `
    -RelativeFiles @(
        'build\native\Microsoft.Web.WebView2.targets'
    )
Assert-PackageFiles `
    -PackageId 'Microsoft.Windows.SDK.BuildTools.MSIX' `
    -PackageVersion $WindowsSdkMsixVersion `
    -RelativeFiles @(
        'build\Microsoft.Windows.SDK.BuildTools.MSIX.props',
        'build\Microsoft.Windows.SDK.BuildTools.MSIX.targets'
    )
Assert-PackageFiles `
    -PackageId 'Microsoft.Windows.CppWinRT' `
    -PackageVersion $CppWinRTVersion `
    -RelativeFiles @(
        'build\native\Microsoft.Windows.CppWinRT.props',
        'build\native\Microsoft.Windows.CppWinRT.targets'
    )
Assert-PackageFiles `
    -PackageId 'Microsoft.Windows.SDK.BuildTools' `
    -PackageVersion $BuildToolsVersion `
    -RelativeFiles @(
        'build\Microsoft.Windows.SDK.BuildTools.props',
        'build\Microsoft.Windows.SDK.BuildTools.targets'
    )

$outputProperty = "$outputPath\"
$intermediateProperty = "$intermediatePath\"
$generatedFilesProperty = "$generatedFilesPath\"

$msbuildArguments = @(
    $projectFilePath,
    '/t:Build',
    '/m',
    '/nologo',
    "/p:Configuration=$Configuration",
    "/p:Platform=$Platform",
    "/p:NugetPackageDirectory=$packagesPath",
    "/p:GeneratedFilesDir=$generatedFilesProperty",
    "/p:ClassMngrEngineLibrary=$engineLibraryPath",
    "/p:ClassMngrEngineIncludeDirectory=$engineIncludePath",
    "/p:ClassMngrWinUIGeneratedIncludeDirectory=$generatedIncludePath",
    "/p:ClassMngrWinUIResourceFile=$resourceFilePath",
    "/p:WindowsTargetPlatformMinVersion=$MinimumWindowsVersion",
    "/p:OutDir=$outputProperty",
    "/p:IntDir=$intermediateProperty",
    '/p:WindowsPackageType=None',
    '/p:WindowsAppSDKSelfContained=true',
    '/p:AppxPackage=false',
    '/p:WinUISDKReferences=false',
    '/p:UseCrtSDKReferenceStaticWarning=false',
    '/p:RestorePackages=false',
    '/p:PreferredToolArchitecture=x64'
)

Write-Host "Building ClassMngrWinUI ($Platform, $Configuration)"
Push-Location $projectDirectoryPath
try {
    & $msbuild @msbuildArguments
    if ($LASTEXITCODE -ne 0) {
        throw "MSBuild failed with exit code $LASTEXITCODE."
    }
}
finally {
    Pop-Location
}

$executablePath = Join-Path $outputPath 'ClassMngrWinUI.exe'
if (-not (Test-Path -LiteralPath $executablePath)) {
    throw "MSBuild completed but the staged executable was not found: $executablePath"
}

$resourceOutputDirectory = Join-Path $outputPath 'resources'
Ensure-Directory -Path $resourceOutputDirectory
Copy-Item -LiteralPath $resourceManifestPath `
    -Destination (Join-Path $resourceOutputDirectory 'manifest.json') `
    -Force

$licenseRelativePaths = @(
    'licenses\fonts\inter\LICENSE.txt',
    'licenses\fonts\pretendard\LICENSE.txt',
    'licenses\fonts\just-another-hand\LICENSE.txt'
)
foreach ($relativePath in $licenseRelativePaths) {
    $sourcePath = Join-Path $projectRootPath $relativePath
    $destinationPath = Join-Path $outputPath $relativePath
    $destinationDirectory = Split-Path -Parent $destinationPath
    Ensure-Directory -Path $destinationDirectory
    Copy-Item -LiteralPath $sourcePath -Destination $destinationPath -Force
}

Write-Host "WinUI stage ready: $outputPath"
